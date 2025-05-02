#include "../Common.h"

#define SERVERPORT 9000
#define BUFSIZE 512

struct SOCKETINFO
{
    SOCKET sock;
    char buf[BUFSIZE + 1];
    int recvbytes;
    int sendbytes;
};
int nTotalSockets = 0;
SOCKETINFO *SocketInfoArray[FD_SETSIZE];

// 소켓 정보 관리 함수
bool AddSocketInfo(SOCKET sock);
void RemoveSocketInfo(int nIndex);

// {최대 소켓 번호 + 1} 얻는 함수
int GetMaxFDPlus1(SOCKET s);

int main(int argc, char *argv[])
{
    int retval;

    // 소켓 생성
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET)
        err_quit("socket()");

    // bind()
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = bind(listen_sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
        err_quit("bind()");

    // listen()
    retval = listen(listen_sock, SOMAXCONN);
    if (retval == SOCKET_ERROR)
        err_quit("listen()");

    // 넌블로킹 소켓으로 전환
    int flags = fcntl(listen_sock, F_GETFL); // accept() 호출 시 클라이언트가 없으면 블로킹을 피하기 위해서서
    flags |= O_NONBLOCK;
    fcntl(listen_sock, F_SETFL, flags);

    // 데이터 통신에 사용할 변수
    fd_set rset, wset;
    int nready; // select()의 반환값. 읽기 또는 쓰기 이벤트가 발생한 소켓의 총 개수, select()에 의해 준비된(read/write) 소켓의 개수,
    SOCKET client_sock;
    struct sockaddr_in clientaddr;
    socklen_t addrlen;

    while (true)
    {
        // 소켓 셋 초기화
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_SET(listen_sock, &rset);

        for (int i = 0; i < nTotalSockets; i++)
        {
            if (SocketInfoArray[i]->recvbytes > SocketInfoArray[i]->sendbytes) // 아직 클라이언트에게 다 보내지 못한 데이터가 있으면 write 준비
                FD_SET(SocketInfoArray[i]->sock, &wset);
            else
                FD_SET(SocketInfoArray[i]->sock, &rset); // read 준비
        }

        // select()
        nready = select(GetMaxFDPlus1(listen_sock), &rset, &wset, NULL, NULL);
        if (nready == SOCKET_ERROR)
            err_quit("select()");

        // 새 클라이언트 접속 처리
        if (FD_ISSET(listen_sock, &rset))
        {
            addrlen = sizeof(clientaddr);
            client_sock = accept(listen_sock, (struct sockaddr *)&clientaddr, &addrlen);
            if (client_sock == INVALID_SOCKET)
            {
                err_display("accept()");
                break;
            }
            else
            {
                // 클라이언트 소켓도 넌블로킹으로 설정(recv(), send())에서 데이터가 준비되지 않았을 때 블로킹 없이 빠르게 return)
                int flags = fcntl(client_sock, F_GETFL);
                flags |= O_NONBLOCK;
                fcntl(client_sock, F_SETFL, flags);

                // 클라이언트 접속 정보 출력
                char addr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
                printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
                       addr, ntohs(clientaddr.sin_port));

                // 소켓 정보 저장, 실패 시 소켓 닫음
                if (!AddSocketInfo(client_sock))
                    close(client_sock);
            }
            if (--nready <= 0) // 이벤트를 처리한 뒤 nready를 감소시키면서 모든 이벤트를 소진할 수 있음.
                continue;      // 처리할 이벤트 없으면 다음 루프로
        }

        // 클라이언트 소켓에서의 데이터 수신/전송 처리
        for (int i = 0; i < nTotalSockets; i++)
        {
            SOCKETINFO *ptr = SocketInfoArray[i]; // 소켓 정보 구조체 포인터, 간결한 코드 작성을 위해 포인터 사용

            // 데이터 수신
            if (FD_ISSET(ptr->sock, &rset))
            {
                retval = recv(ptr->sock, ptr->buf, BUFSIZE, 0);
                if (retval == SOCKET_ERROR)
                {
                    err_display("recv()");
                    RemoveSocketInfo(i); // 소켓 정보 삭제
                }

                else if (retval == 0) // 정상 종료
                {
                    RemoveSocketInfo(i);
                }

                else
                {
                    ptr->recvbytes = retval;
                    ptr->buf[retval] = '\0';

                    // 클라이언트 정보 출력
                    addrlen = sizeof(clientaddr);
                    getpeername(ptr->sock, (struct sockaddr *)&clientaddr, &addrlen); // 소켓에 연결된 피어의 주소 검색, 구조체에 주소 정보 저장, 오류가 아닐 경우 0을 반환

                    // 받은 데이터 출력
                    char addr[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
                    printf("[TCP/%s:%d] %s\n", addr, ntohs(clientaddr.sin_port), ptr->buf);
                }
            }

            // 데이터 송신
            else if (FD_ISSET(ptr->sock, &wset))
            {
                retval = send(ptr->sock, ptr->buf + ptr->sendbytes,
                              ptr->recvbytes - ptr->sendbytes, 0);
                if (retval == SOCKET_ERROR)
                {
                    err_display("send()");
                    RemoveSocketInfo(i);
                }
                else
                {
                    ptr->sendbytes += retval;
                    if (ptr->recvbytes == ptr->sendbytes)    // 송신 완료하면
                        ptr->recvbytes = ptr->sendbytes = 0; // 0으로 초기화
                }
            }
        } // end for
    } // end while (true)

    // 소켓 닫기
    close(listen_sock);
    return 0;
}

// 소켓 정보 추가
bool AddSocketInfo(SOCKET sock)
{
    if (nTotalSockets >= FD_SETSIZE)
    {
        printf("[오류] 소켓 정보를 추가할 수 없습니다!\n");
        return false;
    }
    SOCKETINFO *ptr = new SOCKETINFO;
    if (ptr == NULL)
    {
        printf("[오류] 메모리가 부족합니다!\n");
        return false;
    }
    ptr->sock = sock;
    ptr->recvbytes = 0;
    ptr->sendbytes = 0;
    SocketInfoArray[nTotalSockets++] = ptr;
    return true;
}

// 소켓 정보 삭제
void RemoveSocketInfo(int nIndex)
{
    SOCKETINFO *ptr = SocketInfoArray[nIndex];

    // 클라이언트 정보 얻기
    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof(clientaddr);
    getpeername(ptr->sock, (struct sockaddr *)&clientaddr, &addrlen);

    // 클라이언트 정보 출력
    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
    printf("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
           addr, ntohs(clientaddr.sin_port));

    // 소켓 닫기
    close(ptr->sock);
    delete ptr;

    if (nIndex != (nTotalSockets - 1))                                // 삭제한 소켓이 마지막 소켓이 아닐 경우
        SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets - 1]; // 마지막 소켓을 삭제한 소켓의 위치로 이동
    --nTotalSockets;
}

// {가장 큰 소켓 번호(파일 디스크립터 번호) + 1} 얻기 -> 번호가 순차적이지 않기에 최대값까지 모두 체크, 비효율?
int GetMaxFDPlus1(SOCKET s)
{
    int maxfd = s;
    for (int i = 0; i < nTotalSockets; i++)
    {
        if (SocketInfoArray[i]->sock > maxfd)
        {
            maxfd = SocketInfoArray[i]->sock;
        }
    }
    return maxfd + 1;
}
