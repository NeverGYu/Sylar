# `sylar/test/my_http_server` 的 `perf` 热点分析报告

## 1. 目的

前一份文档已经说明了：

- `my_http_server` 在压测下 `CPU` 可以稳定到 `200%+`
- `pidstat` 能确认是多个线程一起忙
- `strace -c` 能确认系统调用开销很高

但这还不够，因为面试官很可能继续追问：

> 你是怎么进一步定位到热点函数的？  
> 你有没有用 `perf`？

所以这次我直接对 [my_http_server.cpp](/root/workspace/sylar/test/my_http_server.cpp#L1) 做了一次真实的 `perf record` + `perf report`，目标是回答两个问题：

1. 热点主要集中在哪些函数
2. 这些热点到底落在 `sylar` 的哪条代码路径上

## 2. 实验环境

实验日期：`2026-04-01`

机器环境：

- `4` 核 `x86_64`
- 内核：`5.15.0-173-generic`
- `perf` 版本：`5.15.198`

服务程序：

- 可执行文件：[my_http_server](/root/workspace/sylar/bin/exe/my_http_server)
- 源码文件：[my_http_server.cpp](/root/workspace/sylar/test/my_http_server.cpp#L1)

这个测试服务的特点很简单：

- 路由只有 `/sylar/xx`
- 处理逻辑只是返回 `OK\n`
- 调度线程数为 `3`

所以如果它在高并发下 `CPU` 很高，那么大概率不是复杂业务逻辑的问题，而是：

- 网络收发
- `HTTP` 协议解析
- 响应序列化
- 协程切换
- 内核网络栈

## 3. 准备工作

### 3.1 安装 `perf`

我在这台机器上实际安装了：

```bash
apt-get update
apt-get install -y linux-tools-common linux-tools-generic linux-tools-5.15.0-173-generic
```

### 3.2 调整采样权限

为了让 `perf` 可以正常采样，我把内核参数调成：

```bash
sysctl -w kernel.perf_event_paranoid=1
sysctl -w kernel.kptr_restrict=0
```

## 4. 复现步骤

### 4.1 启动服务

```bash
/root/workspace/sylar/bin/exe/my_http_server
```

### 4.2 压测命令

```bash
wrk -t4 -c400 -d15s http://127.0.0.1:8020/sylar/xx
```

这次压测结果如下：

```text
Running 15s test @ http://127.0.0.1:8020/sylar/xx
  4 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.76ms    4.64ms  48.26ms   67.63%
    Req/Sec    15.22k     4.25k   26.90k    75.50%
  909109 requests in 15.07s, 110.11MB read
Requests/sec:  60311.38
Transfer/sec:      7.30MB
```

### 4.3 `perf` 采样命令

我在压测同时执行了：

```bash
perf record -F 199 -g --call-graph dwarf,16384 -p <pid> -o /tmp/my_http_server.perf.data -- sleep 15
```

采样完成后，导出了三份文本报告：

```bash
perf report --stdio -i /tmp/my_http_server.perf.data --sort comm,dso,symbol --percent-limit 0.5 \
  > /tmp/my_http_server.perf.report.txt

perf report --stdio -i /tmp/my_http_server.perf.data -g graph,0.5,caller --percent-limit 1.0 --sort symbol \
  > /tmp/my_http_server.perf.callgraph.txt

perf report --stdio --no-children -i /tmp/my_http_server.perf.data --sort symbol --percent-limit 0.5 \
  > /tmp/my_http_server.perf.self.txt
```

这几个文件都已经生成：

- [/tmp/my_http_server.perf.data](/tmp/my_http_server.perf.data)
- [/tmp/my_http_server.perf.report.txt](/tmp/my_http_server.perf.report.txt)
- [/tmp/my_http_server.perf.callgraph.txt](/tmp/my_http_server.perf.callgraph.txt)
- [/tmp/my_http_server.perf.self.txt](/tmp/my_http_server.perf.self.txt)

## 5. `perf` 看到了什么

## 5.1 最大的调用路径热点

从 `call graph` 看，最重要的一条链路是：

```text
std::function<void ()>::operator()
  -> sylar::http::HttpServer::handleClient
    -> sylar::http::HttpSession::sendResponse            36.17%
      -> sylar::Stream::writeFixSize                     27.43%
        -> sylar::Socket::send                           27.17%
          -> send / __x64_sys_sendto / tcp_sendmsg ...

    -> sylar::http::HttpSession::recvRequest             32.34%
      -> sylar::Socket::recv                             15.59%
        -> recv / __x64_sys_recvfrom / tcp_recvmsg ...

    -> sylar::http::HttpResponse::dump                    5.59%
```

这个结果非常关键，因为它说明：

- 热点主线就是 `handleClient`
- 发送响应比接收请求更热
- `send` 这条链已经一路打进了内核 `tcp_sendmsg`
- 接收请求的成本也不低
- 响应序列化本身也占到了一部分

## 5.2 平铺视角下的热点函数

从 `self` 热点看，比较值得关注的是：

```text
5.88%  __lock_text_start
2.64%  http_parser_execute
1.62%  syscall_enter_from_user_mode
1.50%  srso_alias_safe_ret
1.49%  std::__cxx11::basic_string<...>::_M_data
1.10%  malloc
1.07%  __swapcontext
1.05%  std::_Sp_counted_base<...>::_M_release
1.00%  _int_free
```

这里最有信息量的几个点是：

- `http_parser_execute` 明确上榜
- `__swapcontext` 确实存在，但占比不高
- `malloc/free` 和 `shared_ptr` 释放也有开销
- `__lock_text_start` 很高，说明网络唤醒和锁路径成本不小

## 6. 回到 `sylar` 源码怎么看

### 6.1 请求主路径是 `handleClient`

`perf` 最热的上层函数落在 [http_server.cpp](/root/workspace/sylar/src/http/http_server.cpp#L114) 和 [http_server.cpp](/root/workspace/sylar/src/http/http_server.cpp#L121)：

```cpp
m_dispatch->handle(req, rsp, session);
session->sendResponse(rsp);
```

这说明一次请求最主要的工作都在这里汇聚：

- 调用 `Servlet`
- 组装响应
- 发送响应

### 6.2 发送响应为什么热

发送响应在 [http_session.cpp](/root/workspace/sylar/src/http/http_session.cpp#L80)：

```cpp
int HttpSession::sendResponse(HttpResponse::ptr rsp) 
{
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    return m_stream->writeFixSize(data.c_str(), data.size());
}
```

这里至少有三层成本：

1. `stringstream` 序列化
2. `std::string` 构造与拷贝
3. `writeFixSize -> send`

所以 `perf` 看到：

- `sylar::http::HttpSession::sendResponse`
- `sylar::http::HttpResponse::dump`
- `sylar::Socket::send`

是非常合理的。

### 6.3 接收请求为什么热

接收请求在 [http_session.cpp](/root/workspace/sylar/src/http/http_session.cpp#L15)：

```cpp
HttpRequest::ptr HttpSession::recvRequest()
```

核心成本来自：

- [http_session.cpp](/root/workspace/sylar/src/http/http_session.cpp#L40) 的 `parser->execute(...)`
- [http_session.cpp](/root/workspace/sylar/src/http/http_session.cpp#L67) 的 `m_stream->read(...)`

这正好对应了 `perf` 里的：

- `http_parser_execute`
- `sylar::Socket::recv`
- `__x64_sys_recvfrom`

### 6.4 协程切换是不是罪魁祸首

很多人看到协程框架，第一反应是“是不是切协程太重”。

这次 `perf` 给出的答案是：

> 协程切换有成本，但不是主瓶颈。

因为在 `self` 热点里，`__swapcontext` 只有约 `1.07%`，对应路径会回到 `Fiber::resume/yield`。也就是说：

- 协程调度不是零成本
- 但当前场景下，它不是最主要的热点
- 更大的热点还是网络收发与协议处理

## 7. 这次 `perf` 报告能得出什么结论

### 结论一

热点主路径集中在：

- `sylar::http::HttpServer::handleClient`
- `sylar::http::HttpSession::sendResponse`
- `sylar::http::HttpSession::recvRequest`

### 结论二

发送路径比接收路径更热。

从 `call graph` 上看：

- `sendResponse` 约 `36.17%`
- `recvRequest` 约 `32.34%`

说明这个简单接口的瓶颈更偏向：

- 响应写回
- 内核发送路径
- TCP 栈处理

### 结论三

`HTTP` 解析和响应序列化都是显式成本。

`perf` 已经明确把：

- `http_parser_execute`
- `sylar::http::HttpResponse::dump`

都打成了热点。

### 结论四

协程切换不是主瓶颈。

`__swapcontext` 虽然存在，但远没有 `send/recv`、协议处理和锁路径那么热。

### 结论五

这次 `CPU 200%+` 的主要成分不是“业务代码太复杂”，而是：

- 高并发网络收发
- `HTTP` 协议解析
- 响应序列化
- 内核网络栈与唤醒锁路径

## 8. 如果要继续优化，可以怎么做

如果只是为了排查，到这里已经足够了。

如果要继续做性能优化，我会优先考虑这几个方向：

### 8.1 降低响应序列化成本

[http_session.cpp](/root/workspace/sylar/src/http/http_session.cpp#L82) 现在每次都：

- 建 `stringstream`
- 把 `HttpResponse` 写进去
- 再构造一份 `std::string`

这条路径很容易产生：

- 小对象分配
- 字符串扩容
- 拷贝

### 8.2 减少短连接压力

这个测试接口在压测时，大量时间消耗在 `sendto/recvfrom/tcp_sendmsg/tcp_recvmsg` 上，说明短连接成本非常明显。

如果场景允许：

- 开启更稳定的 `keep-alive`
- 降低连接建立/销毁频率

会比只优化业务代码更有效。

### 8.3 再用 `perf annotate` 或火焰图继续深挖

如果想继续往下追，可以做两步：

1. 重新用 `Debug` 方式编译，带更完整的调试信息
2. 再用：

```bash
perf annotate
perf script
```

或者生成火焰图，继续细看：

- `HttpResponse::dump`
- `http_parser_execute`
- `Socket::send/recv`

## 9. 面试时可以怎么回答

> 我不只会看 `top` 和 `pidstat`，还实际用 `perf` 跑过一次分析。  
> 我拿 `sylar/test/my_http_server` 做例子，在 `wrk -t4 -c400 -d15s` 的压力下，先确认进程 `CPU` 能到 `200%+`，然后用 `perf record -g -p <pid>` 抓样本，再用 `perf report --stdio` 看热点。  
> 结果显示热点主线集中在 `sylar::http::HttpServer::handleClient`，其中 `sendResponse` 比 `recvRequest` 更热，`send` 最终一路打到了内核的 `tcp_sendmsg`，同时 `http_parser_execute` 和 `HttpResponse::dump` 也都是明显热点。  
> 这说明瓶颈不是简单的协程切换，`__swapcontext` 只占很小一部分，真正的开销主要是网络收发、HTTP 解析、响应序列化和内核网络栈。  
> 所以我排查高 CPU 的思路不是猜，而是先用 `pidstat` 看线程，再用 `perf` 找热点函数，再回到源码解释这条热点路径。  

