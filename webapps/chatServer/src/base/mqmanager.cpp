#include "../../include/base/mqmanager.h"

#ifdef SYLAR_ENABLE_AI_MQ

MQManager::MQManager(size_t poolSize)
    : poolSize_(poolSize), counter_(0) {
    for (size_t i = 0; i < poolSize_; ++i) {
        auto conn = std::make_shared<MQConn>();
        conn->channel = AmqpClient::Channel::Create("localhost", 5672, "guest", "guest", "/");
        pool_.push_back(conn);
    }
}

void MQManager::publish(const std::string& queue, const std::string& msg) {
    size_t index = counter_.fetch_add(1) % poolSize_;
    auto& conn = pool_[index];

    std::lock_guard<std::mutex> lock(conn->mtx);
    auto message = AmqpClient::BasicMessage::Create(msg);
    conn->channel->BasicPublish("", queue, message);
}

void RabbitMQThreadPool::start() {
    for (int i = 0; i < thread_num_; ++i) {
        workers_.emplace_back(&RabbitMQThreadPool::worker, this, i);
    }
}

void RabbitMQThreadPool::shutdown() {
    stop_ = true;
    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void RabbitMQThreadPool::worker(int id) {
    try {
        auto channel = AmqpClient::Channel::Create(rabbitmq_host_, 5672, "guest", "guest", "/");
        channel->DeclareQueue(queue_name_, false, true, false, false);
        std::string consumer_tag = channel->BasicConsume(queue_name_, "", true, false, false);

        channel->BasicQos(1);

        while (!stop_) {
            AmqpClient::Envelope::ptr_t env;
            bool ok = channel->BasicConsumeMessage(consumer_tag, env, 500);
            if (ok && env) {
                std::string msg = env->Message()->Body();
                handler_(msg);
                channel->BasicAck(env);
            }
        }

        channel->BasicCancel(consumer_tag);
    }
    catch (const std::exception& e) {
        std::cerr << "Thread " << id << " exception: " << e.what() << std::endl;
    }
}

#else

MQManager::MQManager(size_t poolSize)
    : poolSize_(poolSize), counter_(0) {
}

void MQManager::publish(const std::string& queue, const std::string& msg) {
    (void)queue;
    (void)msg;
    throw std::runtime_error("chat_server was built without RabbitMQ support");
}

void RabbitMQThreadPool::start() {
}

void RabbitMQThreadPool::shutdown() {
    stop_ = true;
    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void RabbitMQThreadPool::worker(int id) {
    (void)id;
}

#endif
