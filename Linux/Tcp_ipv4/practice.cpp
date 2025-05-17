#include "../Common.h"

#define SERVERPORT 9000
#define BUFSIZE 512

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

bool AddSocketInfo(SOCKET sock, bool isIpv6, bool isUdp);
void RemoveSocketInfo(int nIndex);
int GetMaxFDPlus1(SOCKET s1, SOCKET s2, SOCKET s3, SOCKET s4);

void Udp_code()
{
    SOCKET udp_sock, udp_sock6, client_sock;
    struct sockaddr_in udp_serveraddr, clientaddr4;
    struct sockaddr_in6 udp_serveraddr6, clientaddr6;
    socketlen_t addrlen;
    fd_set rset, wset;

    // 1. 새 클라이언트 접속 처리_UDP_IPV4 -> listen_sock이 아닌 udp_sock을 사용
    if (FD_ISSET(udp_sock, &rset))
    {
        // addrlen = sizeof(clientaddr4);
        if (udp_sock == INVALID_SOCKET)
            err_quit("socket()");
        else
        { // 소켓을 넌블로킹으로 설정정
            int flags = fcntl(udp_sock, F_GETFL, 0);
            flags |= O_NONBLOCK;
            fcntl(udp_sock, F_SETFL, flags);

            // 접속 정보 출력
            char addr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientaddr4.sin_addr, addr, sizeof(addr));
            printf("[UDP/IPV4 서버] 클라이언트 접속: IP 주소=%s, 포트번호=%d\n", addr, ntohs(clientaddr4.sin_port));

            // 소켓 정보 저장, 실패시 소켓 닫음
            if (!AddSocketInfo(udp_sock, false, true))
            {
                close(udp_sock);
            }
        } // end of else

        if (--nready <= 0) // 이벤트를 처리한 뒤 nready를 감소시키면서 모든 이벤트를 소진할 수 있음.
            continue;      // 처리할 이벤트 없으면 다음 루프로
    }

    // 2. 새 클라이언트 접속 처리_UDP_IPV6 -> listen_sock6이 아닌 udp_sock6을 사용
    if (FD_ISSET(udp_sock6, &rset))
    {
        // addrlen = sizeof(clientaddr6);
        if (udp_sock6 == INVALID_SOCKET)
            err_quit("socket()");
        else
        { // 소켓을 넌블로킹으로 설정정
            int flags = fcntl(udp_sock6, F_GETFL, 0);
            flags |= O_NONBLOCK;
            fcntl(udp_sock6, F_SETFL, flags);

            // 접속 정보 출력
            char addr[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &clientaddr6.sin6_addr, addr, sizeof(addr));
            printf("[UDP/IPV6 서버] 클라이언트 접속: IP 주소=%s, 포트번호=%d\n", addr, ntohs(clientaddr6.sin6_port));

            // 소켓 정보 저장, 실패시 소켓 닫음
            if (!AddSocketInfo(udp_sock6, true, true))
            {
                close(udp_sock6);
            }
        } // end of else

        if (--nready <= 0) // 이벤트를 처리한 뒤 nready를 감소시키면서 모든 이벤트를 소진할 수 있음.
            continue;      // 처리할 이벤트 없으면 다음 루프로
    }

    // 3. 데이터 수신 (정상_UDP_IPV4)
    if (SocketInfoArray[i]->isUdp)
    {
        addrlen = sizeof(clientaddr4);
        getpeername(SocketInfoArray[i]->sock, (struct sockaddr *)&clientaddr4, addrlen);

        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientaddr4.sin_addr, addr, sizeof(addr));
        printf("[UDP/IPV4 서버] 클라이언트 접속: IP 주소=%s, 포트번호=%d\n", addr, ntohs(clientaddr4.sin_port));
    }

    // 4. 데이터 수신 (정상_UDP_IPV6)
    if (SocketInfoArray[i]->isUdp)
    {
        addrlen = sizeof(clientaddr6);
        getpeername(SocketInfoArray[i]->sock, (struct sockaddr *)&clientaddr6, addrlen);

        char addr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &clientaddr6.sin6_addr, addr, sizeof(addr));
        printf("[UDP/IPV6 서버] 클라이언트 접속: IP 주소=%s, 포트번호=%d\n", addr, ntohs(clientaddr6.sin6_port));
    }
}