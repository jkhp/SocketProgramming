#include "TcpServer.hpp"
#include <iostream>

#define BUFSIZE 1024

void TcpServer::Start()
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        err_quit("WSAStartup()");
#endif
    running = true;
    worker = std::thread(&TcpServer::Run, this); // 클래스 맴버 함수를 쓰레드로 실행하려면 객체주소(this)를 같이 넘겨줘야 함. 뒤에는 함수의 매개변수
}

void TcpServer::Stop()
{
    if (running.exchange(false))
    { // .exchange : atomic 변수의 값을 변경하고 이전 값을 반환(true, false), 이전 값이 true인 경우에만 종료 작업 수행(중복 종료 방지)
        if (listen_sock != INVALID_SOCKET)
        {
            close_socket(listen_sock);
            listen_sock = INVALID_SOCKET;
        }
        if (worker.joinable()) // joinable() : 스레드가 실행 중인지 확인
            worker.join();     // 스레드 종료 대기
#ifdef _WIN32
        WSACleanup();
#endif
    }
}

void TcpServer::InitSocket()
{
    listen_sock = socket(af, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET)
        err_quit("socket()");
}

void TcpServer::BindAndListen()
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
    else
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

SOCKET TcpServer::AcceptClient(sockaddr_storage &caddr, socklen_t &clen)
{
    SOCKET client = accept(listen_sock, (sockaddr *)&caddr, &clen);

    if (client == INVALID_SOCKET)
        err_display("accept()");

    return client;
}

void TcpServer::PrintClientInfo(const sockaddr_storage &caddr, socklen_t clen)
{
    char host[NI_MAXHOST], serv[NI_MAXSERV];
    if (getnameinfo((sockaddr *)&caddr, clen, host, sizeof(host), serv, sizeof(serv),
                    NI_NUMERICHOST | NI_NUMERICSERV) == 0)
    {
        std::cout << "[TCP 서버] 클라이언트 접속: " << host << ":" << serv << std::endl;
    }
}

void TcpServer::HandleClient(SOCKET client, const sockaddr_storage &caddr, socklen_t clen)
{
    char host[NI_MAXHOST], serv[NI_MAXSERV];
    getnameinfo((sockaddr *)&caddr, clen, host, sizeof(host), serv, sizeof(serv),
                NI_NUMERICHOST | NI_NUMERICSERV);

    char buf[BUFSIZE + 1];
    while (true)
    {
        int n = recv(client, buf, BUFSIZE, 0);
        if (n <= 0)
            break;
        buf[n] = '\0';
        std::cout << "[TCP/" << host << ":" << serv << "] " << buf << std::endl;

        int sent = send(client, buf, n, 0);
        if (sent == SOCKET_ERROR)
        {
            err_display("send()");
            break;
        }
    }

    close_socket(client);
    std::cout << "[TCP 서버] 클라이언트 종료: " << host << ":" << serv << std::endl;
}

void TcpServer::Run()
{
    InitSocket();
    BindAndListen();

    while (running)
    {
        sockaddr_storage caddr{};
        socklen_t clen{};
        SOCKET client = AcceptClient(caddr, clen);
        if (client == INVALID_SOCKET)
            continue;

        PrintClientInfo(caddr, clen);
        HandleClient(client, caddr, clen);
    }

    close_socket(listen_sock);
    listen_sock = INVALID_SOCKET;
}