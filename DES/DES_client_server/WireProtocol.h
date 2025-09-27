#pragma once
#include <stdint.h>
#include <stdio.h>
#include "Common.h"
// #include <sys/socket.h>
// #include <winsock2.h>

#define MAX_SIZE 1024

#pragma pack(push, 1) // 구조체 멤버 간의 패딩 제거
typedef struct
{
    int len;
    int blockNum;
    char data[MAX_SIZE];
} Packet;
#pragma pack(pop) // 원래 패딩 설정으로 복원

void SendPacket(int sock, Packet &packet);
int ReceivePacket(int sock, Packet &packet);