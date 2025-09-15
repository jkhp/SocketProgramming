#include "WireProtocol.h"

void BuildPacket(Packet &packet, const uint8_t *data, uint16_t len)
{
    packet.len = htons(len); // 네트워크 바이트 오더로 변환
    memcpy(packet.data, data, len);
}

uint16_t ReturnLength(const Packet &packet)
{
    return ntohs(packet.len); // 호스트 바이트 오더로 변환
}

void SendPacket(int sock, const Packet &packet)
{
    int totalLen = sizeof(packet.len) + packet.len; // 전송할 전체 길이

    int sentLen = send(sock, (char *)&packet, totalLen, 0);
}

void ReceivePacket(int sock)
{
}