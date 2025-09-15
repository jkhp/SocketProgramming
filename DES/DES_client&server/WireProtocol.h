#pragma once
#include <stdint.h>
#include <stdio.h>
// #include <sys/socket.h>
#include <winsock2.h>

#define MAX_SIZE 1024

#pragma pack(push, 1) // 구조체 멤버 간의 패딩 제거
typedef struct
{
    int len;
    int blcokNum;
    char data[MAX_SIZE];
} Packet;
#pragma pack(pop) // 원래 패딩 설정으로 복원

void BuildPacket(Packet &packet, const uint8_t *data, uint16_t len);
uint16_t ReturnLength(const Packet &packet);
void SendPacket(int sock, const Packet &packet);
void ReceivePacket(int sock);