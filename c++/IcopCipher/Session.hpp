#pragma once

#include "../Common.hpp"
#include <atomic>
#include <cstdint> // std::uint64_t
#include <cstring> // std::memcpy
#include <unordered_map>
#include <memory> // std::shared_ptr, std::enable_shared_from_this
#include <mutex>

struct Session : public std::enable_shared_from_this<Session> // 자기 자신을 다른 곳에 전달 할 때 shared_ptr로 안전하게 전달하기 위해 사용
{
    std::uint64_t sessionId{};
    SOCKET sock{INVALID_SOCKET};

    sockaddr_storage remoteAddr{};
    int remoteAddrLen{sizeof(remoteAddr)};

    std::atomic<bool> alive{false};

    Session(std::uint64_t sid, SOCKET s, const sockaddr_storage &addr, int addrLen)
        : sessionId(sid), sock(s), remoteAddrLen(addrLen), alive(true)
    {
        std::memcpy(&remoteAddr, &addr, static_cast<size_t>(addrLen)); // remoteaddr에 복사
    }

    ~Session()
    {
        Close(); // Session 소멸시 소켓 닫기
    }

    void Close()
    {
        bool expected = true;
        if (alive.compare_exchange_strong(expected, false)) // alive가 true(expected)일 때만 false로 변경, 성공한 경우 true 반환 후 아래 실행(소켓 닫기)
        {
            if (sock != INVALID_SOCKET)
            {
                ::closesocket(sock);
                sock = INVALID_SOCKET;
            }
        }
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

        std::uint64_t sid = ++lastId_;
        std::shared_ptr<Session> session = std::make_shared<Session>(sid, sock, addr, addrLen);
        sessions_.emplace(sid, session);
        return session;
    }

    std::shared_ptr<Session> Find(std::uint64_t sid)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_map<std::uint64_t, std::shared_ptr<Session>>::iterator it = sessions_.find(sid); // iterator -> 맵의 항목을 가르키는 포인터 역할
        if (it == sessions_.end())                                                                      // 못 찾음
            return nullptr;
        return it->second; // first: key, second: value(shared_ptr<Session>)
    }

    void Remove(std::uint64_t sid)
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
    std::unordered_map<std::uint64_t, std::shared_ptr<Session>> sessions_;
    std::uint64_t lastId_{0};
};
