#pragma once

#include <cstdint> // std::uint64_t
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable> // std::condition_variable -> 스레드 간 작업 큐 동기화에 사용
#include <atomic>
#include <iostream>
#include "Session.hpp"
#include <thread>

enum class TaskType
{
    RecvMessage,
    Disconnect,
    // 차후 추가 가능
};

struct Task
{
    TaskType type{};
    std::uint64_t sessionId{};
    std::vector<char> data;
};

class TaskQueue
{
public:
    TaskQueue() = default;

    // workerThread에서 TaskThread로 작업 push
    void Push(Task task)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(task));
        }
        cv_.notify_one(); // 대기 중인 스레드 하나 깨우기
    }

    // TaskThread에서 Task pop(blocking)
    bool Pop(Task &out)
    {
        std::unique_lock<std::mutex> lock(mutex_); // 조건 변수와 함께(반복적으로 lock 획득/해제가 필요하므로),
        cv_.wait(lock, [this]()
                 { return !queue_.empty() || stop_; }); // 큐에 작업이 있거나 stop_이 true가 되면 lock 후 실행, false면 unlock 대기

        if (stop_ && queue_.empty())
            return false;

        out = std::move(queue_.front());
        queue_.pop();
        return true; // pop 성공
    }

    // 서버 종료 시, 모든 TaskThread를 깨워서 종료 처리
    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_; // 작업 큐에 대한 알림(깨우기)
    std::queue<Task> queue_;
    bool stop_{false};
};

class TaskThread
{
public:
    TaskThread(SessionRegistry &registry, TaskQueue &queue)
        : registry_(registry), queue_(queue), running_(false) {}

    ~TaskThread()
    {
        Stop();
    }

    void Start()
    {
        running_.store(true);
        worker_ = std::thread(&TaskThread::Run, this);
    }

    void Stop()
    {
        if (!running_.exchange(false))
            return;

        // queue에 대기 중인 스레드를 깨우기 위해 Stop 호출
        queue_.Stop();

        if (worker_.joinable()) // 스레드가 실행 중인지 확인
            worker_.join();
    }

private:
    void Run()
    {
        Task task;

        while (running_.load())
        {
            if (!queue_.Pop(task)) // bool Pop(Task &out)
            {
                break;
            }

            switch (task.type)
            {
            case TaskType::RecvMessage:
                HandleRecvMessage(task);
                break;
            case TaskType::Disconnect:
                HandleDisconnect(task);
                break;
            default:
                std::cerr << "존재하지 않는 TaskType 처리" << std::endl;
                break;
            }
        }
    }

    void HandleRecvMessage(const Task &task)
    {
        std::shared_ptr<Session> session = registry_.Find(task.sessionId);
        if (!session || !session->IsAlive())
        {
            return;
        }

        std::cout << "[TaskThread] RecvMessage, sid = " << task.sessionId << ",size = " << task.data.size() << std::endl;

        // AES_HMAC, DB, 수신 작업, Task를 workerThead에 다시 전달 등
    }

    void HandleDisconnect(const Task &task)
    {
        std::shared_ptr<Session> session = registry_.Find(task.sessionId);
        if (!session)
            return;

        session->Close();
        registry_.Remove(task.sessionId);

        std::cout << "[TaskThread] Disconnect sid=" << task.sessionId << std::endl;
    }

private:
    SessionRegistry &registry_;
    TaskQueue &queue_;
    std::atomic<bool> running_;
    std::thread worker_;
};