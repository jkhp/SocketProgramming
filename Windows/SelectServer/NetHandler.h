#pragma once

#include "Common.h"
#include "SocketManager.h"

// tcp
void AcceptNewTcpClient(SOCKET listen_sock, bool isIpv6); // accept()
void HandleTcpReceive(int index);                         // recv()
void HandleTcpSend(int index);                            // send()

// udp
void HandleUdpIpv4(SOCKET sock); // recvfrom() + sendto()
void HandleUdpIpv6(SOCKET sock); // recvfrom() + sendto()
