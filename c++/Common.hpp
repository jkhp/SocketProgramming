#pragma once

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") // 링커 지시문, 링크가 필요한 라이브러리
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

enum class MsgType : std::uint16_t
{
    Chat = 1,
    ListReq = 2,
    ListResp = 3,
    Error = 4,
};

#pragma pack(push, 1)
struct PacketHeader
{
    std::uint32_t payloadLen;
    std::uint32_t to;
    std::uint32_t from;
    std::uint16_t type;
    std::uint16_t reserved;
};
#pragma pack(pop)

static constexpr std::size_t PacketHeaderSize = sizeof(PacketHeader);

inline std::vector<char> BuildPacket(std::uint32_t toSid, std::uint32_t fromSid, MsgType type, const char *payload, std::size_t payloadLen, std::uint16_t reserved = 0)
{
    std::vector<char> out;
    out.resize(PacketHeaderSize + payloadLen);

    PacketHeader header{};
    header.payloadLen = htonl(static_cast<std::uint32_t>(payloadLen));
    header.to = htonl(toSid);
    header.from = htonl(fromSid);
    header.type = htons(static_cast<std::uint16_t>(type));
    header.reserved = htons(reserved);

    std::memcpy(out.data(), &header, PacketHeaderSize);
    if (payloadLen > 0 && payload)
        std::memcpy(out.data() + PacketHeaderSize, payload, payloadLen);

    return out;
}

inline void err_quit(const char *msg)
{
#ifdef _WIN32
    DWORD err = WSAGetLastError();
    std::fprintf(stderr, "%s: %d\n", msg, err);
    std::exit(1);
#else
    std::perror(msg);
    std::exit(1);
#endif
}

inline void err_display(const char *msg)
{
#ifdef _WIN32
    DWORD err = WSAGetLastError();
    std::fprintf(stderr, "%s: %d\n", msg, err);
#else
    std::perror(msg);
#endif
}

inline void close_socket(SOCKET sock)
{
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}