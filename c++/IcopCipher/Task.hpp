#pragma once

#include <cstdint> // std::uint32_t
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
    std::uint32_t fromSid{};
    std::uint32_t toSid{};
    MsgType msgType{MsgType::Chat};
    std::uint16_t reserved{0};
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
        std::shared_ptr<Session> from = registry_.Find(task.fromSid);
        if (!from || !from->IsAlive())
            return;

        switch (task.msgType)
        {
        case MsgType::ListReq:
        {
            std::string list;

            list += "=== Sessions ===\n";

            registry_.ForEach([&](const std::shared_ptr<Session> &s)
                              {
            if (!s) return;
            if (!s->IsAlive()) return;

            if (s->sessionId == task.fromSid) return;   // 본인은 제외

            list += std::to_string(s->sessionId);
            list += '\n'; });

            // task.msgType은 ListReq이므로 복사본에서 바꿔서 전송
            Task replyTask = task;
            replyTask.msgType = MsgType::ListResp;

            SendServerError(task.fromSid, replyTask, list);
            break;
        }
        case MsgType::Chat:
        {
            std::shared_ptr<Session> target = registry_.Find(task.toSid);
            if (!target || !target->IsAlive())
            {
                SendServerError(task.fromSid, task, "유효하지 않은 SessionId: " + std::to_string(task.toSid));
                return;
            }

            std::string msg(task.data.begin(), task.data.end());

            // 대상에게 전달
            if (msg.empty() || msg.back() != '\n')
                msg.push_back('\n');
            SendPacketToSession(task.toSid, task.fromSid, task, msg);

            std::cout << "[TaskThread] Routed FROM " << task.fromSid << " -> TO " << task.toSid
                      << " (" << msg.size() << " bytes)\n";

            break;
        }
        default:
        {
            SendServerError(task.fromSid, task, "유효하지 않은 작업");

            break;
        }
        }
    }

    void HandleDisconnect(const Task &task)
    {
        std::shared_ptr<Session> session = registry_.Find(task.fromSid);
        if (!session)
            return;

        session->Close();
        registry_.Remove(task.fromSid);

        std::cout << "[TaskThread] Disconnect sid=" << task.fromSid << std::endl;
    }

    void SendPacketToSession(std::uint32_t targetSid, std::uint32_t requestSid, const Task &task, const std::string &msg)
    {
        std::shared_ptr<Session> s = registry_.Find(targetSid);
        if (!s || !s->IsAlive())
            return;

        // Send용 IoContext는 완료까지 살아야 하므로 heap 생성
        IoContext *ctx = new IoContext();
        if (!ctx)
            return;

        ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));
        ctx->type = IoType::Send;
        ctx->sock = s->sock;
        ctx->sessionId = targetSid;

        std::vector<char> packet = BuildPacket(targetSid, requestSid, task.msgType, msg.data(), msg.size()); // header | payload
        ctx->sendBuf = std::make_shared<std::vector<char>>(std::move(packet));
        ctx->sendOffset = 0;

        ctx->wsabuf.buf = reinterpret_cast<char *>(ctx->sendBuf->data());
        ctx->wsabuf.len = static_cast<ULONG>(ctx->sendBuf->size());

        int r = WSASend(ctx->sock, &ctx->wsabuf, 1, nullptr, 0, &ctx->overlapped, nullptr);
        if (r == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
        {
            err_display("WSASend()");
            delete ctx;
            return;
        }
    }

    void SendServerError(std::uint32_t targetSid, const Task &task, const std::string &errMsg)
    {
        std::string out = errMsg + "\n";
        if (out.empty() || out.back() != '\n')
            out.push_back('\n');

        SendPacketToSession(targetSid, 0, task, out);
    }

private:
    SessionRegistry &registry_;
    TaskQueue &queue_;
    std::atomic<bool> running_;
    std::thread worker_;
};