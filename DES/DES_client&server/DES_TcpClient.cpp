#include "Common.h"
#include "../DES.h"

char *SERVERIP = (char *)"127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE 512

volatile int running = 1;

void RecvThread(void *arg)
{
    char buf[BUFSIZE + 1];
    int retval;

    while (running)
    {
        retval = recv(sock, buf, BUFSIZE, 0); // sock 부분 수정필요
        if (retval == SOCKET_ERROR)
        {
            err_display("recv()");
            break;
        }
        else if (retval == 0)
            break;

        buf[retval] = '\0';
        printf("[TCP 클라이언트] %d바이트를 받았습니다. \n", retval);
        printf("[받은 데이터] %s\n", buf);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int retval; // int형 소켓 함수의 결과를 받는 변수

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

    // 데이터 통신에 사용할 변수
    DesContext context; // DES 컨텍스트 구조체
    char buf[BUFSIZE + 1];
    int len;

    // 수신 스레드 시작
    pthread_t tid;
    if (pthread_create(&tid, NULL, RecvThread, NULL) != 0)
    {
        perror("pthread_create");
        close(sock);
        return 1;
    }

    // 서버에 데이터 송신
    while (1)
    { // 데이터 입력
        printf("\n[보낼 데이터] ");
        if (fgets(buf, BUFSIZE + 1, stdin) == NULL) // 일반적 표준입력에는 stdin을 사용
        {
            printf("fgets() 오류\n");
            break;
        }
        else if (strlen(buf) == 0)
        {
            printf("입력 오류, 데이터를 다시 입력하세요.\n");
            continue;
        }
        else // buf 정상 입력 시
        {
            char key[9]; // 8바이트 + null문자, 반복문으로 buf 입력(루프)마다 키를 새로 입력 받음(지역변수 사용)
            while (1)    // 비밀번호 입력 루프
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

            // context.len은 DES_Encryption()에서 저장 됨(blockNum도 동일)
            len = (int)strlen(buf); // size_t를 int형으로 변환
            if (buf[len - 1] == '\n')
                buf[len - 1] = '\0'; // \n을 null으로 바꿔주기(fgets()에서 enter로 입력을 종료해서)
            if (strlen(buf) == 0)
                continue; // 빈 문자열이면 다시 입력 받기;

            // ** buf[] 암호화, 구조체에 각 요소 저장, 암호화된 buf[]를 구조체에 직렬화(memcpy) **
            const char *chiper = DES_Encryption(buf, key, context); // 중간에 \0이 포함될 수 있으므로 len(길이)를 같이 전달, **서버 코드도 변경해야함**
            // 구조체 직렬화

            // 데이터 보내기 -> **직렬화된 구조체 보내기**
            retval = send(sock, chiper, context.len, 0); // send()는 실제 전송된 바이트 수를 반환(덜 보내지는 경우도 존재)
            if (retval == SOCKET_ERROR)
            {
                err_display("send()");
                break;
            }
            printf("[TCP 클라이언트] %d바이트를 보냈습니다. \n", retval);
        }
    }

    running = 0;
    pthread_join(tid, NULL);
    close(sock);
    printf("TCP 클라이언트 종료\n");
    return 0;
}
