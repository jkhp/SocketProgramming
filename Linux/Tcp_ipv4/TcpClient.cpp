#include "../Common.h"

char *SERVERIP = (char *)"127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE 512

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
	char buf[BUFSIZE + 1];
	int len;

	// 서버와 데이터 통신
	while (1)
	{ // 데이터 입력
		printf("\n[보낼 데이터] ");
		if (fgets(buf, BUFSIZE + 1, stdin) == NULL) // 일반적 표준입력에는 stdin을 사용
			break;

		//'\n' 문자 제거
		len = (int)strlen(buf); // size_t를 int형으로 변환
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0'; // \n을 null으로 바꿔주기(fgets()에서 enter로 입력을 종료해서)
		if (strlen(buf) == 0)
			break;

		// 데이터 보내기
		retval = send(sock, buf, (int)strlen(buf), 0); // send()는 실제 전송된 바이트 수를 반환(덜 보내지는 경우도 존재)
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			break;
		}
		printf("[TCP 클라이언트] %d바이트를 보냈습니다. \n", retval);

		// 데이터 받기
		retval = recv(sock, buf, retval, MSG_WAITALL); // 반환 값 0은 상대방에서 연결 종료, -1은 err. MSG_WAITALL은 요청한 바이트를 모두 받는 설정(누락x)
		if (retval == SOCKET_ERROR)
		{
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;

		// 받은 데이터 출력
		buf[retval] = '\0';
		printf("[TCP 클라이언트] %d바이트를 받았습니다. \n", retval);
		printf("[받은 데이터] %s\n", buf);
	}

	close(sock);
	return 0;
}
