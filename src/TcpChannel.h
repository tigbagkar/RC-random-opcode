#ifndef TCP_CHANNEL_H
#define TCP_CHANNEL_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

class TcpChannel
{
public:

    TcpChannel(
		const std::string &host, 
		uint16_t           port
		);
    
	explicit TcpChannel(
		uint16_t port
		);
    
	~TcpChannel();
    
	void accept();
	
    template<typename T>
    void send(
		const T &data
		);

    template<typename T>
    T receive();

private:
    int sock_ = -1;
    int serverSock_ = -1;

    void connectTo(
		const std::string &host, 
		uint16_t           port
		);
    void createServer(
		uint16_t port
		);

    void sendAll(
		const void *data, 
		std::size_t size
		);
    void receiveAll(
		void 	   *data, 
		std::size_t size
		);
};


template<typename T>
void TcpChannel::send(
	const T &data
	)
{
    static_assert(
        std::is_trivially_copyable_v<T>,
        "T must be trivially copyable"
    );

    uint32_t size = sizeof(T);

    sendAll(&size, sizeof(size));
    sendAll(&data, sizeof(T));
}


template<typename T>
T TcpChannel::receive()
{
    static_assert(
        std::is_trivially_copyable_v<T>,
        "T must be trivially copyable"
    );

    uint32_t size = 0;

    receiveAll(&size, sizeof(size));

    if (size != sizeof(T))
        throw std::runtime_error("Received unexpected data size");

    T data{};

    receiveAll(&data, sizeof(T));

    return data;
}

#endif