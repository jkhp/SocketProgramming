#pragma once

#include "Common.hpp"
#include <vector>
#include <thread>
#include <atomic>

struct SOCKETINFO
{
    OVERLAPPED overlapped; // 비동기 입출력 작업의 상태 정보를 저장하는 구조체
    WSABUF wsabuf;         // Windows 소켓 버퍼 구조체(길이, 포인터), WSASend 및 WSARecv 함수에서 사용, IOCP에서 각 클라이언트 송수신 버퍼 관리
    SOCKET sock;
    std::vector<char> buffer;
    int recvBytes;
    int sendBytes;
};

class IocpServer
{
public:
    explicit IocpServer(int af, uint16_t port) // 암시적 변환 x, main에서 af, port 전달받음
        : af(af), port(port), listen_sock(INVALID_SOCKET), running(false) {};
    ~IocpServer();

    void Start();
    void Stop();

protected:
    virtual void Run(); // IOCP 서버의 메인 루프

    // 세부 단계 함수들
    void InitSocket();
    void MakeWorkerThread();
    void BindAndListen();
    SOCKET AcceptClient(sockaddr_storage &caddr, socklen_t &clen);
    void PrintClientInfo(const sockaddr_storage &caddr, socklen_t clen);
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
};
