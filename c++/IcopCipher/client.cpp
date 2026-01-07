#include "../Common.hpp"
#include "AES_HMAC.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static const char *SERVERIP = "127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE 1024

static bool SendAll(SOCKET s, const char *buf, std::size_t len)
{
    std::size_t sent = 0;
    while (sent < len)
    {
        int r = send(s, buf + sent, static_cast<int>(len - sent), 0);
        if (r <= 0)
            return false;
        sent += static_cast<std::size_t>(r);
    }
    return true;
}

static std::vector<char> EncryptChatPayload(const std::string &plain)
{
    const std::string password = "password";
    const int iterations = 200000;

    std::vector<unsigned char> in(plain.begin(), plain.end());
    std::vector<unsigned char> enc = EtmCipher::Encrypt(in, password, iterations);
    return std::vector<char>(enc.begin(), enc.end());
}

static std::string DecryptChatPayload(const std::vector<char> &cipher)
{
    const std::string password = "password";
    const int iterations = 200000;

    std::vector<unsigned char> in(cipher.begin(), cipher.end());
    std::vector<unsigned char> dec = EtmCipher::Decrypt(in, password, iterations);
    return std::string(dec.begin(), dec.end());
}

static void ReceiverLoop(SOCKET sock, std::atomic<bool> &running)
{
    std::vector<char> stream;
    stream.reserve(2048);

    char buf[BUFSIZE];

    while (running.load())
    {
        int r = recv(sock, buf, static_cast<int>(sizeof(buf)), 0);
        if (r <= 0)
            break;

        stream.insert(stream.end(), buf, buf + r);

        while (stream.size() >= PacketHeaderSize)
        {
            PacketHeader header{};
            std::memcpy(&header, stream.data(), PacketHeaderSize);

            const std::uint32_t payloadLen = ntohl(header.payloadLen);
            const MsgType type = static_cast<MsgType>(ntohs(header.type));
            const std::uint32_t fromSid = ntohl(header.from);
            // const std::uint32_t toSid     = ntohl(header.to);

            if (stream.size() < PacketHeaderSize + payloadLen)
                break;

            std::vector<char> payload(
                stream.begin() + PacketHeaderSize,
                stream.begin() + PacketHeaderSize + payloadLen);

            stream.erase(stream.begin(),
                         stream.begin() + PacketHeaderSize + payloadLen);

            if (fromSid == 0)
            {
                std::string msg(payload.begin(), payload.end());
                std::cout << msg << "\n";
            }
            else if (type == MsgType::Chat)
            {
                try
                {
                    std::string plain = DecryptChatPayload(payload);
                    std::cout << "[FROM " << fromSid << "] " << plain << "\n\n";
                }
                catch (const std::exception &e)
                {
                    std::cerr << "[Decrypt 실패] " << e.what() << "\n\n";
                }
            }
            else
            {
                std::string msg(payload.begin(), payload.end());
                std::cout << msg << "\n";
            }
        }
    }

    running.store(false);
}

int main(int argc, char *argv[])
{
    int retval;

    WSADATA wsa;
    retval = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (retval != 0)
    {
        err_quit("WSAStartup()");
        return 1;
    }

    // 소켓 생성
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        err_quit("socket()");
        WSACleanup();
        return 1;
    }

    // connect()
    sockaddr_in serveraddr;
    std::memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVERPORT);

    retval = inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr);
    if (retval != 1)
    {
        err_quit("inet_pton()");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    retval = connect(sock, reinterpret_cast<sockaddr *>(&serveraddr), sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
    {
        err_quit("connect()");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "TCP 서버 연결 성공\n";
    std::cout << "[Commands]  'list'   'To <SessionId>'   'exit'\n\n";

    std::atomic<bool> running{true};
    std::thread recvThread(ReceiverLoop, sock, std::ref(running));

    std::string input;
    while (running.load())
    {
        std::cout << "-> ";
        if (!std::getline(std::cin, input))
            break;

        if (input == "exit" || input.empty())
            break;

        if (input == "list")
        {
            auto packet = BuildPacket(
                0, // to
                0, // from
                MsgType::ListReq,
                nullptr,
                0);

            if (!SendAll(sock, packet.data(), packet.size()))
            {
                err_display("Send(list)");
                break;
            }
            continue;
        }

        if (input.rfind("To ", 0) == 0)
        {
            std::uint32_t toSid = 0;
            try
            {
                std::string sidStr = input.substr(3);
                if (sidStr.empty())
                    throw std::invalid_argument("empty");

                toSid = static_cast<std::uint32_t>(std::stoul(sidStr));
            }
            catch (...)
            {
                std::cout << "형식 오류: To <SessionId>\n";
                continue;
            }

            std::cout << "(msg)> ";
            std::string msg;
            if (!std::getline(std::cin, msg))
                break;

            std::vector<char> enc;
            try
            {
                enc = EncryptChatPayload(msg);
            }
            catch (const std::exception &e)
            {
                std::cerr << "[Encrypt 실패] " << e.what() << "\n";
                continue;
            }

            std::vector<char> packet = BuildPacket(
                toSid, // to
                0,     // from
                MsgType::Chat,
                enc.empty() ? nullptr : enc.data(),
                enc.size());

            if (!SendAll(sock, packet.data(), packet.size()))
            {
                err_display("send(chat)");
                break;
            }
            continue;
        }

        std::cout
            << "다시 입력해주세요.\n"
            << "commands:\n"
            << "  list\n"
            << "  To <SessionId>\n"
            << "  exit\n\n";
    }

    running.store(false);
    shutdown(sock, SD_BOTH);
    closesocket(sock);

    if (recvThread.joinable())
        recvThread.join();

    WSACleanup();
    return 0;
}