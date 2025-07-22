#define _CRT_SECURE_NO_WARNINGS         // 구형 C 함수 사용 시 경고 끄기
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 구형 소켓 API 사용 시 경고 끄기

#pragma once // 중복include 방지

#include <winsock2.h> // Windows 소켓 API
#include <ws2tcpip.h> // Windows 소켓 API 확장

#include <tchar.h>  // _T(), ...
#include <stdio.h>  // printf(), ...
#include <stdlib.h> // exit(), ...
#include <string.h> // strncpy(), ...

#pragma comment(lib, "ws2_32") // ws2_32.lib (정적 링커 라이브러리)

// 공통 에러 처리 함수
void err_quit(const char *msg);    // 소켓 함수 오류 출력 후 종료
void err_display(const char *msg); // 소켓 함수 오류 출력
void err_display(int errcode);     // 소켓 함수 오류 출력 (오류 코드로)

void SetNonBlocking(SOCKET sock); // 소켓을 넌블로킹 모드로 설정