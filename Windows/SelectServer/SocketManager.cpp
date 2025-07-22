#include "SocketManager.h"

SOCKETINFO *SocketInfoArray[MAX_SOCKETS] = {0};
int nTotalSockets = 0;

bool AddSocketInfo(SOCKET sock, bool isIpv6, bool isUdp)
{
    if (nTotalSockets >= MAX_SOCKETS)
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
    ptr->sendbytes = 0;
    SocketInfoArray[nTotalSockets++] = ptr;

    return true;
}

void RemoveSocketInfo(int index)
{
    SOCKETINFO *ptr = SocketInfoArray[index];

    // 클라이언트 주소 정보 출력
    if (!ptr->isIpv6)
    {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        if (getpeername(ptr->sock, (struct sockaddr *)&addr, &len) == 0)
        {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
            printf("[TCP/IPV4 종료] IP=%s, PORT=%d\n", ip, ntohs(addr.sin_port));
        }
        else
        {
            struct sockaddr_in6 addr;
            socklen_t len = sizeof(addr);
            if (getpeername(ptr->sock, (struct sockaddr *)&addr, &len) == 0)
            {
                char ip[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &addr.sin6_addr, ip, sizeof(ip));
                printf("[TCP/IPV6 종료] IP=%s, PORT=%d\n", ip, ntohs(addr.sin6_port));
            }
        }
    }

    closesocket(ptr->sock);
    delete ptr;

    if (index != nTotalSockets - 1) // 마지막 소켓이 아니면
    {
        SocketInfoArray[index] = SocketInfoArray[nTotalSockets - 1]; // 마지막 소켓 정보를 현재 인덱스로 이동
    }
    SocketInfoArray[nTotalSockets - 1] = nullptr; // 마지막 소켓 정보는 nullptr로 설정
    --nTotalSockets;                              // 총 소켓 수 감소
}

int GetMaxFDPlus1(SOCKET s1, SOCKET s2, SOCKET s3, SOCKET s4)
{
    int maxfd = 0; // 최대 소켓 번호(파일 디스크립터 번호)를 저장할 변수

    if (s1 > maxfd)
        maxfd = s1;
    if (s2 > maxfd)
        maxfd = s2;
    if (s3 > maxfd)
        maxfd = s3;
    if (s4 > maxfd)
        maxfd = s4;

    for (int i = 0; i < nTotalSockets; i++)
    {
        if (SocketInfoArray[i]->sock > maxfd)
            maxfd = SocketInfoArray[i]->sock;
    }

    return maxfd + 1; // select() 함수는 최대 소켓 번호 + 1을 필요로 함
}