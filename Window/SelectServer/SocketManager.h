#pragma once
#include "Common.h"

#define BUFSIZE 512
#define MAX_SOCKETS FD_SETSIZE

struct SOCKETINFO
{
    SOCKET sock;
    bool isIpv6;
    bool isUdp;
    char buf[BUFSIZE + 1];
    int recvbytes;
    int sendbytes;
};

// 소켓 정보 관리 함수
bool AddSocketInfo(SOCKET sock, bool isIpv6, bool isUdp);
void RemoveSocketInfo(int index);
int GetMaxFDPlus1(SOCKET s1, SOCKET s2, SOCKET s3, SOCKET s4);

// 외부에서 접근 가능한 배열
extern SOCKETINFO *SocketInfoArray[MAX_SOCKETS];
extern int nTotalSockets;
