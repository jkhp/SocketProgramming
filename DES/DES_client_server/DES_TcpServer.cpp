#include "Common.h"
#include "../DES.h"
#include "WireProtocol.h"

#define SERVERPORT 9000
#define BUFSIZE 1024
volatile int running = 1;

typedef struct
{
    char ipAddr[INET_ADDRSTRLEN];
    uint16_t port;
    int display = NULL; // client에게 보여줄 번호, 매 작동마다 새롭게 부여 및 초기화
} acceptClientInfo;

void SocketBindAndListen(SOCKET &listen_sock);
void keyLoop(char *key);

int main(int argc, char *argv[])
{
    printf("TCP 서버 시작\n");
    int retval;

    SOCKET listen_sock;

    SocketBindAndListen(listen_sock);

    SOCKET client_sock;
    struct sockaddr_in clientaddr;
    socklen_t addrlen;
    Packet packet;

    printf("클라이언트 접속 대기...\n");

    // 데이터 송수신 및 복호화
    while (1)
    {
        addrlen = sizeof(clientaddr);
        client_sock = accept(listen_sock, (struct sockaddr *)&clientaddr, &addrlen);
        if (client_sock == INVALID_SOCKET)
        {
            err_display("accept()");
            break;
        }

        char addr[INET_ADDRSTRLEN];
        uint16_t port = ntohs(clientaddr.sin_port);                   // client port 번호
        inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr)); // client IP 주소

        printf("\n[TCP 서버] 클라이언트 접속 : IP 주소 = %s, 포트 번호 = %d\n", addr, port);

        while (1)
        {
            retval = ReceivePacket(client_sock, packet);

            if (retval == SOCKET_ERROR)
            {
                // recv() 에러
                printf("수신 중 오류 발생\n");
                break;
            }
            else if (retval == 0)
            {
                // 클라이언트가 정상적으로 연결 종료
                printf("클라이언트 연결 종료 요청\n");
                break;
            }
            if (packet.len == 0)
                continue;

            // 복호화 데이터 출력
            char key[9];                 // 8바이트 키
            keyLoop(key);                // key 입력 루프
            DES_Decryption(packet, key); // 중간에 \0이 포함될 수 있으므로 len(길이)를 같이 전달

            printf("복호화된 데이터: %s\n", packet.data);
        }
        close(client_sock);
        printf("[TCP 서버] 클라이언트 종료 : IP 주소 = %s, 포트 번호 =%d\n", addr, ntohs(clientaddr.sin_port));
    }
    close(listen_sock);
    return 0;
}

void SocketBindAndListen(SOCKET &listen_sock)
{
    int retval;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET)
        err_quit("socket()");

    struct sockaddr_in serveraddr;

    memset(&serveraddr, 0, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);

    retval = bind(listen_sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));

    if (retval == SOCKET_ERROR)
        err_quit("bind()");

    retval = listen(listen_sock, SOMAXCONN);
    if (retval == SOCKET_ERROR)
        err_quit("listen()");
}

void keyLoop(char *key)
{
    // key 입력 루프
    while (1)
    {
        printf("8바이트 비밀번호 입력: ");
        if (fgets(key, 9, stdin) == NULL) // 마지막 \n은 잘리고 null문자가 자동으로 추가됨, 버퍼에 남아있는 초과 문자 제거 필요
        {
            printf("fgets() 오류\n");
            continue;
        }
        else if (strlen(key) < 8 || key[7] == '\n') // 8바이트 미만 입력 시 다시 입력 받기
        {
            printf("8바이트 미만 입력, 다시 입력하세요.\n");
            continue;
        }
        else if (key[8] != '\0') // 9번째 문자가 \n이나 \0이 아니면 초과 입력
        {
            printf("8바이트 초과 입력, 다시 입력하세요.\n");
            while (getchar() != '\n') // 버퍼에 남아있는 초과 문자 제거
                continue;
        }
        else // 정상 입력 시
        {
            while (getchar() != '\n') // 버퍼에 남아있는 초과 문자 제거
                continue;
            break; // while 탈출
        }
    }
}