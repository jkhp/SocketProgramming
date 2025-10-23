#pragma once
#include "Common.hpp"
#include <thread>
#include <atomic>

class TcpServer
{
public:
    TcpServer(int af, uint16_t port);
    ~TcpServer();

    void Start();
    void Stop();

protected:
    virtual void Run();

    // 세부 단계 함수들
    void InitSocket();
    void BindAndListen();
    SOCKET AcceptClient(sockaddr_storage &caddr, socklen_t &clen);
    void PrintClientInfo(const sockaddr_storage &caddr, socklen_t clen);
    void HandleClient(SOCKET client, const sockaddr_storage &caddr, socklen_t clen);

    SOCKET GetListenSocket() const { return listen_sock; }
    bool IsRunning() const { return running.load(); }

private:
    int af;
    uint16_t port;
    SOCKET listen_sock;
    std::atomic<bool> running; // volatile -> atomic, 멀티스레드 환경에서 변수(공유자원)의 일관성을 보장하기 위해 사용(원자성 보장)
    std::thread worker;
};