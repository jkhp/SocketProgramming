#include "TcpSelect.hpp"

#include <iostream>
#include <vector>
#include <algorithm>

#define BUFSIZE 1024

void TcpSelect::Run()
{
    InitSocket();
    BindAndListen();

    fd_set rset, allset;
    FD_ZERO(&allset);
    FD_SET(GetListenSocket(), &allset);

    std::vector<SOCKET> clients; // 연결된 클라이언트 소켓들을 저장할 벡터

#ifdef _WIN32
    int maxfd = 0; // Windows에서는 select의 첫 번째 인자가 무시되므로 0으로 설정
#else
    int maxfd = GetListenSocket(); // 리눅스에서는 최대 소켓 번호를 관리
#endif

    while (IsRunning())
    {
        rset = allset; // 매 루프마다 읽기 셋을 초기화

#ifdef _WIN32
        int nfds = 0; // Windows에서는 첫 번째 인자가 무시되므로 0으로 설정
#else
        int nfds = maxfd + 1; // 리눅스에서는 최대 소켓 번호 + 1
#endif

        int nready = select(nfds, &rset, nullptr, nullptr, nullptr); // writeset은 꺼둠 (cpu 과부화 방지)
        if (nready == SOCKET_ERROR)
        {
            err_display("select()");
            continue;
        }
        if (nready == 0)
            continue;

        // 새로운 클라이언트 접속 처리
        if (FD_ISSET(GetListenSocket(), &rset))
        {
            sockaddr_storage caddr{};
            socklen_t clen{};

            SOCKET client = AcceptClient(caddr, clen);
            if (client != INVALID_SOCKET)
            {
                PrintClientInfo(caddr, clen);

                FD_SET(client, &allset);   // 읽기 셋에 클라이언트 소켓 추가
                clients.push_back(client); // 벡터에 클라이언트 소켓 추가

#ifndef _WIN32
                if (static_cast<int>(client) > maxfd)
                    maxfd = static_cast<int>(client); // 최대 소켓 번호 갱신
#endif
            }
            if (--nready <= 0)
                continue; // 처리할 소켓이 더 없음
        }

        // 기존 클라이언트 데이터 처리
        for (size_t i = 0; i < clients.size() && nready > 0; ++i)
        {
            SOCKET s = clients[i];
            if (!FD_ISSET(s, &rset))
                continue;

            char buf[BUFSIZE + 1];
            int n = recv(s, buf, BUFSIZE, 0);

            if (n <= 0) // 클라이언트 종료 또는 에러
            {
                close_socket(s);
                FD_CLR(s, &allset); // 읽기 셋에서 제거

                clients.erase(clients.begin() + i); // 벡터에서 소켓 제거

#ifndef _WIN32
                if (!clients.empty())
                {
                    int curmax = static_cast<int>(GetListenSocket());
                    for (SOCKET cs : clients)
                    {
                        if (static_cast<int>(cs) > curmax)
                            curmax = static_cast<int>(cs);
                    }
                }
                else
                {
                    maxfd = static_cast<int>(GetListenSocket());
                }
#endif
            }
            else
            {
                // 간단한 에코 서버 동작
                int sent = send(s, buf, n, 0);
                if (sent == SOCKET_ERROR)
                {
                    err_display("send()");
                    close_socket(s);
                    FD_CLR(s, &allset);
                    clients.erase(std::remove(clients.begin(), clients.end(), s), clients.end());

#ifndef _WIN32
                    if (!clients.empty())
                    {
                        int curmax = static_cast<int>(GetListenSocket());
                        for (SOCKET cs : clients)
                            if (static_cast<int>(cs) > curmax)
                                curmax = static_cast<int>(cs);
                        maxfd = curmax;
                    }
                    else
                    {
                        maxfd = static_cast<int>(GetListenSocket());
                    }
#endif
                }
            }
            --nready;
        } // end for(클라이언트 데이터 처리)
    } // end while

    for (SOCKET s : clients) // 모든 클라이언트 소켓 닫기
    {
        close_socket(s);
        FD_CLR(s, &allset);
    }
    clients.clear();
}
