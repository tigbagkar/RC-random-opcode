#include "RC.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

int main() {
    std::cout << "[MAIN] PROCESS STARTED" << "\n";

    int num_devices = 0;
    ibv_device **device_list = ibv_get_device_list(&num_devices);

	try {

		if (num_devices == 0) 
			throw std::runtime_error("[MAIN] ibv_get_device_list() failed");
	
		ibv_device *device = device_list[0];
	
        TcpChannel channel(4000);
		std::cout << "[MAIN] WAITING FOR INITIATOR PROCESS TO START" << "\n\n"; 
        channel.accept();	
        
		
        RC target(device, channel);

		bool     is_initiator = false;
        
        target.init(
			is_initiator 
		);

        target.target();

		ibv_free_device_list(device_list);
		device_list = nullptr;
        
        std::cout << "[MAIN] TEST PASSED" << "\n\n";

		return 0;
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << '\n';

        if (device_list != nullptr)
            ibv_free_device_list(device_list);

        return -1;
	}
}