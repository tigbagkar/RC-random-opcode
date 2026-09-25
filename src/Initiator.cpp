#include "RC.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

int main() {
    std::cout << "[MAIN] PROCESS STARTED" << "\n\n";
    
    int num_devices = 0;
	ibv_device **device_list = ibv_get_device_list(&num_devices);

	try {
		
		if (num_devices == 0) 
			throw std::runtime_error("[MAIN] ibv_get_device_list() failed");
	
		ibv_device *device = device_list[0];
		
		TcpChannel channel("127.0.0.1", 4000);
	
		RC initiator(device, channel);

		bool     is_initiator               = true;
        int      requested_max_cq_size      = 8;
        int      required_min_cq_size       = 2;
        int      requested_max_send_wr      = 8;
        int      required_min_send_wr       = 2;
        int      requested_max_recv_wr      = 0;
        int      required_min_recv_wr       = 0;
        int      requested_max_send_sge     = 3;
        int      required_min_send_sge      = 1;
        int      requested_max_recv_sge     = 0;
        int      required_min_recv_sge      = 0;
        uint32_t psn                        = 100;
        int      packets_amount_per_message = 4;
        
        initiator.init(
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

        initiator.initiator();

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