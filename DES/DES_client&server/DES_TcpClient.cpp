#include "Common.h"
#include "../DES.h"
#include "WireProtocol.h"

char *SERVERIP = (char *)"127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE 1024

volatile int running = 1;

void *RecvThread(void *arg)
{
    int sock = *((int *)arg);
    Packet packet;

    while (running)
    {
        int state = ReceivePacket(sock, packet);

        if (state <= 0)
        {
            running = 0;
            break; // 루프 탈출
        }
    }

    return;
}

int main(int argc, char *argv[])
{
    int retval;    // int형 소켓 함수의 결과를 받는 변수
    Packet packet; // 데이터 통신에 사용할 변수

    // 명령행 인수가 있으면 ip 주소로 사용
    if (argc > 1)
        SERVERIP = argv[1]; // [0]은 프로그램 실행 명령, 1부터 입력받는 자료

    // 소켓 생성
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        err_quit("socket()"); // 실패시 INVAILD_SOCKET은 -1을 반환

    // connect()
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = connect(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
        err_quit("connect()");

    // 수신 스레드 시작
    pthread_t tid;
    if (pthread_create(&tid, NULL, RecvThread, &sock) != 0)
    {
        perror("pthread_create");
        close(sock);
        return 1;
    }

    // 서버에 데이터 송신
    while (1)
    { // 데이터 입력
        printf("\n[보낼 데이터] ");
        if (fgets(packet.data, BUFSIZE, stdin) == NULL) // 일반적 표준입력에는 stdin을 사용
        {
            printf("fgets() 오류\n");
            break;
        }

        int inputLen = strlen(packet.data);

        // 예외처리
        if (inputLen == 0)
        {
            printf("입력 오류, 데이터를 다시 입력하세요.\n");
            continue;
        }
        else if (inputLen >= BUFSIZE - 1 || packet.data[inputLen - 1] != '\n') // BUFSIZE 초과 입력 시
        {
            printf("입력 데이터가 너무 깁니다. 최대 %d바이트까지 입력 가능합니다.\n", BUFSIZE - 1);
            while (getchar() != '\n') // 버퍼에 남아있는 초과 문자 제거
                continue;
            continue; // 다시 입력 받기
        }
        else // buf 정상 입력 시
        {
            char key[9]; // 8바이트 + null문자, 반복문으로 buf 입력(루프)마다 키를 새로 입력 받음(지역변수 사용)

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

            // 개행 \0 처리
            if (packet.data[inputLen - 1] == '\n')
            {
                packet.data[inputLen - 1] = '\0';
                inputLen--;
            }
            if (inputLen == 0)
                continue;
            // blockNum은 DES_Encryption()에서 저장 됨
            packet.len = inputLen;

            // ** buf[] 암호화, 구조체에 각 요소 저장, 암호화된 buf[]를 구조체에 직렬화(memcpy) **
            DES_Encryption(packet, key); // 중간에 \0이 포함될 수 있으므로 len(길이)를 같이 전달, **서버 코드도 변경해야함**
            // 구조체 직렬화

            // 데이터 보내기 -> **직렬화된 구조체 보내기**
            SendPacket(sock, packet);
        }
    }

    running = 0;
    pthread_join(tid, NULL);
    close(sock);
    printf("TCP 클라이언트 종료\n");
    return 0;
}
