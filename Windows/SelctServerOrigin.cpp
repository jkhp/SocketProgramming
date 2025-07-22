#include "../Common.h"

#define SERVERPORT 9000
#define BUFSIZE 512

// 소켓 정보 저장을 위한 구조체와 변수
struct SOCKETINFO
{
    SOCKET sock;
    bool isIpv6;
    bool isUdp;
    char buf[BUFSIZE + 1];
    int recvbytes;
    int sendbytes;
};

int nTotalSockets = 0;
SOCKETINFO *SocketInfoArray[FD_SETSIZE];

// 소켓 정보 관리 함수
bool AddSocketInfo(SOCKET sock, bool isIpv6, bool isUdp);
void RemoveSocketInfo(int nIndex);

// {최대 소켓 번호 + 1} 얻는 함수
int GetMaxFDPlus1(SOCKET s1, SOCKET s2, SOCKET s3, SOCKET s4);

int main(int argc, char *argv[])
{
    int retval;

    /**************** TCP/Ipv4 소켓 초기화 **************/
    // 소켓 생성
    SOCKET listen_sock4 = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock4 == INVALID_SOCKET)
        err_quit("soket()");

    // bind()
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);
    retval =
        bind(listen_sock4, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
        err_quit("bind()");

    // listen()
    retval = listen(listen_sock4, SOMAXCONN);
    if (retval == SOCKET_ERROR)
        err_quit("listen()");

    // TCP listen_sock4을 넌블로킹으로 설정
    int flags = fcntl(listen_sock4, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(listen_sock4, F_SETFL, flags);

    /************* tcp/Ipv4 소켓 초기화 종료 ***********/

    /************** TCP/Ipv6 소켓 초기화 시작 *************/
    // 소켓 생성
    SOCKET listen_sock6 = socket(AF_INET6, SOCK_STREAM, 0);
    if (listen_sock6 == INVALID_SOCKET)
        err_quit("socket()");

    // 듀얼 스택을 끈다. [Windows는 꺼져 있음(기본값). UNIX/Linux는 OS마다 다름]
    int no = 1;
    setsockopt(listen_sock6, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));

    // bind()
    struct sockaddr_in6 serveraddr6;
    memset(&serveraddr6, 0, sizeof(serveraddr6));
    serveraddr6.sin6_family = AF_INET6;
    serveraddr6.sin6_addr = in6addr_any;
    serveraddr6.sin6_port = htons(SERVERPORT);
    retval = bind(listen_sock6, (struct sockaddr *)&serveraddr6, sizeof(serveraddr6));
    if (retval == SOCKET_ERROR)
        err_quit("bind()");

    // listen()
    retval = listen(listen_sock6, SOMAXCONN);
    if (retval == SOCKET_ERROR)
        err_quit("listen()");

    // TCP listen_sock6을 넌블로킹으로 설정
    flags = fcntl(listen_sock6, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(listen_sock6, F_SETFL, flags);

    /************** TCP/Ipv6 소켓 초기화 종료 *************/

    /************** UDP/Ipv4 소켓 초기화 시작 *************/
    // 소켓 생성
    SOCKET udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock == INVALID_SOCKET)
        err_quit("socket()");

    // bind()
    struct sockaddr_in udp_serveraddr;
    memset(&udp_serveraddr, 0, sizeof(udp_serveraddr));
    udp_serveraddr.sin_family = AF_INET;
    udp_serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_serveraddr.sin_port = htons(SERVERPORT);
    retval = bind(udp_sock, (struct sockaddr *)&udp_serveraddr,
                  sizeof(udp_serveraddr));
    if (retval == SOCKET_ERROR)
        err_quit("bind()");

    // UDP udp_sock을 넌블로킹으로 설정
    flags = fcntl(udp_sock, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(udp_sock, F_SETFL, flags);

    /************** UDP/Ipv4 소켓 초기화 종료 *************/

    /************** UDP/Ipv6 소켓 초기화 시작 *************/
    // 소켓 생성
    SOCKET udp_sock6 = socket(AF_INET6, SOCK_DGRAM, 0);
    if (udp_sock6 == INVALID_SOCKET)
        err_quit("socket()");

    // 듀얼 스택을 끈다. [Windows는 꺼져 있음(기본값). UNIX/Linux는 OS마다 다름]
    // int no = 1;
    setsockopt(udp_sock6, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));

    // bind()
    struct sockaddr_in6 udp_serveraddr6;
    memset(&udp_serveraddr6, 0, sizeof(udp_serveraddr6));
    udp_serveraddr6.sin6_family = AF_INET6;
    udp_serveraddr6.sin6_addr = in6addr_any;
    udp_serveraddr6.sin6_port = htons(SERVERPORT);
    retval = bind(udp_sock6, (struct sockaddr *)&udp_serveraddr6, sizeof(udp_serveraddr6));
    if (retval == SOCKET_ERROR)
        err_quit("bind()");

    // UDP udp_sock6을 넌블로킹으로 설정
    flags = fcntl(udp_sock6, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(udp_sock6, F_SETFL, flags);

    /************** UDP/Ipv6 소켓 초기화 종료 *************/

    // 데이터 통신에 사용할 변수(공통)
    fd_set rset, wset;
    SOCKET client_sock;
    socklen_t addrlen;

    // 데이터 통신에 사용할 변수 (ipv4, ipv6)
    struct sockaddr_in clientaddr4;
    struct sockaddr_in6 clientaddr6;

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

        // select()
        int nready = select(GetMaxFDPlus1(listen_sock4, listen_sock6, udp_sock, udp_sock6), &rset, &wset, NULL, NULL);
        if (nready == SOCKET_ERROR)
            err_quit("select()");

        // UDP 새 클라이언트 수신 처리_IPV4 -> listen_sock이 아닌 udp_sock을 사용
        if (FD_ISSET(udp_sock, &rset))
        {
            addrlen = sizeof(clientaddr4);
            char buffer[BUFSIZE + 1]; // UDP는 단일 소켓으로 모든 클라이언트 요청을 받아서 따로 buffer를 선언, socketinfo가 사실상 무의미

            retval = recvfrom(udp_sock, buffer, BUFSIZE, 0, (struct sockaddr *)&clientaddr4, &addrlen);

            if (retval == SOCKET_ERROR)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK) // 넌블로킹 소켓에서 데이터가 없을 때는 오류가 아님
                {
                    err_quit("recvfrom()");
                    break;
                }
                continue;
            }
            else
            {
                buffer[retval] = '\0';

                char addr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientaddr4.sin_addr, addr, sizeof(addr));
                printf("[UDP/IPV4 서버] 클라이언트 접속: IP 주소=%s, 포트번호=%d\n", addr, ntohs(clientaddr4.sin_port));
                printf("[UDP/IPV4 서버] 받은 데이터: %s\n", buffer);

                // 에코 응답
                int sendlen = retval;
                retval = sendto(udp_sock, buffer, sendlen, 0, (struct sockaddr *)&clientaddr4, addrlen);
                if (retval == SOCKET_ERROR)
                {
                    err_display("sendto()");
                    continue;
                }

            } // end of else
        }

        // UDP 새 클라이언트 수신 처리_IPV6 -> listen_sock6이 아닌 udp_sock6을 사용
        if (FD_ISSET(udp_sock6, &rset))
        {
            addrlen = sizeof(clientaddr6);
            char buffer[BUFSIZE + 1]; // UDP는 단일 소켓으로 모든 클라이언트 요청을 받아서 따로 buffer를 선언, socketinfo가 사실상 무의미?

            retval = recvfrom(udp_sock6, buffer, BUFSIZE, 0, (struct sockaddr *)&clientaddr6, &addrlen);

            if (retval == SOCKET_ERROR)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK) // 넌블로킹 소켓에서 데이터가 없을 때는 오류가 아님
                {
                    err_quit("recvfrom()");
                    break;
                }
                continue;
            }
            else
            {
                buffer[retval] = '\0';

                char addr[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &clientaddr6.sin6_addr, addr, sizeof(addr));
                printf("[UDP/IPV6 서버] 클라이언트 접속: IP 주소=%s, 포트번호=%d\n", addr, ntohs(clientaddr6.sin6_port));
                printf("[UDP/IPV6 서버] 받은 데이터: %s\n", buffer);

                // 에코 응답
                int sendlen = retval;
                retval = sendto(udp_sock6, buffer, sendlen, 0, (struct sockaddr *)&clientaddr6, addrlen);
                if (retval == SOCKET_ERROR)
                {
                    err_display("sendto()");
                    continue;
                }
            } // end of else
        }

        // TCP 새 클라이언트 접속 처리_IPV4
        if (FD_ISSET(listen_sock4, &rset)) // 준비된 listen_sock이 있으면
        {
            addrlen = sizeof(clientaddr4);
            client_sock = accept(listen_sock4, (struct sockaddr *)&clientaddr4, &addrlen);
            if (client_sock == INVALID_SOCKET)
            {
                err_display("accept()");
                break;
            }
            else
            {
                // 클라이언트 소켓도 넌블로킹으로 설정, recv(), send()에서 데이터가 준비되지 않았을 때 블로킹 없이 빠르게 return
                int flags = fcntl(client_sock, F_GETFL);
                flags |= O_NONBLOCK;
                fcntl(client_sock, F_SETFL, flags);

                // 클라이언트 접속 정보 출력
                char addr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientaddr4.sin_addr, addr, sizeof(addr));
                printf("\n[TCP/IPV4 서버] 클라이언트 접속 : IP 주소 = %s, 포트 번호=%d\n", addr, ntohs(clientaddr4.sin_port));

                // 소켓 정보 저장, 실패시 소켓 닫음
                if (!AddSocketInfo(client_sock, false, false))
                    close(client_sock);
            }
            if (--nready <= 0) // 이벤트를 처리한 뒤 nready를 감소시키면서 모든 이벤트를 소진할 수 있음.
                continue;      // 처리할 이벤트 없으면 다음 루프로
        }

        // TCP 새 클라이언트 접속 처리_IPV6
        if (FD_ISSET(listen_sock6, &rset)) // 준비된 listen_sock이 있으면
        {
            addrlen = sizeof(clientaddr6);
            client_sock = accept(listen_sock6, (struct sockaddr *)&clientaddr6, &addrlen);
            if (client_sock == INVALID_SOCKET)
            {
                err_display("accept()");
                break;
            }
            else
            {
                // 클라이언트 소켓도 넌블로킹으로 설정, recv(), send()에서 데이터가 준비되지 않았을 때 블로킹 없이 빠르게 return
                int flags = fcntl(client_sock, F_GETFL);
                flags |= O_NONBLOCK;
                fcntl(client_sock, F_SETFL, flags);

                // 클라이언트 접속 정보 출력
                char addr[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &clientaddr6.sin6_addr, addr, sizeof(addr));
                printf("\n[TCP/IPV6 서버] 클라이언트 접속 : IP 주소 = %s, 포트 번호=%d\n", addr, ntohs(clientaddr6.sin6_port));

                // 소켓 정보 저장, 실패시 소켓 닫음
                if (!AddSocketInfo(client_sock, true, false))
                    close(client_sock);
            }
            if (--nready <= 0) // 이벤트(준비된 소켓)를 처리한 뒤 nready를 감소시키면서 모든 이벤트를 소진할 수 있음.
                continue;      // 처리할 이벤트 없으면 다음 루프로
        }

        // 클라이언트 소켓에서 데이터 수신/전송 처리
        for (int i = 0; i < nTotalSockets; i++)
        {
            // 데이터 수신
            if (FD_ISSET(SocketInfoArray[i]->sock, &rset))
            {
                retval = recv(SocketInfoArray[i]->sock, SocketInfoArray[i]->buf, BUFSIZE, 0);

                if (retval == SOCKET_ERROR)
                {
                    err_display("recv()");
                    RemoveSocketInfo(i); // 소켓 정보 삭제
                }

                else if (retval == 0) // 정상 종료
                {
                    RemoveSocketInfo(i);
                }

                else
                {
                    SocketInfoArray[i]->recvbytes = retval;
                    SocketInfoArray[i]->buf[retval] = '\0';

                    if (SocketInfoArray[i]->isIpv6) // ipv6일 경우
                    {
                        addrlen = sizeof(clientaddr6);
                        getpeername(SocketInfoArray[i]->sock, (struct sockaddr *)&clientaddr6, &addrlen); // 소켓에 연결된 피어의 주소 검색, 구조체에 주소 정보 저장, 오류가 아닐 경우 0을 반환

                        // 클라이언트 정보 출력
                        char addr[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, &clientaddr6.sin6_addr, addr, sizeof(addr));
                        printf("[TCP/IPV6 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n", addr, ntohs(clientaddr6.sin6_port));
                    }
                    else // ipv4일 경우
                    {
                        addrlen = sizeof(clientaddr4);
                        getpeername(SocketInfoArray[i]->sock, (struct sockaddr *)&clientaddr4, &addrlen); // 소켓에 연결된 피어의 주소 검색, 구조체에 주소 정보 저장, 오류가 아닐 경우 0을 반환

                        // 클라이언트 정보 출력
                        char addr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &clientaddr4.sin_addr, addr, sizeof(addr));
                        printf("[TCP/IPV4 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n", addr, ntohs(clientaddr4.sin_port));
                    }

                    // 받은 데이터 출력
                    printf("[TCP/%s 서버] 받은 데이터: %s\n", SocketInfoArray[i]->isIpv6 ? "Ipv6" : "Ipv4", SocketInfoArray[i]->buf);
                } // end of else
            }

            // 데이터 송신 (echo server)
            else if (FD_ISSET(SocketInfoArray[i]->sock, &wset))
            {

                retval = send(SocketInfoArray[i]->sock, SocketInfoArray[i]->buf + SocketInfoArray[i]->sendbytes,
                              SocketInfoArray[i]->recvbytes - SocketInfoArray[i]->sendbytes, 0); // '전송할 데이터 주소 계산' 및 '보낼 데이터 크기 계산'을위해 sendbytes를 이용

                // 송신 오류 처리
                if (retval == SOCKET_ERROR)
                {
                    err_display("send()");
                    RemoveSocketInfo(i);
                }
                // 정상 송신
                else
                {
                    SocketInfoArray[i]->sendbytes += retval;
                    if (SocketInfoArray[i]->recvbytes == SocketInfoArray[i]->sendbytes)    // 송신 완료하면
                        SocketInfoArray[i]->recvbytes = SocketInfoArray[i]->sendbytes = 0; // 0으로 초기화
                }
            }
        } // end of for (송수신 처리)
    } // end of while(1)

    // 소켓 닫기
    close(listen_sock4);
    close(listen_sock6);
    close(udp_sock);
    close(udp_sock6);

    for (int i = 0; i < nTotalSockets; i++)
    {
        if (SocketInfoArray[i] != NULL)
        {
            close(SocketInfoArray[i]->sock);
            delete SocketInfoArray[i];
        }
    }

    return 0;
}

bool AddSocketInfo(SOCKET sock, bool isIpv6, bool isUdp)
{
    if (nTotalSockets >= FD_SETSIZE)
    {
        printf("[오류] 소켓 정보를 추가할 수 없습니다!\n");
        return false;
    }
    SOCKETINFO *ptr = new SOCKETINFO;
    if (ptr == NULL)
    {
        printf("[오류] 메모리가 부족합니다!\n");
        return false;
    }
    ptr->sock = sock;
    ptr->isIpv6 = isIpv6;
    ptr->isUdp = isUdp;
    ptr->recvbytes = 0;
    SocketInfoArray[nTotalSockets++] = ptr;
    return true;
}

// 소켓 정보 삭제
void RemoveSocketInfo(int nIndex)
{
    SOCKETINFO *ptr = SocketInfoArray[nIndex];

    if (ptr->isIpv6 == false)
    {
        // 클라이언트 정보 얻기
        struct sockaddr_in clientaddr;
        socklen_t addrlen = sizeof(clientaddr);
        getpeername(ptr->sock, (struct sockaddr *)&clientaddr, &addrlen);
        // 클라이언트 정보 출력
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
        printf("[TCP/IPv4 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n", addr, ntohs(clientaddr.sin_port));
    }
    else
    {
        // 클라이언트 정보 얻기
        struct sockaddr_in6 clientaddr;
        socklen_t addrlen = sizeof(clientaddr);
        getpeername(ptr->sock, (struct sockaddr *)&clientaddr, &addrlen);
        // 클라이언트 정보 출력
        char addr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &clientaddr.sin6_addr, addr, sizeof(addr));
        printf("[TCP/IPv6 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n", addr, ntohs(clientaddr.sin6_port));
    }
    // 소켓 닫기
    close(ptr->sock);
    delete ptr;

    if (nIndex != (nTotalSockets - 1))
        SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets - 1];
    --nTotalSockets;
}

// {최대 소켓 번호 + 1} 얻기
int GetMaxFDPlus1(SOCKET s1, SOCKET s2, SOCKET s3, SOCKET s4)
{
    int maxfd = s1;
    if (s2 > maxfd)
        maxfd = s2;
    if (s3 > maxfd)
        maxfd = s3;
    if (s4 > maxfd)
        maxfd = s4;

    for (int i = 0; i < nTotalSockets; i++)
    {
        if (SocketInfoArray[i]->sock > maxfd)
        {
            maxfd = SocketInfoArray[i]->sock;
        }
    }
    return maxfd + 1;
}
