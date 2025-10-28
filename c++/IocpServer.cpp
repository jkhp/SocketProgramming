#include "IocpServer.hpp"
#include <iostream>
#include <array>

#define BUFSIZE 1024

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
        if (main_thread.joinable())
            main_thread.join();

        for (auto &t : worker_threads)
        {
            if (t.joinable())
                t.join();
        }

        WSACleanup();
    }
}

void IocpServer::Run()
{
    iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); // IOCP 핸들 생성
    if (iocpHandle == NULL)
        err_quit("CreateIoCompletionPort()");

    MakeWorkerThread();

    InitSocket();
    BindAndListen();

    while (IsRunning())
    {
        sockaddr_storage caddr{};
        socklen_t clen{};
        SOCKET client_sock = AcceptClient(caddr, clen);
        if (client_sock == INVALID_SOCKET)
            continue;

        PrintClientInfo(caddr, clen);
        IocpStart(client_sock, caddr, clen); // IOCP에서는 여기서 처리하지 않음
    }
    closesocket(listen_sock);
    listen_sock = INVALID_SOCKET;
}

void IocpServer::MakeWorkerThread()
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    // CPU *2 개수만큼 워커 스레드 생성
    for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors * 2; ++i) // c++에서는 ++i를 권장, 전위증가 연산자가 불필요한 임시객체 생성을 줄여줌
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
        err_quit("lisetn()");

    std::cout << "[TCP 서버] 포트번호: " << port << ", " << (af == AF_INET ? "IPv4" : "IPv6") << " 시작" << std::endl;
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
    std::array<char, NI_MAXHOST> hostIP{}; // 나중에 String으로 변환, ClientInfo 구조체에 저장할 수도 있음(Map, vector 등)
    std::array<char, NI_MAXSERV> hostPort{};

    int retval = getnameinfo((sockaddr *)&caddr, clen, hostIP.data(), hostIP.size(), hostPort.data(), hostPort.size(), NI_NUMERICHOST | NI_NUMERICSERV);
    if (retval != 0)
    {
        std::cerr << "getnameinfo() 오류: " << gai_strerrorA(retval) << std::endl;
        // closesocket(client_sock);
        return;
    }
    std::cout << "[IOCP 클라이언트 접속] IP 주소: " << hostIP.data() << ", 포트 번호: " << hostPort.data() << std::endl;
}

void IocpServer::IocpStart(SOCKET client_sock, const sockaddr_storage &caddr, socklen_t clen)
{
    // 소켓과 입출력 완료 포트 연결
    CreateIoCompletionPort((HANDLE)client_sock, iocpHandle, client_sock, 0);

    // 소켓 정보 구조체 할당
    std::shared_ptr<SOCKETINFO> sockInfo = std::make_shared<SOCKETINFO>();
    if (sockInfo == NULL)
        return;
    memset(&sockInfo->overlapped, 0, sizeof(OVERLAPPED));
    sockInfo->sock = client_sock;
    sockInfo->recvBytes = 0;
    sockInfo->sendBytes = 0;
    sockInfo->wsabuf.len = BUFSIZE;
    sockInfo->wsabuf.buf = sockInfo->buffer.data();

    // 비동기 수신 시작
    DWORD flags = 0;
    int retval = WSARecv(client_sock, &sockInfo->wsabuf, 1, NULL, &flags, &sockInfo->overlapped, NULL);
    if (retval == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            err_display("WSARecv()");
            closesocket(client_sock);
            return;
        }
    }
}

DWORD WINAPI IocpServer::WorkerThread(void *arg)
{
}
