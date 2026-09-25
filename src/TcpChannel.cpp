#include "TcpChannel.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

TcpChannel::TcpChannel(
	const std::string &host, 
	uint16_t           port
	)
{
    connectTo(host, port);
}

TcpChannel::TcpChannel(
	uint16_t port
	)
{
    createServer(port);
}

TcpChannel::~TcpChannel()
{
    if (sock_ != -1)
        close(sock_);

    if (serverSock_ != -1)
        close(serverSock_);
}

void TcpChannel::connectTo(
    const std::string &host,
    uint16_t 		   port
	)
{
    sock_ = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_ == -1)
        throw std::runtime_error("socket() failed");

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_port   = htons(port);

    if (inet_pton(
            AF_INET,
            host.c_str(),
            &address.sin_addr) <= 0
			) {
        close(sock_);
        sock_ = -1;

        throw std::runtime_error("Invalid address");
    }


    if (connect(
            sock_,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) == -1
			) {
        close(sock_);
        sock_ = -1;

        throw std::runtime_error("connect() failed");
    }
}


void TcpChannel::createServer(
	uint16_t port
	)
{
    serverSock_ = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSock_ == -1)
        throw std::runtime_error("socket() failed");


    int reuse = 1;

    setsockopt(
        serverSock_,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
		);


    sockaddr_in address{};

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(port);

    if (bind(
            serverSock_,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) == -1
			) {
        close(serverSock_);
        serverSock_ = -1;

        throw std::runtime_error("bind() failed");
    }

    if (listen(serverSock_, 1) == -1) {
        close(serverSock_);
        serverSock_ = -1;

        throw std::runtime_error("listen() failed");
    }
}

void TcpChannel::accept()
{
    sock_ = ::accept(
        serverSock_,
        nullptr,
        nullptr
		);

    if (sock_ == -1)
        throw std::runtime_error("accept() failed");
}

void TcpChannel::sendAll(
    const void *data,
    std::size_t size
	)
{
    const char* ptr =
        static_cast<const char*>(data);

    while (size > 0)
    {
        ssize_t result =
            ::send(sock_, ptr, size, 0);

        if (result <= 0)
            throw std::runtime_error("send() failed");

        ptr += result;
        size -= result;
    }
}

void TcpChannel::receiveAll(
    void       *data,
    std::size_t size
	)
{
    char* ptr =
        static_cast<char*>(data);

    while (size > 0)
    {
        ssize_t result =
            ::recv(sock_, ptr, size, 0);

        if (result == 0)
            throw std::runtime_error("Connection closed");

        if (result < 0)
            throw std::runtime_error("recv() failed");

        ptr += result;
        size -= result;
    }
}
