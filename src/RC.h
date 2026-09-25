#ifndef RC_H
#define RC_H

#include <cstddef>
#include <string>
#include <infiniband/verbs.h>
#include "TcpChannel.h"

struct ConnectionInfo {
    uint32_t qpn;
	uint32_t psn;
    ibv_gid  gid;
};

struct CapInfo {
    ibv_mtu  mtu;	
    int      max_batch_wr;
    int      max_batch_rdma;
};

struct MRInfo {
    uint32_t r_key;
	uint64_t mr_start_addr;
};

struct RecvExp {
    bool          recv_wr_required;
    bool          with_imm;
    bool          with_payload;
    uint32_t      crc32;
    uint64_t      wr_id;
    ibv_wr_opcode opcode;
};

class RC {
	private:
		bool         is_initiator = false;
		ibv_device  *device       = nullptr;
		ibv_context *context      = nullptr;
		ibv_pd      *pd           = nullptr;
		void        *buffer       = nullptr;
		ibv_mr      *mr           = nullptr;
		ibv_cq      *cq           = nullptr;
		ibv_qp      *qp           = nullptr;
		TcpChannel   channel;
        MRInfo       remote_mr_info;
        int          max_batch_wr;
        int          max_batch_rdma;
        int          max_sge;
        int          message_length;

        void        fillBuffer(void* buff_start_addr, size_t length, uint64_t wr_id);
        uint32_t    calcCrc32(void* buff_start_addr, size_t length);
        std::string wrOpcodeToString(ibv_wr_opcode opcode);
        std::string wcOpcodeToString(ibv_wc_opcode opcode);

    public:
		RC(
			ibv_device *device,
			TcpChannel &channel
		);
		~RC();

		void init(
			bool is_initiator, 
            int      requested_max_cq_size,
            int      required_min_cq_size,
            int      requested_max_send_wr,
            int      required_min_send_wr,
            int      requested_max_recv_wr,
            int      required_min_recv_wr,
            int      requested_max_send_sge,
            int      required_min_send_sge,
            int      requested_max_recv_sge,
            int      required_min_recv_sge,
            uint32_t psn,
            int      packets_amount_per_message
		);
			
		void initiator();
		
		void target();
		
		void exit();


};

#endif
