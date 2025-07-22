#include "Common.h"
#include "NetHandler.h"

void AcceptNewTcpClient(SOCKET listen_sock, bool isIpv6)
{
    struct sockaddr_storage clientaddr;
    socklen_t addrlen = sizeof(clientaddr);

    SOCKET client_sock = accept(listen_sock, (struct sockaddr *)&clientaddr, &addrlen);
    if (client_sock == INVALID_SOCKET)
    {
        err_display("accept()");
        return;
    }
    else
    {
        // 클라이언트 소켓도 넌블로킹으로 설정, recv(), send()에서 데이터가 준비되지 않았을 때 블로킹 없이 빠르게 return
        SetNonBlocking(client_sock);

        // 클라이언트 접속 정보 출력
        char addr[INET6_ADDRSTRLEN];
        int port;

        if (!isIpv6)
        {
            struct sockaddr_in *clientaddr4 = (struct sockaddr_in *)&clientaddr;
            inet_ntop(AF_INET, &clientaddr4->sin_addr, addr, sizeof(addr));
            port = ntohs(clientaddr4->sin_port);
        }
        else
        {
            struct sockaddr_in6 *clientaddr6 = (struct sockaddr_in6 *)&clientaddr;
            inet_ntop(AF_INET6, &clientaddr6->sin6_addr, addr, sizeof(addr));
            port = ntohs(clientaddr6->sin6_port);
        }

        printf("[TCP/%s 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n", isIpv6 ? "Ipv6" : "Ipv4", addr, port);

        // 소켓 정보 저장, 실패시 소켓 닫음
        if (!AddSocketInfo(client_sock, isIpv6, false))
            closesocket(client_sock);
    }
}

void HandleTcpReceive(int index)
{
    SOCKETINFO *socketInfo = SocketInfoArray[index];
    int retval = recv(socketInfo->sock, socketInfo->buf, BUFSIZE, 0);

    if (retval == SOCKET_ERROR)
    {
        err_display("recv()");
        RemoveSocketInfo(index);
        return;
    }
    else if (retval == 0) // 정상 종료
    {
        RemoveSocketInfo(index);
        return;
    }
    else
    {
        socketInfo->recvbytes = retval;
        socketInfo->sendbytes = 0;
        socketInfo->buf[retval] = '\0';

        struct sockaddr_storage clientaddr;
        socklen_t addrlen = sizeof(clientaddr);
        getpeername(socketInfo->sock, (struct sockaddr *)&clientaddr, &addrlen);

        char addr[INET6_ADDRSTRLEN];
        int port;

        if (!socketInfo->isIpv6)
        {
            struct sockaddr_in *clientaddr4 = (struct sockaddr_in *)&clientaddr;
            inet_ntop(AF_INET, &clientaddr4->sin_addr, addr, sizeof(addr));
            port = ntohs(clientaddr4->sin_port);
        }
        else
        {
            struct sockaddr_in6 *clientaddr6 = (struct sockaddr_in6 *)&clientaddr;
            inet_ntop(AF_INET6, &clientaddr6->sin6_addr, addr, sizeof(addr));
            port = ntohs(clientaddr6->sin6_port);
        }

        printf("[TCP/%s 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
               socketInfo->isIpv6 ? "Ipv6" : "Ipv4", addr, port);

        // 받은 데이터 출력
        printf("[TCP/%s 서버] 받은 데이터: %s\n", socketInfo->isIpv6 ? "Ipv6" : "Ipv4", socketInfo->buf);
    }
}

void HandleTcpSend(int index)
{
    SOCKETINFO *socketInfo = SocketInfoArray[index];
    int retval = send(socketInfo->sock, socketInfo->buf + socketInfo->sendbytes, // [socketInfo->buf + socketInfo->sendbytes] 보낸만큼 건너뛰고(포인터 연산)
                      socketInfo->recvbytes - socketInfo->sendbytes, 0);         // [socketInfo->recvbytes - socketInfo->sendbytes] 남은 바이트 수 계산

    if (retval == SOCKET_ERROR)
    {
        err_display("send()");
        RemoveSocketInfo(index);
        return;
    }

    else if (retval == 0) // 정상 종료
    {
        RemoveSocketInfo(index);
        return;
    }
    else
    {
        socketInfo->sendbytes += retval;
        if (socketInfo->sendbytes == socketInfo->recvbytes)
            socketInfo->recvbytes = socketInfo->sendbytes = 0; // 송신 완료 후 초기화
    }
}

void HandleUdpIpv4(SOCKET sock)
{
    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof(clientaddr);
    char buffer[BUFSIZE + 1];

    int retval = recvfrom(sock, buffer, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &addrlen);
    if (retval == SOCKET_ERROR)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK) // 넌블로킹 소켓에서 데이터가 없을 때는 오류가 아님
        {
            err_display("recvfrom()");
            return;
        }
        return; // 데이터가 없으면 그냥 리턴
    }

    buffer[retval] = '\0';
    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
    printf("[UDP/IPV4 서버] 클라이언트 접속: IP 주소=%s, 포트번호=%d\n", addr, ntohs(clientaddr.sin_port));
    printf("[UDP/IPV4 서버] 받은 데이터: %s\n", buffer);

    // 에코 응답
    retval = sendto(sock, buffer, retval, 0, (struct sockaddr *)&clientaddr, addrlen);
    if (retval == SOCKET_ERROR)
    {
        err_display("sendto()");
        return;
    }
}

void HandleUdpIpv6(SOCKET sock)
{
    struct sockaddr_in6 clientaddr;
    socklen_t addrlen = sizeof(clientaddr);
    char buffer[BUFSIZE + 1];

    int retval = recvfrom(sock, buffer, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &addrlen);
    if (retval == SOCKET_ERROR)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK) // 넌블로킹 소켓에서 데이터가 없을 때는 오류가 아님
        {
            err_display("recvfrom()");
            return;
        }
        return; // 데이터가 없으면 그냥 리턴
    }

    buffer[retval] = '\0';
    char addr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &clientaddr.sin6_addr, addr, sizeof(addr));
    printf("[UDP/IPV6 서버] 클라이언트 접속: IP 주소=%s, 포트번호=%d\n", addr, ntohs(clientaddr.sin6_port));
    printf("[UDP/IPV6 서버] 받은 데이터: %s\n", buffer);

    // 에코 응답
    retval = sendto(sock, buffer, retval, 0, (struct sockaddr *)&clientaddr, addrlen);
    if (retval == SOCKET_ERROR)
    {
        err_display("sendto()");
        return;
    }
}
