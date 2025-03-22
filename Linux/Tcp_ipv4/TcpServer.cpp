#include "../Common.h"
#include <iostream>
using namespace std;

#define SERVERPORT 9000
#define BUFSIZE 512

// SOCKET을 서버 소켓으로 설정(bind())하고, 서버 소켓을 수동 소켓으로 전환(listen())한다. 클라이언트의 연결 요청이 발생하면, 수락(accept())으로 활성 소켓을 반환한다. 활성 소켓이 클라이언트와 데이터 송수신에 사용됨.

void *tcpServerIpv4(void *arg) // 포인터 반환형 -> 멀티스레드 함수를 사용하기 위해서, 매개변수 포인터 -> 유연성(타입 캐스팅해서 사용)
{
	int retval; // 정수형을 반환하는 소켓 함수의 값을 저장하는 변수(bind() 등)

	// 소켓 생성
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0); // socket()의 반환 값은 양수, -1으로 소켓 디스크립터이다. 디스크립터는 리소스를 추상적으로 참조하는데 사용되는 고유한 값. 윈도우에서는 handle이라고 함.
	if (listen_sock == INVALID_SOCKET)
		err_quit("socket()");

	// bind()
	// 소켓에 IP와 PORT를 할당하는 역할을 함. 반환 값은 성공 시 0, 실패 시 -1. 오류코드는 errno에 저장됨. errno는 전역변수로 errno.h에 정의되어 있으며 스레드마다 개별 유지되는 변수임. 매개변수로 소켓 주소를나타내는 일반적인 자료형 struct sockaddr *을 요구함. 그러나 serveraddr의 자료형은 struct sockaddr_in
	struct sockaddr_in serveraddr;													// ipv4 소켓 구조체 생성
	memset(&serveraddr, 0, sizeof(serveraddr));										// memset으로 0초기화
	serveraddr.sin_family = AF_INET;												// 주소체계를 저장하는 sin_family
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);									// IP주소 체계를 의미하는 sin_.addr 구조체의 s_addr에 htonl함수를 이용하여 주소를 넣음, INADDR_ANY는 0.0.0.0 상수로 모든 사용 가능한 네트워크 인터페이스를 바인딩함. 즉, 특정 IP주소가 아닌 시스템의 모든 IP주소에서 수신 대기 가능.
	serveraddr.sin_port = htons(SERVERPORT);										// short타입의 포트번호
	retval = bind(listen_sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)); // serveraddr의 자료형은 struct sockaddr_in이라 형변환이 필요. 가져온 주소를 형 변환시킴. 포인터(&)를 가져와야 포인터 형식으로 형변환 가능. (struct sockaddr*)&serveraddr => 1. &serveraddr : 주소를 가져온다 2. (struct sockaddr*)
	// 형 변환을 하는 이유 : 함수가 다른 주소체계를 수용하기 위해 대표 자료형 (struct sockaddr *)을 사용함. 근데 구조체 맴버가 바뀌네? sa_data[14]에 port랑 addr이 자료가 변환되어서 들어감.
	if (retval == SOCKET_ERROR)
		err_quit("bind()");

	// listen()
	// tcp 서버 소켓에서 클라이언트의 연결 요청을 기다리는 함수, socket(), bind()를 호출한 후 사용됨. bind()와 동일하게 errno 사용.
	retval = listen(listen_sock, SOMAXCONN); // backlog(listen() 상태에서 연결 대기열(queue)에 저장할 수 있는 최대 연결 요청 개수)를 SOMAXCONN으로 설정하면 시스템이 지원할 수 있는 최대 연결요청 수를 사용가능
	if (retval == SOCKET_ERROR)
		err_quit("listen()");

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	struct sockaddr_in clientaddr;
	socklen_t addrlen; // 주소의 길이를 나타내기 위한 정수형 데이터 타입.
	char buf[BUFSIZE + 1];

	while (1)
	{
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (struct sockaddr *)&clientaddr, &addrlen); // addrlen 변수(출력 매개변수)를 사용하는 이유 -> addrlen이 accept() 실행 전후가 다를 수 있기 때문. sockaddr_storade 구조체를 사용하는 경우때문에		// listen_sock이 필요한 이유 -> 1. 연결 대기 소켓을 식별 2. 클라이언트나 요청에 따라 서버 소켓을 독립적으로 관리 및 제어 3. 비동기 동작 수행을 위해
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			break;
		}

		// 접속한 클라이언트 정보 출력
		char addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
		printf("\n[TCP 서버] 클라이언트 접속 : IP 주소 = %s, 포트 번호 = %d\n", addr, ntohs(clientaddr.sin_port));

		// 클라이언트와 데이터 통신
		while (1)
		{ // 데이터 받기
			retval = recv(client_sock, buf, BUFSIZE, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("recv()");
				break;
			}
			else if (retval == 0)
				break;

			// 받은 데이터 출력
			buf[retval] = '\0';
			printf("[TCP/%s:%d] %s\n", addr, ntohs(clientaddr.sin_port), buf);

			// 데이터 보내기
			retval = send(client_sock, buf, retval, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				break;
			}
		}

		// 소켓 닫기
		close(client_sock);
		printf("[TCP 서버] 클라이언트 종료 : IP 주소 = %s, 포트 번호 =%d\n", addr, ntohs(clientaddr.sin_port));
	}

	// 소켓 닫기
	close(listen_sock);
	return 0; // 자동 형변환( (void *)0 )으로 에러 x, NULL(null 포인터)을 반환하는 것과 동일
}

void *tcpServerIpv6(void *arg)
{
	int retval;

	// 소켓 생성
	SOCKET listen_sock = socket(AF_INET6, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
		err_quit("socket()");

	// 듀얼 스택(ipv4, ipv6 동시지원)을 끈다(호환성을 위해서). [window는 꺼진게 기본 값. linux는 os마다 다름]
	int no = 1;
	setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));

	// bind
	struct sockaddr_in6 serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin6_family = AF_INET6;
	serveraddr.sin6_addr = in6addr_any; // 헤더파일의 변수, 값은 ::
	serveraddr.sin6_port = htons(SERVERPORT);
	retval = bind(listen_sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
		err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
		err_quit("listen()");

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	struct sockaddr_in6 clientaddr;
	socklen_t addrlen;
	char buf[BUFSIZE + 1];

	// 클라이언트와 통신
	while (1)
	{
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (struct sockaddr *)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			break;
		}

		// 접속한 클라이언트 정보 출력
		char ipaddr[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &clientaddr.sin6_addr, ipaddr, sizeof(ipaddr));
		printf("\n[TCP 서버] 클라이언트 접속 : IP 주소 = %s, 포트 번호 = %d\n", ipaddr, ntohs(clientaddr.sin6_port));

		// 클라이언트와 데이터 통신
		while (1)
		{
			// 데이터 받기
			retval = recv(client_sock, buf, BUFSIZE, 0);

			if (retval == SOCKET_ERROR)
			{
				err_display("recv()");
				break;
			}
			else if (retval == 0)
				break;

			// 받은 데이터 출력
			buf[retval] = '\0';
			printf("%s", buf);
		}

		// 소켓 닫기
		close(client_sock);
		printf("[TCP 서버] 클라이언트 종료: IP 주소 = %s, 포트 번호 = %d\n", ipaddr, ntohs(clientaddr.sin6_port));
	}

	// 소켓 닫기
	close(listen_sock);
	return 0;
}
