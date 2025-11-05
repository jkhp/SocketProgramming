#include "IocpServer.hpp"
#include <iostream>

void IocpServer::Start()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        err_quit("WSAStartup()");

    running = true;
    main_thread = std::thread(&IocpServer::Run, this);
}

void IocpServer::Stop()
{
    if (running.exchange(false))
    {
        if (listen_sock != INVALID_SOCKET)
        {
            closesocket(listen_sock);
            listen_sock = INVALID_SOCKET;
        }

        // 대기 중인 워커를 깨우기 위해 웨이크업 패킷 전송, 그렇지 않으면 GetQueuedCompletionStatus에서 영원히 대기
        for (size_t i = 0; i < worker_threads.size(); ++i)
        {
            PostQueuedCompletionStatus(iocpHandle, 0, 0, nullptr); // 모든 워커 스레드에 nullptr 전달, 스레드 내부에서 종료 처리, [check]
        }

        if (main_thread.joinable())
            main_thread.join();

        for (auto &t : worker_threads)
        {
            if (t.joinable())
                t.join();
        }

        // Run() IOCP 핸들 정리
        if (iocpHandle)
        {
            CloseHandle(iocpHandle);
            iocpHandle = nullptr;
        }

        WSACleanup();
    }
}

void IocpServer::Run()
{
    iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0); // IOCP 핸들 생성
    if (iocpHandle == nullptr)
        err_quit("CreateIoCompletionPort()");

    MakeWorkerThread();

    InitSocket();
    BindAndListen();

    while (IsRunning())
    {
        sockaddr_storage caddr{};
        socklen_t clen{};

        clen = sizeof(caddr); // AcceptClient 호출 전 초기화

        SOCKET client_sock = AcceptClient(caddr, clen);
        if (client_sock == INVALID_SOCKET)
            continue;

        PrintClientInfo(caddr, clen);
        IocpStart(client_sock, caddr, clen);
    }

    if (listen_sock != INVALID_SOCKET)
    {
        closesocket(listen_sock);
        listen_sock = INVALID_SOCKET;
    }
}

void IocpServer::MakeWorkerThread()
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    // CPU *2 개수만큼 워커 스레드 생성
    for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors * 2; ++i)
    {
        worker_threads.emplace_back(&IocpServer::WorkerThread, this, iocpHandle); // puch_back과 달리 emplace_back은 생성자를 직접 호출, 이동생성자 반복호출 x
    }
}

void IocpServer::InitSocket()
{
    listen_sock = socket(af, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET)
        err_quit("socket()");
}

void IocpServer::BindAndListen()
{
    if (af == AF_INET)
    {
        sockaddr_in addr{}; // 전체를 0으로 초기화
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(listen_sock, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
            err_quit("bind()");
    }
    else if (af == AF_INET6)
    {
        sockaddr_in6 addr6{};
        addr6.sin6_family = AF_INET6;
        addr6.sin6_addr = in6addr_any;
        addr6.sin6_port = htons(port);

        if (bind(listen_sock, (sockaddr *)&addr6, sizeof(addr6)) == SOCKET_ERROR)
            err_quit("bind()");
    }

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR)
        err_quit("listen()");

    std::cout << "[TCP SERVER] PORT: " << port << ", " << (af == AF_INET ? "IPv4" : "IPv6") << " START" << std::endl;
}

SOCKET IocpServer::AcceptClient(sockaddr_storage &caddr, socklen_t &clen)
{
    SOCKET client_sock = accept(listen_sock, (sockaddr *)&caddr, &clen);
    if (client_sock == INVALID_SOCKET)
        err_display("accept()");
    return client_sock;
}

void IocpServer::PrintClientInfo(const sockaddr_storage &caddr, socklen_t clen)
{
    std::array<char, NI_MAXHOST> hostIP{}; // [check] 나중에 String에 값을 복사 후, ClientInfo 구조체에 저장할 수도 있음(Map, vector 등)
    std::array<char, NI_MAXSERV> hostPort{};

    int retval = getnameinfo((sockaddr *)&caddr, clen,
                             hostIP.data(), (DWORD)hostIP.size(),
                             hostPort.data(), (DWORD)hostPort.size(),
                             NI_NUMERICHOST | NI_NUMERICSERV);
    if (retval != 0)
    {
        std::cerr << "getnameinfo() 오류: " << gai_strerrorA(retval) << std::endl;
        return;
    }
    std::cout << "[클라이언트 접속] IP 주소: " << hostIP.data() << ", 포트 번호: " << hostPort.data() << std::endl
              << std::endl;
}

void IocpServer::IocpStart(SOCKET client_sock, const sockaddr_storage &caddr, socklen_t clen)
{
    // 소켓과 입출력 완료 포트 연결
    HANDLE h = CreateIoCompletionPort((HANDLE)client_sock, iocpHandle, (ULONG_PTR)client_sock, 0);
    if (h == nullptr)
    {
        err_display("CreateIoCompletionPort()");
        closesocket(client_sock);
        return;
    }

    // 소켓 정보 구조체 할당
    SOCKETINFO *sockInfo = new SOCKETINFO();
    if (sockInfo == nullptr)
        return;
    ZeroMemory(&sockInfo->overlapped, sizeof(OVERLAPPED)); // [check]
    sockInfo->sock = client_sock;
    sockInfo->recvBytes = 0;
    sockInfo->sendBytes = 0;
    sockInfo->wsabuf.len = BUFSIZE;
    sockInfo->wsabuf.buf = sockInfo->buffer.data();

    // 비동기 수신 시작
    DWORD flags = 0;
    int retval = WSARecv(client_sock, &sockInfo->wsabuf, 1, nullptr, &flags, &sockInfo->overlapped, nullptr); // [check] 누적 recv 처리
    if (retval == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            err_display("WSARecv()");
            closesocket(client_sock);
            delete sockInfo;
            return;
        }
    }
}

DWORD WINAPI IocpServer::WorkerThread(void *arg)
{
    HANDLE iocpHandle = static_cast<HANDLE>(arg);

    while (IsRunning())
    {
        DWORD cbTransferred = 0;
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED lpOv = nullptr;

        BOOL ok = GetQueuedCompletionStatus(
            iocpHandle,
            &cbTransferred,
            &completionKey,
            &lpOv,
            INFINITE);

        if (lpOv == nullptr) // stop()에서 running=false로 바꾸고 깨우기 용도로 PostQueuedCompletionStatus 호출 시, [check]
        {
            if (!IsRunning())
            {
                break; // 종료 지시 → 루프 종료
            }
            else
            {
                continue; // 그냥 깨우기 용도였다면 무시
            }
        }

        SOCKETINFO *ptr = reinterpret_cast<SOCKETINFO *>(lpOv); // OVERLAPPED가 첫 멤버여야 안전

        // 원격 종료 또는 실패, 정상 io 완료지만 전송된 바이트 수가 0인 경우도 포함
        if (!ok)
        {
            std::cout << "[클라이언트 종료] 소켓: " << ptr->sock << std::endl;
            closesocket(ptr->sock);
            delete ptr;
            continue;
        }

        // 주소 출력 [Check]
        sockaddr_storage ss{};
        int slen = sizeof(ss);
        char hostIP[NI_MAXHOST]{}, hostPort[NI_MAXSERV]{};
        if (getpeername(ptr->sock, reinterpret_cast<sockaddr *>(&ss), &slen) == 0)
        {
            int nameinfoResult = getnameinfo(reinterpret_cast<sockaddr *>(&ss), slen,
                                             hostIP, NI_MAXHOST, hostPort, NI_MAXSERV,
                                             NI_NUMERICHOST | NI_NUMERICSERV);
            if (nameinfoResult != 0)
            {
                std::cerr << "getnameinfo() 오류: " << gai_strerrorA(nameinfoResult) << std::endl;
                continue;
            }
        }

        // 수신/송신 진행도 업데이트
        if (ptr->recvBytes == 0)
        { // 이번 완료는 '수신'으로 간주 (최초 post가 WSARecv였기 때문)
            ptr->recvBytes = cbTransferred;
            ptr->sendBytes = 0;

            // 마지막 널문자 추가
            if (ptr->recvBytes < ptr->buffer.size())
                ptr->buffer[ptr->recvBytes] = '\0';

            // 서버에서 출력, wsarecv()에서 받은 데이터
            printf("[WorkerThread] 클라이언트 주소: %s, 포트: %s\n수신 데이터: %s\n", hostIP, hostPort, ptr->buffer.data());
        }
        else
        {
            ptr->sendBytes += cbTransferred; // 이번 완료는 '송신' 일부 완료, 누적
        }

        // 아직 보낼 게 남았으면 추가 송신
        if (ptr->recvBytes > ptr->sendBytes)
        {
            ZeroMemory(&ptr->overlapped, sizeof(OVERLAPPED));
            ptr->wsabuf.buf = ptr->buffer.data() + ptr->sendBytes;
            ptr->wsabuf.len = ptr->recvBytes - ptr->sendBytes;
            DWORD flags = 0;
            int r = WSASend(ptr->sock, &ptr->wsabuf, 1, nullptr, flags, &ptr->overlapped, nullptr); // [check] 후에 echo 외 다른 처리
            if (r == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
            {
                err_display("WSASend()");
                closesocket(ptr->sock);
                delete ptr;
                continue;
            }
        }
        else
        { // 모두 보냄 → 다음 수신 재개
            ptr->recvBytes = 0;
            ZeroMemory(&ptr->overlapped, sizeof(OVERLAPPED));
            ptr->wsabuf.buf = ptr->buffer.data();
            ptr->wsabuf.len = BUFSIZE;
            DWORD flags = 0;
            int r = WSARecv(ptr->sock, &ptr->wsabuf, 1, nullptr, &flags, &ptr->overlapped, nullptr);
            if (r == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
            {
                err_display("WSARecv()");
                closesocket(ptr->sock);
                delete ptr;
                continue;
            }
        }
    }
    return 0;
}
