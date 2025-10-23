#include "WireProtocol.h"

// c->s, s->c 공통
void SendPacket(int sock, Packet &packet)
{
    int len = packet.blockNum * 8; // 암호문을 전송하기 위한 길이(바이트)

    packet.len = htons((uint16_t)packet.len);           // 네트워크 바이트 오더로 변환
    packet.blockNum = htons((uint16_t)packet.blockNum); // data는 DES에서 직렬화해서 넣어줌(memcpy)

    int totalLen = sizeof(packet.len) + sizeof(packet.blockNum) + len; // 전송할 전체 길

    int totalsent = 0;
    int sent;

    // 전송 루프
    while (totalsent < totalLen)
    {
        sent = send(sock, (char *)&packet + totalsent, totalLen - totalsent, 0);
        if (sent == SOCKET_ERROR)
        {
            err_display("send()");
            return;
        }
        totalsent += sent;
    }
    printf("전송 성공, %d 바이트 전송\n", totalsent);
}

int ReceivePacket(int sock, Packet &packet)
{
    int received = 0;

    int headerSize = sizeof(packet.len) + sizeof(packet.blockNum);
    char *headerPtr = (char *)&packet; // 헤더의 시작 주소 len부터 저장, recv()는 char* 타입을 요구

    // 헤더(길이 정보) 먼저 받기
    while (received < headerSize)
    {
        int ret = recv(sock, headerPtr + received, headerSize - received, 0);
        // non-blocking 소켓을 사용하면 EAGAIN, EWOULDBLOCK 처리 필요
        if (ret == SOCKET_ERROR)
        {
            err_display("recv()");
            return -1;
        }
        else if (ret == 0)
        {
            break; // 정상 종료
        }
        received += ret;
    }

    // 네트워크 바이트 오더를 호스트 바이트 오더로 변환
    packet.len = (int)ntohs(packet.len);
    packet.blockNum = (int)ntohs(packet.blockNum);

    if (packet.len == 0 || packet.blockNum == 0)
        return 1; // 데이터가 없는 경우

    // 실제 데이터 받기
    int dataSize = packet.blockNum * 8; // 암호화 후 데이터 크기(바이트)
    char *dataPtr = packet.data;        // 데이터의 시작 주소
    received = 0;                       // 받은 바이트 수 초기화

    while (received < dataSize)
    {
        int ret = recv(sock, dataPtr + received, dataSize - received, 0);
        if (ret == SOCKET_ERROR)
        {
            err_display("recv()");
            return -1;
        }
        else if (ret == 0)
        {
            break; // 정상 종료
        }
        received += ret;
    }

    // 받은 데이터 출력
    printf("\n수신 성공, len: %d, blockNum: %d \n전송 받은 데이터: ", packet.len, packet.blockNum);
    fwrite(packet.data, 1, dataSize, stdout); // 받은 데이터 출력, null 문자 포함을 위해
    printf("\n");

    return 1;
}