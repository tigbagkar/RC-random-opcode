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

		bool     is_initiator               = false;
        int      requested_max_cq_size      = 8;
        int      required_min_cq_size       = 2;
        int      requested_max_send_wr      = 0;
        int      required_min_send_wr       = 0;
        int      requested_max_recv_wr      = 8;
        int      required_min_recv_wr       = 2;
        int      requested_max_send_sge     = 0;
        int      required_min_send_sge      = 0;
        int      requested_max_recv_sge     = 3;
        int      required_min_recv_sge      = 1;
        uint32_t psn                        = 1000;
        int      packets_amount_per_message = 4;
        
        target.init(
			is_initiator, 
            requested_max_cq_size,
            required_min_cq_size,
            requested_max_send_wr,
            required_min_send_wr,
            requested_max_recv_wr,
            required_min_recv_wr,
            requested_max_send_sge,
            required_min_send_sge,
            requested_max_recv_sge,
            required_min_recv_sge,
            psn,
            packets_amount_per_message
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