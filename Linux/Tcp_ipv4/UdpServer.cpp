#include "../Common.h"
#include <iostream>
using namespace std;

#define SERVERPORT 9000
#define BUFSIZE 512

void *udpServerIpv4(void *arg)
{
	int retval;

	// 소켓 생성
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET)
		err_quit("socket()");

	// bind()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
		err_quit("bind()");

	// 데이터 통신에 사용할 변수
	struct sockaddr_in clientaddr;
	socklen_t addrlen; // 주소의 길이를 나타내기 위한 정수형 데이터 타입.
	char buf[BUFSIZE + 1];

	while (1)
	{
		// 데이터 수신
		addrlen = sizeof(clientaddr);
		retval = recvfrom(sock, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &addrlen);
		if (retval == SOCKET_ERROR)
		{
			err_display("recvfrom()");
			break;
		}
		else if (retval == 0)
			break;
	}

	// 받은 데이터 출력
	buf[retval] = '\0';
	char addr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
	printf("[UDP/%s:%d] %s\n", addr, ntohs(clientaddr.sin_port), buf);

	// 데이터 보내기
	retval = sendto(sock, buf, retval, 0, (struct sockaddr *)&clientaddr, addrlen);
	if (retval == SOCKET_ERROR)
	{
		err_display("sendto()");
		break;
	}

	// 소켓 닫기
	retval = close(sock);
	if (retval == SOCKET_ERROR)
		err_display("close()");
	return 0; // 포인터 반환형 함수에서 NULL을 반환하는 것과 동일한 의미.
			  // 포인터를 반환하는 함수에서 NULL을 반환하는 것은 포인터가 가리키는 메모리 주소가 유효하지 않음을 나타내는 것이다.
			  // 즉, 포인터가 가리키는 메모리 주소가 NULL이면 해당 메모리 주소에 접근할 수 없다는 것을 의미한다.
			  // 따라서 포인터를 반환하는 함수에서 NULL을 반환하는 것은 에러를 나타내는 것이다.
			  //  자동 형변환((void *)0)으로 에러 x, NULL(null 포인터)을 반환하는 것과 동일
}

void *udpServerIpv6(void *arg)
{
	int retval;
	// 소켓 생성
	SOCKET sock = socket(AF_INET6, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET)
		err_quit("socket()");

	// 듀얼 스택(ipv4, ipv6 동시지원)을 끈다(호환성을 위해서). [window는 꺼진게 기본 값. linux는 os마다 다름]
	int no = 1;
	setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));

	// bind()
	struct sockaddr_in6 serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin6_family = AF_INET6;
	serveraddr.sin6_addr = in6addr_any;
	serveraddr.sin6_port = htons(SERVERPORT);
	retval = bind(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
		err_quit("bind()");

	// 데이터 통신에 사용할 변수
	struct sockaddr_in6 clientaddr;
	socklen_t addrlen; // 주소의 길이를 나타내기 위한 정수형 데이터 타입.
	char buf[BUFSIZE + 1];
	while (1)
	{
		// 데이터 수신
		addrlen = sizeof(clientaddr);
		retval = recvfrom(sock, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &addrlen);
		if (retval == SOCKET_ERROR)
		{
			err_display("recvfrom()");
			break;
		}
		else if (retval == 0)
			break;
	}

	// 받은 데이터 출력
	buf[retval] = '\0';
	char addr[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &clientaddr.sin6_addr, addr, sizeof(addr));
	printf("[UDP/%s:%d] %s\n", addr, ntohs(clientaddr.sin6_port), buf);

	// 데이터 보내기
	retval = sendto(sock, buf, retval, 0, (struct sockaddr *)&clientaddr, addrlen);
	if (retval == SOCKET_ERROR)
	{
		err_display("sendto()");
		break;
	}

	// 소켓 닫기
	retval = close(sock);
	if (retval == SOCKET_ERROR)
		err_display("close()");
	return 0; // 포인터 반환형 함수에서 NULL을 반환하는 것과 동일한 의미 (자동 형변환((void *)0)으로 에러 x, NULL(null 포인터)을 반환하는 것과 동일)
			  // 포인터를 반환하는 함수에서 NULL을 반환하는 것은 포인터가 가리키는 메모리 주소가 유효하지 않음을 나타내는 것이다.
			  // 즉, 포인터가 가리키는 메모리 주소가 NULL이면 해당 메모리 주소에 접근할 수 없다는 것을 의미한다.
			  // 따라서 포인터를 반환하는 함수에서 NULL을 반환하는 것은 에러를 나타내는 것이다.
}

int main(void)
{
	udpServerIpv4(NULL);
	return 0;
}