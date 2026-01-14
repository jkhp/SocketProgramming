#pragma once
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

#include "../Common.hpp"
#include "Session.hpp"
#include "Task.hpp"
#include <vector>
#include <array>
#include <thread>
#include <atomic>
#include <mswsock.h> // AcceptEx

class IocpServer
{
public:
    explicit IocpServer(int af, uint16_t port) // 암시적 변환 x, main에서 af, port 전달받음
        : af(af), port(port), listen_sock(INVALID_SOCKET), iocpHandle(nullptr), running(false) {};
    ~IocpServer() = default;

    void Start();
    void Stop();

protected:
    virtual void Run(); // IOCP 서버의 메인 루프

    // 세부 단계 함수들
    void InitSocket();
    void MakeWorkerThread();
    void BindAndListen();
    SOCKET AcceptClient(sockaddr_storage &caddr, socklen_t &clen);
    bool LoadAcceptEx();
    bool PostAccept();
    void HandleAcceptComplete(IoContext *ctx, DWORD cbTransferred, BOOL ok);
    void PrintClientInfo(const sockaddr_storage &caddr, socklen_t clen, bool type);
    void IocpStart(SOCKET client, const sockaddr_storage &caddr, socklen_t clen);

    DWORD WINAPI WorkerThread(void *arg); // void* == LPVOID, IocpStart에서 WSARecv()가 정상 호출된 후 WorkerThread에서 IOCP 작업 처리

    SOCKET GetListenSocket() const { return listen_sock; }
    bool IsRunning() const { return running.load(); }

private:
    int af;
    uint16_t port;
    SOCKET listen_sock;
    HANDLE iocpHandle;
    std::thread main_thread; // accept & iocp thread
    std::vector<std::thread> worker_threads;
    std::atomic<bool> running; // volatile -> atomic, 멀티스레드 환경에서 변수(공유자원)의 일관성을 보장하기 위해 사용(원자성 보장)

    SessionRegistry sessionRegistry; // 세션 관리 객체

    TaskQueue taskQueue_;
    TaskThread taskThread_{sessionRegistry, taskQueue_};

    LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
    LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = nullptr;

    static constexpr ULONG_PTR exitKey = 0xDEAD;
    static constexpr ULONG_PTR listenKey = 0;
};
