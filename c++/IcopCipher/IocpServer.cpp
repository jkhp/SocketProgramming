#include "IocpServer.hpp"
#include "AES_HMAC.hpp"
#include <iostream>

void IocpServer::Start()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        err_quit("WSAStartup()");

    iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (iocpHandle == nullptr)
        err_quit("CreateIoCompletionPort()");

    running = true;

    taskThread_.Start();
    main_thread = std::thread(&IocpServer::Run, this); // main_thread 제거?
}

void IocpServer::Stop()
{
    if (running.exchange(false))
    {
        if (listen_sock != INVALID_SOCKET)
        {
            closesocket(listen_sock);
            listen_sock = INVALID_SOCKET;
        }

        sessionRegistry.ForEach([](const std::shared_ptr<Session> &s)
                                { if (s) (void)s->Close(); }); // 세션이 남아있다면 모두 종료, Close()의 반환값 무시

        // 대기 중인 워커를 깨우기 위해 웨이크업 패킷 전송, 그렇지 않으면 GetQueuedCompletionStatus에서 영원히 대기
        if (iocpHandle)
        {
            for (size_t i = 0; i < worker_threads.size(); ++i)
            {
                PostQueuedCompletionStatus(iocpHandle, 0, exitKey, nullptr); // 모든 워커 스레드에 nullptr 전달, 스레드 내부에서 종료 처리, [check]
            }
        }

        if (main_thread.joinable())
            main_thread.join();

        for (auto &t : worker_threads)
        {
            if (t.joinable())
                t.join();
        }

        worker_threads.clear();

        // Run() IOCP 핸들 정리
        if (iocpHandle)
        {
            CloseHandle(iocpHandle);
            iocpHandle = nullptr;
        }

        taskThread_.Stop();

        WSACleanup();
    }
}

void IocpServer::Run()
{
    MakeWorkerThread();

    InitSocket();
    BindAndListen();

    if (CreateIoCompletionPort((HANDLE)listen_sock, iocpHandle, listenKey, 0) == nullptr) // listen_sock을 IOCP에 연결
        err_quit("CreateIoCompletionPort(listen_sock)");

    if (!LoadAcceptEx())
        err_quit("LoadAcceptEx()");

    SYSTEM_INFO sysInfo;     // 시스템 정보 구조체
    GetSystemInfo(&sysInfo); // 시스템 정보 가져오기

    for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors * 2; ++i) // CPU 코어 수 * 2 만큼 미리 AcceptEx 요청
        PostAccept();

    return; // main 스레드 종료, accept는 worker에서 처리
}

void IocpServer::MakeWorkerThread()
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    // CPU *2 개수만큼 워커 스레드 생성
    for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors * 2; ++i)
    {
        worker_threads.emplace_back(&IocpServer::WorkerThread, this, iocpHandle); // puch_back과 달리 emplace_back은 생성자를 직접 호출, 이동생성자 반복호출 x
    }
}

void IocpServer::InitSocket()
{
    listen_sock = socket(af, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET)
        err_quit("socket()");
}

void IocpServer::BindAndListen()
{
    if (af == AF_INET)
    {
        sockaddr_in addr{}; // 전체를 0으로 초기화
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(listen_sock, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
            err_quit("bind()");
    }
    else if (af == AF_INET6)
    {
        sockaddr_in6 addr6{};
        addr6.sin6_family = AF_INET6;
        addr6.sin6_addr = in6addr_any;
        addr6.sin6_port = htons(port);

        if (bind(listen_sock, (sockaddr *)&addr6, sizeof(addr6)) == SOCKET_ERROR)
            err_quit("bind()");
    }

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR)
        err_quit("listen()");

    std::cout << "[TCP SERVER] PORT: " << port << ", " << (af == AF_INET ? "IPv4" : "IPv6") << " START" << std::endl;
}

bool IocpServer::LoadAcceptEx() // AcceptEx 함수 포인터 로드
{
    DWORD bytes = 0;

    GUID guidAcceptEx = WSAID_ACCEPTEX;
    if (WSAIoctl(listen_sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &guidAcceptEx, sizeof(guidAcceptEx),
                 &lpfnAcceptEx, sizeof(lpfnAcceptEx),
                 &bytes, nullptr, nullptr) == SOCKET_ERROR)
    {
        err_display("WSAIOctl(AcceptEx)");
        return false;
    }

    GUID guidGetAddrs = WSAID_GETACCEPTEXSOCKADDRS;
    if (WSAIoctl(listen_sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &guidGetAddrs, sizeof(guidGetAddrs),
                 &lpfnGetAcceptExSockaddrs, sizeof(lpfnGetAcceptExSockaddrs),
                 &bytes, nullptr, nullptr) == SOCKET_ERROR)
    {
        err_display("WSAIOctl(GetAcceptExSockaddrs)");
        return false;
    }

    return true;
}

bool IocpServer::PostAccept() // AcceptEx 비동기 접속 대기 요청
{
    if (!lpfnAcceptEx || listen_sock == INVALID_SOCKET)
        return false;

    IoContext *ctx = IoContext::CreateAccept(af);
    if (!ctx)
        return false;

    DWORD bytes = 0;
    const DWORD addrLen = sizeof(sockaddr_storage) + 16;

    BOOL ok = lpfnAcceptEx(
        listen_sock,
        ctx->sock,
        ctx->acceptBuf.data(),
        0,
        addrLen,
        addrLen,
        &bytes,
        &ctx->overlapped);

    if (!ok)
    {
        int err = WSAGetLastError();
        if (err != ERROR_IO_PENDING)
        {
            err_display("AcceptEx()");
            closesocket(ctx->sock);
            delete ctx;
            return false;
        }
    }

    return true;
}

void IocpServer::HandleAcceptComplete(IoContext *ctx, DWORD cbTransferred, BOOL ok) // AcceptEx 완료 처리
{
    if (!ctx) // 정상적인 IoContext 포인터가 아니면
        return;

    if (!ok) // AcceptEx 실패 시
    {
        if (ctx->sock != INVALID_SOCKET) //  소켓 닫기
            closesocket(ctx->sock);
        delete ctx;

        if (IsRunning())  // 서버가 실행 중이면
            PostAccept(); // 다시 접속 대기 요청
        return;
    }

    if (setsockopt(ctx->sock, // AcceptEx로 생성된 소켓 옵션 설정?
                   SOL_SOCKET,
                   SO_UPDATE_ACCEPT_CONTEXT,
                   (char *)&listen_sock,
                   sizeof(listen_sock)) == SOCKET_ERROR)
    {
        err_display("setsockopt(SO_UPDATE_ACCEPT_CONTEXT)"); // 소켓 옵션 설정 실패 시
        closesocket(ctx->sock);
        delete ctx;

        if (IsRunning())
            PostAccept();
        return;
    }

    sockaddr *localAddr = nullptr;  // 로컬(server) 주소 추출
    sockaddr *clientAddr = nullptr; // client 주소 추출
    int localLen = 0;
    int clientLen = 0;

    const DWORD addrLen = sizeof(sockaddr_storage) + 16; //

    lpfnGetAcceptExSockaddrs(
        ctx->acceptBuf.data(),
        0,
        addrLen, addrLen,
        &localAddr, &localLen,
        &clientAddr, &clientLen);

    sockaddr_storage caddr{};
    socklen_t clen = 0;

    if (clientAddr && clientLen > 0) // client 주소가 유효하면
    {
        std::memcpy(&caddr, clientAddr, (size_t)clientLen);
        clen = (socklen_t)clientLen;
    }

    if (clen > 0) // client 주소 길이가 0보다 크면
        PrintClientInfo(caddr, clen, 0);

    IocpStart(ctx->sock, caddr, clen); // 새 세션 생성 및 IOCP 시작

    delete ctx; // IoContext 메모리 해제, iocpStart에서 WSARecv용 IoContext 생성

    if (IsRunning())
        PostAccept();
}

void IocpServer::PrintClientInfo(const sockaddr_storage &caddr, socklen_t clen, bool type)
{
    char ip[INET6_ADDRSTRLEN];
    uint16_t port = 0;

    if (caddr.ss_family == AF_INET)
    {
        sockaddr_in *addr = (sockaddr_in *)&caddr;
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
        port = ntohs(addr->sin_port);
    }
    else
    {
        sockaddr_in6 *addr = (sockaddr_in6 *)&caddr;
        inet_ntop(AF_INET6, &addr->sin6_addr, ip, sizeof(ip));
        port = ntohs(addr->sin6_port);
    }

    if (type == 0)
        std::cout << "[클라이언트 접속] IP 주소: " << ip << ", 포트 번호: " << port << std::endl;
    else
        std::cout << "[클라이언트 종료] IP 주소: " << ip << ", 포트 번호: " << port << std::endl;
}

void IocpServer::IocpStart(SOCKET client_sock, const sockaddr_storage &caddr, socklen_t clen)
{
    std::shared_ptr<Session> session = sessionRegistry.Add(client_sock, caddr, static_cast<int>(clen));
    if (!session)
    {
        closesocket(client_sock);
        return;
    }

    HANDLE h = CreateIoCompletionPort((HANDLE)client_sock, iocpHandle, (ULONG_PTR)session->sessionId, 0);
    if (h == nullptr)
    {
        err_display("CreateIoCompletionPort()");
        session->Close();
        sessionRegistry.Remove(session->sessionId);
        return;
    }

    IoContext *ctx = IoContext::CreateRecv(client_sock, session->sessionId);
    if (ctx == nullptr)
    {
        session->Close();
        sessionRegistry.Remove(session->sessionId);
        return;
    }

    // 비동기 수신 시작
    DWORD flags = 0;
    int retval = WSARecv(client_sock, &ctx->wsabuf, 1, nullptr, &flags, &ctx->overlapped, nullptr);
    if (retval == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        err_display("WSARecv()");
        session->Close();                           // 소켓 닫기
        sessionRegistry.Remove(session->sessionId); // map에서 shared_ptr 제거
        delete ctx;                                 // IoContext 메모리 해제
        return;
    }

    // 접속 메시지 + Sesison 목록 전송 (서버 -> 클라이언트)
    std::string out;
    out += "나의 SessionId = " + std::to_string(session->sessionId) + "\n";

    std::vector<char> packet = BuildPacket(session->sessionId, 0, MsgType::ListResp, out.data(), out.size());

    IoContext *sctx = IoContext::CreateSend(client_sock, session->sessionId, std::make_shared<std::vector<char>>(std::move(packet))); // ?
    if (!sctx)
    {
        session->Close();
        sessionRegistry.Remove(session->sessionId);
        return;
    }

    int sr = WSASend(client_sock, &sctx->wsabuf, 1, nullptr, 0, &sctx->overlapped, nullptr);
    if (sr == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        err_display("WSASend()");
        delete sctx; // Session은 유지
    }
}

DWORD WINAPI IocpServer::WorkerThread(void *arg)
{
    HANDLE iocpHandle = static_cast<HANDLE>(arg);

    // 종료 중에도 completion packet이 남아있을 수 있으므로, Stop()에서 Session 종료(소켓 닫기) 후 while 탈출(break)
    while (true)
    {
        DWORD cbTransferred = 0; // 완료된 io 바이트 수
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED lpOv = nullptr;

        BOOL ok = GetQueuedCompletionStatus(
            iocpHandle,
            &cbTransferred,
            &completionKey,
            &lpOv,
            INFINITE);

        if (lpOv == nullptr && completionKey == exitKey)
            break;

        // 스푸리어스/깨움 용도(현재는 사용 안 해도 안전하게 처리)
        if (lpOv == nullptr)
            continue;

        IoContext *ctx = reinterpret_cast<IoContext *>(lpOv);
        if (ctx->type == IoType::Accept) // AcceptEx 완료 처리
        {
            HandleAcceptComplete(ctx, cbTransferred, ok);
            continue;
        }

        const std::uint32_t sid = static_cast<std::uint32_t>(completionKey);
        std::shared_ptr<Session> session = sessionRegistry.Find(sid);

        // shutdown/에러/원격종료/세션 미존재 => 자원 정리로 수렴
        if (!ok || cbTransferred == 0 || !session || !session->IsAlive())
        {
            if (session)
            {
                if (session->Close()) // Close()가 최초 호출된 경우에만 종료 로그 출력
                    PrintClientInfo(session->remoteAddr, session->remoteAddrLen, 1);

                sessionRegistry.Remove(sid);
            }

            delete ctx;
            continue;
        }

        switch (ctx->type)
        {
        case IoType::Recv:
        {
            // TCP stream 누적 + (header|payload) 파싱
            ctx->streamBuf.insert(ctx->streamBuf.end(),
                                  ctx->recvBuf.begin(),
                                  ctx->recvBuf.begin() + cbTransferred);

            size_t consumed = 0;
            while (true)
            {
                const size_t available = ctx->streamBuf.size() - consumed;
                if (available < PacketHeaderSize) // 헤더가 모두 올 때까지 대기
                    break;

                PacketHeader header{};
                std::memcpy(&header, ctx->streamBuf.data() + consumed, PacketHeaderSize);

                const std::uint32_t payloadLen = ntohl(header.payloadLen);
                const std::uint32_t toSid = ntohl(header.to);
                // const std::uint32_t fromSid = ntohl(header.from);
                const MsgType msgType = static_cast<MsgType>(ntohs(header.type));
                const std::uint16_t reserved = ntohs(header.reserved);

                if (available < PacketHeaderSize + payloadLen)
                    break; // payload가 아직 덜 옴

                const char *payloadStart = ctx->streamBuf.data() + consumed + PacketHeaderSize;

                Task task = Task::MakeRecvMessage(sid, toSid, msgType, reserved,
                                                  std::vector<char>(payloadStart, payloadStart + payloadLen));

                taskQueue_.Push(std::move(task));

                consumed += PacketHeaderSize + payloadLen;
            }

            if (consumed > 0)
                ctx->streamBuf.erase(ctx->streamBuf.begin(), ctx->streamBuf.begin() + consumed);

            // 다음 수신 재개(Recv ctx 재사용)
            ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));
            ctx->wsabuf.buf = ctx->recvBuf.data();
            ctx->wsabuf.len = BUFSIZE;

            DWORD flags = 0;
            int r = WSARecv(ctx->sock, &ctx->wsabuf, 1, nullptr, &flags, &ctx->overlapped, nullptr);
            if (r == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
            {
                err_display("WSARecv()");
                session->Close();
                sessionRegistry.Remove(sid);
                delete ctx;
            }
            break;
        }

        case IoType::Send:
        {
            // 부분 송신 처리
            ctx->sendOffset += cbTransferred;

            if (ctx->sendBuf && ctx->sendOffset < ctx->sendBuf->size())
            {
                ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));

                ctx->wsabuf.buf = reinterpret_cast<char *>(ctx->sendBuf->data() + ctx->sendOffset);
                ctx->wsabuf.len = static_cast<ULONG>(ctx->sendBuf->size() - ctx->sendOffset);

                int r = WSASend(ctx->sock, &ctx->wsabuf, 1, nullptr, 0, &ctx->overlapped, nullptr);
                if (r == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
                {
                    err_display("WSASend()");
                    delete ctx; // send ctx만 정리
                }
                break;
            }

            // send 완료
            delete ctx;
            break;
        }

        default:
            delete ctx;
            break;
        }
    }
    return 0;
}
