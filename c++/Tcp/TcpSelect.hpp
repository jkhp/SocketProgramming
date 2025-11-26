#pragma once
#include "TcpServer.hpp"

class TcpSelect : public TcpServer
{
public:
    TcpSelect(int af, uint16_t port) : TcpServer(af, port) {} // using TcpServer::TcpServer(동일), 부모 생성자 상속
    ~TcpSelect() {}

protected:
    void Run() override;
};