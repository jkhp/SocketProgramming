#pragma once

#include "../Common.hpp"
#include <atomic>
#include <cstdint> // std::uint32_t
#include <cstring> // std::memcpy
#include <unordered_map>
#include <memory> // std::shared_ptr, std::enable_shared_from_this
#include <mutex>
#include <array>
#include <vector>

#define BUFSIZE 1024

enum class IoType : uint8_t
{
    Accept,
    Recv,
    Send
};

struct IoContext
{
    OVERLAPPED overlapped{};
    IoType type{IoType::Recv};

    SOCKET sock{INVALID_SOCKET};
    std::uint32_t sessionId{0};

    WSABUF wsabuf{};
    std::array<char, BUFSIZE> recvBuf{};

    std::array<char, (sizeof(sockaddr_storage) + 16) * 2> acceptBuf{}; // acceptex 버퍼

    std::shared_ptr<std::vector<char>> sendBuf;
    size_t sendOffset{0}; // 부분 송신 시 사용

    std::vector<char> streamBuf; // tcp stream 누적 버퍼

    static IoContext *CreateRecv(SOCKET s, std::uint32_t sid)
    {
        IoContext *ctx = new IoContext();
        if (!ctx)
            return nullptr;

        ::ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));
        ctx->type = IoType::Recv;
        ctx->sock = s;
        ctx->sessionId = sid;

        ctx->wsabuf.buf = ctx->recvBuf.data();
        ctx->wsabuf.len = BUFSIZE;
        return ctx;
    }

    static IoContext *CreateSend(SOCKET s, std::uint32_t sid, std::shared_ptr<std::vector<char>> buf, size_t offset = 0)
    {
        IoContext *ctx = new IoContext();
        if (!ctx)
            return nullptr;

        ::ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));
        ctx->type = IoType::Send;
        ctx->sock = s;
        ctx->sessionId = sid;

        ctx->sendBuf = std::move(buf);
        ctx->sendOffset = offset;

        const size_t remain = (ctx->sendBuf && ctx->sendBuf->size() > ctx->sendOffset)
                                  ? (ctx->sendBuf->size() - ctx->sendOffset)
                                  : 0;

        ctx->wsabuf.buf = (remain > 0) ? reinterpret_cast<char *>(ctx->sendBuf->data() + ctx->sendOffset) : nullptr;
        ctx->wsabuf.len = static_cast<ULONG>(remain);
        return ctx;
    }

    static IoContext *CreateAccept(int af)
    {
        IoContext *ctx = new IoContext();
        if (!ctx)
            return nullptr;

        ::ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));
        ctx->type = IoType::Accept;
        ctx->sock = ::WSASocket(af, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (ctx->sock == INVALID_SOCKET)
        {
            delete ctx;
            return nullptr;
        }
        return ctx;
    }
};

struct Session : public std::enable_shared_from_this<Session> // 자기 자신을 다른 곳에 전달 할 때 shared_ptr로 안전하게 전달하기 위해 사용
{
    std::uint32_t sessionId{};
    SOCKET sock{INVALID_SOCKET};

    sockaddr_storage remoteAddr{};
    int remoteAddrLen{sizeof(remoteAddr)};

    std::atomic<bool> alive{false};

    Session(std::uint32_t sid, SOCKET s, const sockaddr_storage &addr, int addrLen)
        : sessionId(sid), sock(s), remoteAddrLen(addrLen), alive(true)
    {
        std::memcpy(&remoteAddr, &addr, static_cast<size_t>(addrLen)); // remoteaddr에 복사
    }

    ~Session()
    {
        (void)Close(); // Session 소멸시 소켓 닫기
    }

    bool Close()
    {
        bool expected = true;
        if (!alive.compare_exchange_strong(expected, false)) // compare_exchange_strong: alive와 expected의 값이 같으면 alive를 false로 변경하고 true 반환, 다르면 expected에 alive값을 복사하고 false 반환
            return false;                                    // 이미 Close()를 호출한 상태

        if (sock != INVALID_SOCKET)
        {
            ::shutdown(sock, SD_BOTH); //
            ::closesocket(sock);
            sock = INVALID_SOCKET;
        }
        return true;
    }

    bool IsAlive() const noexcept // noexcept: 예외를 던지지 않음을 컴파일러에 알림
    {
        return alive.load(std::memory_order_acquire); // std::memory_order_acquire -> 멀티스레드 환경에서 변수 일관성 보장, 최적화 문제 방지
    }
};

class SessionRegistry // 세션 관리 클래스
{
public:
    SessionRegistry() = default; // 기본 생성자

    std::shared_ptr<Session> Add(SOCKET sock, const sockaddr_storage &addr, int addrLen)
    {
        std::lock_guard<std::mutex> lock(mutex_); // 기존 mutex 방식보다 안전함, 스코프를 벗어나면 자동으로 unlock

        std::uint32_t sid = ++lastId_;
        std::shared_ptr<Session> session = std::make_shared<Session>(sid, sock, addr, addrLen);
        sessions_.emplace(sid, session);
        return session;
    }

    std::shared_ptr<Session> Find(std::uint32_t sid)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_map<std::uint32_t, std::shared_ptr<Session>>::iterator it = sessions_.find(sid); // iterator -> 맵의 항목을 가르키는 포인터 역할
        if (it == sessions_.end())                                                                      // 못 찾음
            return nullptr;
        return it->second; // first: key, second: value(shared_ptr<Session>)
    }

    void Remove(std::uint32_t sid)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(sid);
    }

    template <typename F> // std::function과 달리 템플릿이 더 효율적이고 간접호출 오버헤드, 타입 제약 없음
    void ForEach(F &&f)   // map의 모든 Session에 대해 f 함수 실행
                          // Forwarding Reference(완벽 전달용 참조) -> 하나의 템플릿으로 lvalue, rvalue 모두 처리 가능?
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &[sessionId, session] : sessions_)
        {
            f(session);
        }
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::uint32_t, std::shared_ptr<Session>> sessions_;
    std::uint32_t lastId_{0};
};
