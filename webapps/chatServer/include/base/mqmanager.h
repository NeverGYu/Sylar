#pragma once

#ifdef SYLAR_ENABLE_AI_MQ
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#endif

#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>

class MQManager {
public:
    static MQManager& instance() {
        static MQManager mgr;
        return mgr;
    }

    void publish(const std::string& queue, const std::string& msg);

private:
    struct MQConn {
#ifdef SYLAR_ENABLE_AI_MQ
        AmqpClient::Channel::ptr_t channel;
#endif
        std::mutex mtx;
    };

    MQManager(size_t poolSize = 5);

    MQManager(const MQManager&) = delete;
    MQManager& operator=(const MQManager&) = delete;

    std::vector<std::shared_ptr<MQConn>> pool_;
    size_t poolSize_;
    std::atomic<size_t> counter_;
};

class RabbitMQThreadPool {
public:
    using HandlerFunc = std::function<void(const std::string&)>;

    RabbitMQThreadPool(const std::string& host,
        const std::string& queue,
        int thread_num,
        HandlerFunc handler)
        : stop_(false),
        rabbitmq_host_(host),
        queue_name_(queue),
        thread_num_(thread_num),
        handler_(handler) {}

    void start();
    void shutdown();

    ~RabbitMQThreadPool() {
        shutdown();
    }

private:
    void worker(int id);

private:
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_;
    std::string queue_name_;
    int thread_num_;
    std::string rabbitmq_host_;
    HandlerFunc handler_;
};
