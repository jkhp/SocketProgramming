#include "../Common.hpp"
#include "AES_HMAC.hpp"
#include <iostream>
#include <vector>
#include <string>

char *SERVERIP = (char *)"127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE 1024

int main(int argc, char *argv[])
{

    int retval; // int형 소켓 함수의 결과를 받는 변수

    // 명령행 인수가 있으면 ip 주소로 사용
    if (argc > 1)
        SERVERIP = argv[1]; // [0]은 프로그램 실행 명령, 1부터 입력받는 자료

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        err_quit("WSAStartup()");
        return 1;
    }

    // 소켓 생성
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        err_quit("socket()"); // 실패시 INVAILD_SOCKET은 -1을 반환
        WSACleanup();
        return 1;
    }

    // connect()
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = connect(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
    {
        err_quit("connect()");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "TCP 서버 연결 성공" << std::endl;
    std::cout << "전송은 메세지 입력 후 Enter, 종료는 빈 줄 혹은 exit 후 Enter" << std::endl;

    std::string input;

    while (1)
    {
        std::cout << "-> ";

        if (!std::getline(std::cin, input))
            break;
        if (input.empty() || input == "exit")
            break;

        std::vector<unsigned char> plaintext(input.begin(), input.end());
        std::vector<unsigned char> cipher;

        try
        {
            cipher = EtmCipher::Encrypt(plaintext, "password", 200000);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Encrypt 실패" << e.what() << std::endl;
            continue;
        }

        std::cout << "[암호문] ";
        fwrite(cipher.data(), 1, cipher.size(), stdout);
        std::cout << std::endl;

        size_t total_sent = 0;
        while (total_sent < cipher.size())
        {
            int sent = send(sock, reinterpret_cast<const char *>(cipher.data() + total_sent),
                            static_cast<int>(cipher.size() - total_sent), 0);

            if (sent == SOCKET_ERROR)
            {
                std::cerr << "send() 실패" << WSAGetLastError() << "\n";
                closesocket(sock);
                WSACleanup();
                return 1;
            }
            total_sent += sent;

            // echo 암호문 수신
            unsigned char recvBuf[4096];
            int recvLen = recv(sock, reinterpret_cast<char *>(recvBuf),
                               sizeof(recvBuf), 0);

            if (recvLen == SOCKET_ERROR)
            {
                std::cerr << "recv() 실패 " << WSAGetLastError() << "\n";
                closesocket(sock);
                WSACleanup();
                return 1;
            }
            if (recvLen == 0)
            {
                std::cout << "서버 연결 종료\n";
                closesocket(sock);
                WSACleanup();
                return 0;
            }

            std::vector<unsigned char> cipherEcho(recvBuf, recvBuf + recvLen);

            std::cout << "[echo 복호화 전] ";
            std::fwrite(cipherEcho.data(), 1, cipherEcho.size(), stdout);
            std::printf("\n");

            // 복호화
            std::vector<unsigned char> plainEcho;
            try
            {
                plainEcho = EtmCipher::Decrypt(cipherEcho, "password", 200000);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Decrypt 실패: " << e.what() << "\n";
                closesocket(sock);
                WSACleanup();
                return 1;
            }

            std::string textResult(plainEcho.begin(), plainEcho.end());
            std::cout << "[echo 복호화 후] "
                      << textResult << "\n\n";
        }
    }
    closesocket(sock);
    WSACleanup();
    return 0;
}