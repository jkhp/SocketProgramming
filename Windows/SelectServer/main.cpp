#include "Common.h"
#include "NetHandler.h"
#include "SocketManager.h"

#define SERVERPORT 9000

SOCKET InitTcpSocket(int af_family, int port);
SOCKET InitUdpSocket(int af_family, int port);

int main()
{
	// 윈속 초기화
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    // 소켓 초기화
    SOCKET listen_sock4 = InitTcpSocket(AF_INET, SERVERPORT);
    SOCKET listen_sock6 = InitTcpSocket(AF_INET6, SERVERPORT);
    SOCKET udp_sock = InitUdpSocket(AF_INET, SERVERPORT);
    SOCKET udp_sock6 = InitUdpSocket(AF_INET6, SERVERPORT);

    fd_set rset, wset;

    while (1)
    {
        FD_ZERO(&rset);
        FD_ZERO(&wset);

        FD_SET(listen_sock4, &rset);
        FD_SET(listen_sock6, &rset);
        FD_SET(udp_sock, &rset);
        FD_SET(udp_sock6, &rset);

        for (int i = 0; i < nTotalSockets; i++)
        {
            if (SocketInfoArray[i]->recvbytes >
                SocketInfoArray[i]->sendbytes) // 아직 클라이언트에게 다 보내지 못한
                                               // 데이터가 있으면 write 준비
                FD_SET(SocketInfoArray[i]->sock, &wset);
            else
                FD_SET(SocketInfoArray[i]->sock, &rset); // read 준비
        }

        int nready = select(GetMaxFDPlus1(listen_sock4, listen_sock6, udp_sock, udp_sock6), &rset, &wset, NULL, NULL);
        if (nready == SOCKET_ERROR)
            err_quit("select()");

        if (FD_ISSET(listen_sock4, &rset))
        {
            AcceptNewTcpClient(listen_sock4, false);
            if (--nready <= 0) // 준비된 이벤트를 수행했으니 nready 감소, 이벤트를 다 처리하면
                continue;      // 루프 처음으로 돌아감
        }

        if (FD_ISSET(listen_sock6, &rset))
        {
            AcceptNewTcpClient(listen_sock6, true);
            if (--nready <= 0)
                continue;
        }

        // TCP 데이터 수신/송신 처리
        for (int i = 0; i < nTotalSockets; i++)
        {
            if (!SocketInfoArray[i])
                continue;

            if (FD_ISSET(SocketInfoArray[i]->sock, &rset))
            {
                HandleTcpReceive(i);
                if (--nready <= 0)
                    break;
            }
            else if (FD_ISSET(SocketInfoArray[i]->sock, &wset))
            {
                HandleTcpSend(i);
                if (--nready <= 0)
                    break;
            }
        }

        // UDP 처리
        if (FD_ISSET(udp_sock, &rset))
            HandleUdpIpv4(udp_sock);
        if (FD_ISSET(udp_sock6, &rset))
            HandleUdpIpv6(udp_sock6);
    }

    // 종료 처리
    closesocket(listen_sock4);
    closesocket(listen_sock6);
    closesocket(udp_sock);
    closesocket(udp_sock6);

    for (int i = 0; i < nTotalSockets; i++)
    {
        if (SocketInfoArray[i])
        {
            closesocket(SocketInfoArray[i]->sock);
            delete SocketInfoArray[i];
        }
    }

	WSACleanup();
    return 0;
}

SOCKET InitTcpSocket(int af_family, int port)
{
    SOCKET sock = socket(af_family, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        err_quit("socket()");

    if (af_family == AF_INET6)
    {
        int no = 1;
        setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&no, sizeof(no));
    }

    if (af_family == AF_INET)
    {
        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
            err_quit("bind()");
    }
    else
    {
        struct sockaddr_in6 addr6 = {};
        addr6.sin6_family = AF_INET6;
        addr6.sin6_addr = in6addr_any;
        addr6.sin6_port = htons(port);
        if (bind(sock, (struct sockaddr *)&addr6, sizeof(addr6)) == SOCKET_ERROR)
            err_quit("bind()");
    }

    if (listen(sock, SOMAXCONN) == SOCKET_ERROR)
        err_quit("listen()");

    return sock;
}

SOCKET InitUdpSocket(int af_family, int port)
{
    SOCKET sock = socket(af_family, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET)
        err_quit("socket()");

    if (af_family == AF_INET6)
    {
        int no = 1;
        setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*) & no, sizeof(no));
    }

    if (af_family == AF_INET)
    {
        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
            err_quit("bind()");
        SetNonBlocking(sock);
    }
    else
    {
        struct sockaddr_in6 addr6 = {};
        addr6.sin6_family = AF_INET6;
        addr6.sin6_addr = in6addr_any;
        addr6.sin6_port = htons(port);
        if (bind(sock, (struct sockaddr *)&addr6, sizeof(addr6)) == SOCKET_ERROR)
            err_quit("bind()");
        SetNonBlocking(sock);
    }

    return sock;
}
