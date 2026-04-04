# `sylar/test/my_http_server` CPU 过高排查示例

## 1. 背景

为了说明 `CPU` 利用率过高时应该如何排查，我直接拿 `sylar` 里的测试服务器做实验，文件是 [test/my_http_server.cpp](/root/workspace/sylar/test/my_http_server.cpp#L1)。

这个服务非常简单：

- 监听 `0.0.0.0:8020`
- 注册路由 `/sylar/xx`
- 请求返回固定字符串 `OK\n`
- 使用 `sylar::IOManager iom(3)`，也就是 `3` 个调度线程

对应代码重点如下：

```cpp
sylar::http::HttpServer::ptr http_server(
    new sylar::http::HttpServer(true, sylar::IOManager::GetThis(), sylar::IOManager::GetThis(),
        false, nullptr, false, false));

sd->addServlet("/sylar/xx", [](sylar::http::HttpRequest::ptr req,
                               sylar::http::HttpResponse::ptr rsp,
                               sylar::http::HttpSession::ptr session) {
    rsp->setHeader("Content-Type", "text/plain; charset=utf-8");
    rsp->setBody("OK\n");
    return 0;
});

sylar::IOManager iom(3);
```

可以看到，这里几乎没有业务逻辑，所以如果压测时 `CPU` 仍然很高，说明瓶颈更可能在：

- 网络收发
- `HTTP` 解析与响应拼装
- 协程调度与线程切换
- 系统调用频率过高

## 2. 复现实验

实验日期：`2026-04-01`

实验机器：

- `4` 核 `x86_64`
- 内核：`Linux 5.15.0-173-generic`

### 2.1 启动服务

```bash
/root/workspace/sylar/bin/exe/my_http_server
```

### 2.2 压测命令

我使用 `wrk` 对 `/sylar/xx` 发起并发请求：

```bash
wrk -t4 -c400 -d15s http://127.0.0.1:8020/sylar/xx
```

### 2.3 CPU 采样命令

同时用 `pidstat` 观察进程和线程的 `CPU`：

```bash
pidstat -u -t -p <pid> 1 18
```

线程维度确认也可以用：

```bash
top -H -p <pid>
ps -L -p <pid> -o pid,tid,pcpu,stat,comm
```

## 3. 实测结果

### 3.1 压测结果

```text
Running 15s test @ http://127.0.0.1:8020/sylar/xx
  4 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.43ms    4.25ms  32.97ms   63.30%
    Req/Sec    15.82k     4.25k   28.18k    77.83%
  945451 requests in 15.10s, 114.51MB read
Requests/sec:  62623.99
Transfer/sec:      7.58MB
```

### 3.2 `pidstat` 结果

压测期间，`my_http_server` 的平均 `CPU` 如下：

```text
Average:        0    150925         -  111.78   92.94    0.00   15.17  204.72     -  my_http_server
Average:        0         -    150925   36.44   31.61    0.00   15.17   68.06     -  |__my_http_server
Average:        0         -    150926   36.61   29.22    0.00   17.50   65.83     -  |__Scheduler_0
Average:        0         -    150927   38.67   32.11    0.00   12.50   70.78     -  |__Scheduler_1
```

单秒峰值甚至到了：

```text
15:03:59 ... %CPU 255.00 my_http_server
```

## 4. 这些数字该怎么理解

### 4.1 `CPU = 200%` 不是 bug 的充分条件

这台机器是 `4` 核，所以单个进程的 `CPU` 超过 `100%` 很正常。

- `100%`：大约占满 `1` 个核
- `200%`：大约占满 `2` 个核
- `300%`：大约占满 `3` 个核

所以，这次的 `204.72%` 不表示程序异常，而是表示：

> 这个进程在压测期间，平均大约打满了 `2` 个 `CPU` 核。

### 4.2 这次不是“空转”，而是在忙

如果是死循环或空轮询，通常会看到：

- 没什么请求，`CPU` 还是高
- 某一个线程长期接近 `100%`
- 吞吐量上不去

但这次不同：

- `wrk` 实测 `6.26` 万 `QPS`
- `3` 个线程都在忙
- `user` 和 `system` `CPU` 都很高

这更像是：

> 服务在高并发请求下，确实把 `CPU` 花在了处理连接、解析请求、收发数据、调度协程上。

## 5. 为什么说这次要重点看 `system CPU`

`pidstat` 里最关键的一点不是只有总 `CPU`，而是：

- `%usr = 111.78`
- `%system = 92.94`

这说明什么？

- `user CPU` 高：用户态代码热点也不低
- `system CPU` 高：系统调用和内核网络栈开销非常明显

也就是说，这个案例不能只盯着“是不是业务代码太重”，还要看：

- `send/recv`
- `accept`
- `epoll`
- 锁与线程唤醒

## 6. syscall 侧的采样结果

为了继续确认问题方向，我又对这个进程做了一次 `strace -c` 采样。

命令：

```bash
strace -c -p <pid> -p <tid1> -p <tid2>
```

采样结果：

```text
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 70.96    6.635727          26    247345           sendto
 26.24    2.454384           9    250234      2817 recvfrom
  0.79    0.074287          14      4986           epoll_ctl
  0.76    0.070971          10      6745           rt_sigprocmask
  0.35    0.032592          11      2894           write
  0.23    0.021266          18      1149        86 futex
  0.20    0.018527          66       278           epoll_wait
```

这个结果非常有代表性：

- 大部分时间花在 `sendto` / `recvfrom`
- `epoll_ctl`、`epoll_wait` 也有开销
- `futex` 说明线程同步不是完全没有成本

结合 [test/my_http_server.cpp](/root/workspace/sylar/test/my_http_server.cpp#L24) 里几乎没有业务逻辑这一点，可以得到一个比较稳的判断：

> 这次 `CPU` 高的主要来源不是复杂业务计算，而是高并发短连接下的网络收发、`HTTP` 处理和调度开销。

## 7. 这个案例应该怎么排查

如果面试官问“`CPU` 利用率太高如何排查”，我建议按下面这个顺序说。

### 7.1 先确认是“有流量时高”，还是“没流量也高”

先判断症状类型：

- 没流量也高：优先怀疑空轮询、死循环、定时器太密、协程调度空转
- 压测时才高：优先怀疑网络收发、协议解析、锁竞争、频繁系统调用

在这个例子里，`my_http_server` 是压测时才高，所以我不会先怀疑死循环。

### 7.2 再看是进程高，还是线程高

用：

```bash
pidstat -u -t -p <pid> 1
top -H -p <pid>
```

这一步的目的不是只看“高不高”，而是看：

- 是不是一个线程打满
- 还是多个线程一起高
- `user` 高还是 `system` 高

在这次实验里，`3` 个线程都高，而且 `system` 不低。

### 7.3 然后决定下一步工具

如果看到：

- `user CPU` 很高：优先 `perf` / `gdb` / 火焰图，看热点函数
- `system CPU` 很高：优先 `strace -c` / 网络栈方向 / `epoll` / 锁竞争

这次更适合先从 `strace -c` 入手，因为 `system CPU` 已经接近 `93%`。

### 7.4 最后再回到代码解释

这次代码路径本身很短：

- 监听地址在 [test/my_http_server.cpp](/root/workspace/sylar/test/my_http_server.cpp#L12)
- 路由处理在 [test/my_http_server.cpp](/root/workspace/sylar/test/my_http_server.cpp#L24)
- 调度线程数在 [test/my_http_server.cpp](/root/workspace/sylar/test/my_http_server.cpp#L52)

因此可以反推出：

- 如果这里都能到 `200%+`
- 真正复杂的业务服务只会更容易把 `CPU` 打高

所以这个测试服务很适合作为“排查 CPU 利用率”的教学样例。

## 8. 这个案例里的结论

### 结论一

`my_http_server` 在 `wrk -t4 -c400 -d15s` 的压力下，`CPU` 平均可达到 `204.72%`，峰值约 `255%`。

### 结论二

这不是简单的“某个线程死循环”，而是 `3` 个线程一起工作，进程在高并发请求下真实消耗了多核 `CPU`。

### 结论三

由于这个测试接口本身几乎没有业务逻辑，因此这次 `CPU` 高更偏向：

- 网络收发
- `HTTP` 协议处理
- 协程/线程调度
- 系统调用频率高

### 结论四

如果以后线上服务也出现类似现象，最有效的排查路径就是：

1. `top` / `pidstat` 确认是哪个进程和线程高
2. 分清 `user CPU` 和 `system CPU`
3. `system` 高就先看 `strace -c`
4. `user` 高就继续用 `perf`、火焰图、`gdb` 看热点函数
5. 回到代码定位到底是空转、锁竞争，还是网络与协议处理成本高

## 9. 面试时可以怎么说

> 我在自己的 `sylar` 测试服务器上做过一次真实排查。  
> 当时我用 `wrk` 压测 `test/my_http_server.cpp` 里的 `/sylar/xx` 接口，进程 `CPU` 平均到了 `204%`，峰值大概 `255%`。  
> 我先用 `pidstat -u -t` 看线程级别 `CPU`，确认不是单线程死循环，而是 3 个调度线程都在忙；然后发现 `system CPU` 接近 `93%`，说明内核态开销不低。  
> 接着我用 `strace -c` 看 syscall 分布，发现主要时间花在 `sendto` 和 `recvfrom` 上。  
> 因为这个接口只返回一个固定字符串，所以我判断这次瓶颈不在业务逻辑，而在高并发下的网络收发、HTTP 处理和协程调度成本。  
> 这个过程让我形成了一个固定排查思路：先看进程，再看线程，再区分 user/system，最后选择 `strace` 或 `perf` 深挖。  

