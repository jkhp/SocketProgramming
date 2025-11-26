#include <iostream>
#include "IocpServer.hpp"

int main(void)
{
    IocpServer server(AF_INET, 9000);
    server.Start();

    std::cout << "서버 종료를 원하면 Enter 키를 누르세요..." << std::endl;
    std::cin.get();

    server.Stop();

    return 0;
}