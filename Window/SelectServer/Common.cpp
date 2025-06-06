#include "Common.h"

// 소켓 함수 오류 출력 후 종료
void err_quit(const char *msg)
{
    char *msgbuf = strerror(errno);
    printf("[%s] %s\n", msg, msgbuf);
    exit(1);
}

// 소켓 함수 오류 출력
void err_display(const char *msg)
{
    char *msgbuf = strerror(errno);
    printf("[%s] %s\n", msg, msgbuf);
}

// 소켓 함수 오류 출력
void err_display(int errcode)
{
    char *msgbuf = strerror(errcode);
    printf("[오류] %s\n", msgbuf);
}

void SetNonBlocking(SOCKET sock)
{
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1)
        err_quit("fcntl(F_GETFL)");

    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
        err_quit("fcntl(F_SETFL)");
}