#include "../Common.h"
#include <iostream>
using namespace std;

char *SERVERIP = (char *)"127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE 512

int main(int argc, char *argv[])
{
    int retval;

    // 명령행 인수가 있으면 IP 주소로 사용
    if (argc > 1)
        SERVERIP = argv[1];

    // 소켓 생성
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET)
        err_quit("socket()");

    // 소켓 주소 구조체 초기화
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr); // IP 주소를 이진 형식으로 변환하여 저장
    serveraddr.sin_port = htons(SERVERPORT);            // 포트 번호를 네트워크 바이트 순서로 변환하여 저장

    // 데이터 통신에 사용할 변수
    struct sockaddr_in peeraddr;
    socklen_t addrlen; // 주소의 길이를 나타내기 위한 정수형 데이터 타입.
    char buf[BUFSIZE + 1];
    int len;

    while (1)
    {
        // 데이터 입력
        printf("보낼 데이터 : ");
        fgets(buf, BUFSIZE, stdin);
        len = strlen(buf);
        if (len == 0)
            break; // 입력이 없으면 종료

        // 데이터 전송
        retval = sendto(sock, buf, len, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
        if (retval == SOCKET_ERROR)
            err_display("sendto()");

        // 데이터 수신
        addrlen = sizeof(peeraddr);
        retval = recvfrom(sock, buf, BUFSIZE, 0, (struct sockaddr *)&peeraddr, &addrlen);
        if (retval == SOCKET_ERROR)
            err_display("recvfrom()");
        else if (retval == 0)
            break; // 연결 종료

        // 송신자 IP 주소 체크
        if (memcmp(&peeraddr, &serveraddr, sizeof(peeraddr)) != 0)
        {
            printf("서버와 연결된 클라이언트가 아닙니다.\n");
            break;
        }

        // 받은 데이터 출력
        buf[retval] = '\0';
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peeraddr.sin_addr, addr, sizeof(addr));
        printf("[UDP/%s:%d] %s\n", addr, ntohs(peeraddr.sin_port), buf);
    }

    close(sock);
    return 0;
}