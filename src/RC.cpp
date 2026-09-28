#include "RC.h"
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <stdio.h>
#include <arpa/inet.h>
#include <random>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

RC::RC(
    ibv_device *device,
    TcpChannel &channel
) : device(device), channel(channel) {}

RC::~RC() { 
    exit(); 
}

void RC::init(
    bool is_initiator
    ) {
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// START INITIALIZATION //////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INIT] START INITIALIZATION" << "\n\n";

    this->is_initiator = is_initiator;
	
    int requested_max_cqe      =                 64;
    int required_min_cqe       =                 16;
    int requested_max_send_wr  =  is_initiator ? 64 : 0;
    int required_min_send_wr   =  is_initiator ? 16 : 0;
    int requested_max_recv_wr  = !is_initiator ? 64 : 0;
    int required_min_recv_wr   = !is_initiator ? 16 : 0;
    int requested_max_send_sge =  is_initiator ?  8 : 0;
    int required_min_send_sge  =  is_initiator ?  2 : 0;
    int requested_max_recv_sge = !is_initiator ?  8 : 0;
    int required_min_recv_sge  = !is_initiator ?  2 : 0;
    int psn                    = is_initiator  ?  0 : 1000;

    int ret                  = 0;
    int actual_max_cqe       = 0;
    int actual_max_wr        = 0;
    int local_max_batch_wr   = 0;
    int local_max_batch_rdma = 0;

    std::cout << "    initialization data"                                    << "\n" <<
                 "        requested_max_cqe:      " << requested_max_cqe      << "\n" <<      
                 "        required_min_cqe:       " << required_min_cqe       << "\n" <<
                 "        requested_max_send_wr:  " << requested_max_send_wr  << "\n" <<
                 "        required_min_send_wr:   " << required_min_send_wr   << "\n" <<  
                 "        requested_max_recv_wr:  " << requested_max_recv_wr  << "\n" << 
                 "        required_min_recv_wr:   " << required_min_recv_wr   << "\n" << 
                 "        requested_max_send_sge: " << requested_max_send_sge << "\n" << 
                 "        required_min_send_sge:  " << required_min_send_sge  << "\n" << 
                 "        requested_max_recv_sge: " << requested_max_recv_sge << "\n" << 
                 "        required_min_recv_sge:  " << required_min_recv_sge  << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// OPEN AND QUERY DEVICE /////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INIT] OPEN AND QUERY DEVICE" << "\n";

        //-------------//
        // OPEN DEVICE //
        //-------------//
	context = ibv_open_device(device);
	if (context == nullptr) 
		throw std::runtime_error("[ERROR][INIT] ibv_open_device() failed");

        //--------------//
        // QUEUE DEVICE //
        //--------------//
    ibv_device_attr device_attr{};
	ret = ibv_query_device(context, &device_attr);	
	if (ret) 
		throw std::runtime_error("[ERROR][INIT] ibv_query_device() failed");

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    device opened and queued successfully"      << "\n" <<
                 "        device name: " << context->device->name << "\n" <<
                 "        context:     " << context               << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// ALLOCATE PD ///////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INIT] ALLOCATE PD" << "\n";

        //-------------//
        // ALLOCATE PD //
        //-------------//
	pd = ibv_alloc_pd(context);
	if (pd == nullptr) 
		throw std::runtime_error("[ERROR][INIT] ibv_allic_pd() failed");

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    pd allocated successfully"        << "\n" <<
                 "        pd:      " << pd              << "\n" <<
                 "        context: " << pd->context     << "\n\n";
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// CREATE CQ /////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INIT] CREATE CQ" << "\n";

        //-------------------------------//
        // REQUIRED MIN CQE VS CAP CHECK //
        //-------------------------------//
    if (required_min_cqe > device_attr.max_cqe) 
        throw std::runtime_error(
            "[ERROR][INIT] required_min_cqe: " + std::to_string(required_min_cqe) + "\n" +
            "more than device max_cqe:       " + std::to_string(device_attr.max_cqe)
            );
        
        //------------------------------//
        // REQUESTED MAX CQE CORRECTION //
        //------------------------------//
    if (requested_max_cqe > device_attr.max_cqe) 
        requested_max_cqe = device_attr.max_cqe;
    actual_max_cqe = requested_max_cqe;

        //-------------------------------//
        // CQ OVERFLOW PROTECTION CHECKS //
        //-------------------------------//
    if (required_min_send_wr > requested_max_cqe)
            throw std::runtime_error(
                "[ERROR][INIT] required_min_send_wr: " + std::to_string(required_min_send_wr) + "\n" +
                "more than requested_max_cqe:        " + std::to_string(requested_max_cqe)
                );
    if (required_min_recv_wr > requested_max_cqe)
            throw std::runtime_error(
                "[ERROR][INIT] requiered_min_recv_wr: " + std::to_string(required_min_recv_wr) + "\n" +
                "more than requested_max_cqe:         " + std::to_string(requested_max_cqe)
                );

        //-----------//
        // CREATE CQ //
        //-----------//
	cq = ibv_create_cq(
		context,
		requested_max_cqe,
		nullptr,
		nullptr,
		0
	);
	if (cq == nullptr) 
		throw std::runtime_error("[ERROR][INIT] ibv_create_cq() failed");
        
        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    cq created succesfully"          << "\n" <<
                 "        cq:      " << cq             << "\n" <<
                 "        max_cqe: " << actual_max_cqe << "\n" << 
                 "        context: " << cq->context    << "\n\n";
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// CREATE QP /////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INIT] CREATE QP" << "\n";
        
        //------------------------------//
        // REQUIRED MIN_* VS CAP CHECKS //
        //------------------------------//
    if (required_min_send_wr > device_attr.max_qp_wr) 
        throw std::runtime_error(
            "[ERROR][INIT] required_min_send_wr: " + std::to_string(required_min_send_wr) + "\n" +
            "more than device max_qp_wr:         " + std::to_string(device_attr.max_qp_wr)
            );
    if (required_min_recv_wr > device_attr.max_qp_wr)
        throw std::runtime_error(
            "[ERROR][INIT] required_min_recv_wr: " + std::to_string(required_min_recv_wr) + "\n" +
            "more than device max_qp_wr:         " + std::to_string(device_attr.max_qp_wr)
            );
    if (required_min_send_sge > device_attr.max_sge)
        throw std::runtime_error(
            "[ERROR][INIT] required_min_send_sge:" + std::to_string(required_min_send_sge) + "\n" +
            "more than device max_sge:           " + std::to_string(device_attr.max_sge)
            );
    if (required_min_recv_sge > device_attr.max_sge)
        throw std::runtime_error(
            "[ERROR][INIT] required_min_recv_sge: " + std::to_string(required_min_recv_sge) + "\n" +
            "more than device max_sge:            " + std::to_string(device_attr.max_sge)
            );

        //-----------------------------//
        // REQUESTED MAX_* CORRECTIONS //
        //-----------------------------//
    if (requested_max_send_wr > device_attr.max_qp_wr) 
        requested_max_send_wr = device_attr.max_qp_wr;
    if (requested_max_recv_wr > device_attr.max_qp_wr)
        requested_max_recv_wr = device_attr.max_qp_wr;
    if (requested_max_send_sge > device_attr.max_sge)
        requested_max_send_sge = device_attr.max_sge;
    if (requested_max_recv_sge > device_attr.max_sge)
        requested_max_recv_sge = device_attr.max_sge;

        //-----------//
        // CREATE QP //
        //-----------//
	ibv_qp_init_attr qp_init_attr{};
	qp_init_attr.send_cq          = cq;
	qp_init_attr.recv_cq          = cq;
	qp_init_attr.cap.max_send_sge = requested_max_send_sge;
	qp_init_attr.cap.max_send_wr  = requested_max_send_wr;
	qp_init_attr.cap.max_recv_sge = requested_max_recv_sge;
	qp_init_attr.cap.max_recv_wr  = requested_max_recv_wr;	
	qp_init_attr.qp_type          = IBV_QPT_RC;
	
	qp = ibv_create_qp(
		pd,
		&qp_init_attr
	);
	if (qp == nullptr) 
		throw std::runtime_error("[ERROR][INIT] ibv_create_qp() failed");

        //---------------------------------//
        // REQUIRED MIN_* VS ACTUAL CHECKS //
        //---------------------------------//
    if (required_min_send_wr > qp_init_attr.cap.max_send_wr) 
        throw std::runtime_error(
            "[ERROR][INIT] required_min_send_wr: " + std::to_string(required_min_send_wr) + "\n" +
            "more than actual qp max_send_wr:    " + std::to_string(qp_init_attr.cap.max_send_wr)
            );
    if (required_min_recv_wr > qp_init_attr.cap.max_recv_wr)
        throw std::runtime_error(
            "[ERROR][INIT] required_min_recv_wr: " + std::to_string(required_min_recv_wr) + "\n" +
            "more than actual qp max_recv_wr:    " + std::to_string(qp_init_attr.cap.max_recv_wr)
            );
    if (required_min_send_sge > qp_init_attr.cap.max_send_sge)
        throw std::runtime_error(
            "[ERROR][INIT] required_min_send_sge: " + std::to_string(required_min_send_sge) + "\n" +
            "more than actual qp max_send_sge:    " + std::to_string(qp_init_attr.cap.max_send_sge)
            );
    if (required_min_recv_sge > qp_init_attr.cap.max_recv_sge)
        throw std::runtime_error(
            "[ERROR][INIT] required_min_recv_sge: " + std::to_string(required_min_recv_sge) + "\n" +
            "more than actual qp max_recv_sge:    " + std::to_string(qp_init_attr.cap.max_recv_sge)
            );
    
    if (is_initiator) {
        actual_max_wr        = qp_init_attr.cap.max_send_wr;
        max_sge              = qp_init_attr.cap.max_send_sge;
        local_max_batch_rdma = device_attr.max_qp_init_rd_atom;
    }
    else {
        actual_max_wr        = qp_init_attr.cap.max_recv_wr;
        max_sge              = qp_init_attr.cap.max_recv_sge;
        local_max_batch_rdma = device_attr.max_res_rd_atom;
    } 
         
        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    qp created successfully"                             << "\n" <<
                 "        type:         " << qp->qp_type                   << "\n" <<
                 "        qpn:          " << qp->qp_num                    << "\n" <<
                 "        send_cq:      " << qp->send_cq                   << "\n" <<
                 "        recv_cq:      " << qp->recv_cq                   << "\n" <<
                 "        max_send_sge: " << qp_init_attr.cap.max_send_sge << "\n" <<
                 "        max_send_wr:  " << qp_init_attr.cap.max_send_wr  << "\n" <<
                 "        max_recv_sge: " << qp_init_attr.cap.max_recv_sge << "\n" <<
                 "        max_recv_wr:  " << qp_init_attr.cap.max_recv_wr  << "\n" <<
                 "        state:        " << qp->state                     << "\n" <<
                 "        context:      " << qp->context                   << "\n" <<
                 "        pd:           " << qp->pd                        << "\n\n";
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// FIND SUITABLE PORT AND GID ////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INIT] FIND SUITABLE PORT AND GID" << "\n";

	uint8_t  port_num;
	ibv_mtu  mtu;
	uint32_t gid_index;
	ibv_gid  gid;
	bool     found = false;
        
        //----------------------------//
        // FIND SUITABLE PORT AND GID //
        //----------------------------//
	for (uint8_t port_n = 1; port_n <= device_attr.phys_port_cnt; port_n++) {
        if (found)
			break;
		
		ibv_port_attr port_attr{};

		ret = ibv_query_port(
			context, 
			port_n, 
			&port_attr
			);
		if (ret)
			throw std::runtime_error("[ERROR][INIT] ibv_query_port() failed");
            
            //------------//
            // CHECK PORT //
            //------------//
		if (port_attr.state != IBV_PORT_ACTIVE)
			continue;

		if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET)
			continue;

		for (uint32_t gid_i = 0; gid_i < port_attr.gid_tbl_len; gid_i++) {
			ibv_gid_entry entry{};

			ret = ibv_query_gid_ex(
				context,
				port_n,
				gid_i,
				&entry,
				0
			);
			if (ret) 
				throw std::runtime_error("[ERROR][INIT] ibv_query_gid_ex() failed");

                //-----------//
                // CHECK GID //
                //-----------//
			if (entry.gid_type == IBV_GID_TYPE_ROCE_V2) {
				port_num  = port_n;
				mtu       = port_attr.active_mtu;
				gid_index = gid_i;
				gid       = entry.gid;			
				found     = true;
				break;
			}
			else 
				continue;
		}
	}
	if (!found) 
		throw std::runtime_error("[ERROR][INIT] suitable port and gid not found");

    char local_gid_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, gid.raw, local_gid_str, sizeof(local_gid_str));

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    suitable port and gid found"                   << "\n" <<
                 "        port_num:  " << static_cast<int>(port_num) << "\n" <<
                 "        mtu:       " << mtu                        << "\n" <<
                 "        gid_index: " << gid_index                  << "\n" <<
                 "        gid:       " << local_gid_str              << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// LOCAL CQ OVERFLOW PROTECTION CHECK ////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INIT] LOCAL CQ OVERFLOW PROTECTION CHECK" << "\n";

        //-----------------------------------//
        // CQ OVERFLOW PROTECTION CORRECTION //
        //-----------------------------------//
    if (actual_max_wr > actual_max_cqe)
        local_max_batch_wr = actual_max_cqe;
    else
        local_max_batch_wr = actual_max_wr;

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    local_max_batch_wr: " << local_max_batch_wr << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// EXCHANGE CONNECTION INFO AND CAP INFO /////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INIT] EXCHANGE CONNECTION INFO AND CAP INFO" << "\n";

        //--------------------------//
        // EXCHANGE CONNECTION INFO //
        //--------------------------//
    ConnectionInfo local_connection_info{
        .qpn = qp->qp_num,
        .psn = psn,
        .gid = gid
    };
    ConnectionInfo remote_connection_info{};

	if (is_initiator) {
		channel.send(local_connection_info);
		remote_connection_info = channel.receive<ConnectionInfo>();
	}
	else {
		remote_connection_info = channel.receive<ConnectionInfo>();
		channel.send(local_connection_info);	
	}

        //---------//
        // SUCCESS //
        //---------//
    char remote_gid_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, remote_connection_info.gid.raw, remote_gid_str, sizeof(remote_gid_str));

    std::cout << "    send connection info"                   << "\n" <<
                 "        qpn: " << local_connection_info.qpn << "\n" <<
                 "        psn: " << local_connection_info.psn << "\n" <<
                 "        gid: " << local_gid_str             << "\n";

    std::cout << "    recv connection info"                    << "\n" <<
                 "        qpn: " << remote_connection_info.qpn << "\n" <<
                 "        psn: " << remote_connection_info.psn << "\n" <<
                 "        gid: " << remote_gid_str             << "\n\n";

        //-------------------//
        // EXCHANGE CAP INFO //
        //-------------------//
    CapInfo local_cap_info{
        .mtu            = mtu,
        .max_batch_wr   = local_max_batch_wr,
        .max_batch_rdma = local_max_batch_rdma
    };
    CapInfo remote_cap_info{};  

    if (is_initiator) {
        channel.send(local_cap_info);
		remote_cap_info = channel.receive<CapInfo>();
	}
	else {
        remote_cap_info = channel.receive<CapInfo>();
		channel.send(local_cap_info);	
	}

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    send cap info"                                         << "\n" <<
                 "        mtu:            " << local_cap_info.mtu            << "\n" <<
                 "        max_batch_wr:   " << local_cap_info.max_batch_wr   << "\n" << 
                 "        max_batch_rdma: " << local_cap_info.max_batch_rdma << "\n";

    std::cout << "    recv cap info"                                          << "\n" <<
                 "        mtu:            " << remote_cap_info.mtu            << "\n" <<
                 "        max_batch_wr:   " << remote_cap_info.max_batch_wr   << "\n" << 
                 "        max_batch_rdma: " << remote_cap_info.max_batch_rdma << "\n\n";
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// REMOTE CQ OVERFLOW AND MAX OUTSTANDING RDMA PROTECTION CHECK //////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////		
    std::cout << "[INIT] REMOTE CQ OVERFLOW AND MAX OUTSTANDING RDMA PROTECTION CHECK" << "\n";

        //-----------------------------------//
        // CQ OVERFLOW PROTECTION CORRECTION //
        //-----------------------------------//
    if (local_cap_info.max_batch_wr > remote_cap_info.max_batch_wr)
        max_batch_wr = remote_cap_info.max_batch_wr;
    else
        max_batch_wr = local_cap_info.max_batch_wr;

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    max_batch_wr:   " << max_batch_wr << "\n";

        //-------------------------------------------------//
        // OUTSTANDING RDMA OVERFLOW PROTECTION CORRECTION //
        //-------------------------------------------------//
    if (local_cap_info.max_batch_rdma > remote_cap_info.max_batch_rdma)
        max_batch_rdma = remote_cap_info.max_batch_rdma;
    else
        max_batch_rdma = local_cap_info.max_batch_rdma;

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    max_batch_rdma: " << max_batch_rdma << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// ALLOCATE BUFFER ///////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INIT] ALLOCATE BUFFER" << "\n";

        //-----------------------------------------//
        // CALCULATE BUFF SIZE AND MESSAGES LENGTH //
        //-----------------------------------------//    
    size_t  buff_size = 0;
    ibv_mtu pmtu      = mtu > remote_cap_info.mtu ? remote_cap_info.mtu : mtu;
    
    if (is_initiator) {
        int     int_pmtu  = 0;
        switch(pmtu) {
            case IBV_MTU_256:
                int_pmtu = 256;
                break;
            case IBV_MTU_512:
                int_pmtu = 512;
                break;
            case IBV_MTU_1024:
                int_pmtu = 1024;
                break;
            case IBV_MTU_2048:
                int_pmtu = 2048;
                break;
            case IBV_MTU_4096:
                int_pmtu = 4096;
        }

        std::random_device                 last_packet_size_rd;
        std::mt19937                       last_packet_size_gen(last_packet_size_rd());
        std::uniform_int_distribution<int> last_packet_size_dist(max_sge, int_pmtu);

        std::random_device                 packets_per_message_rd;
        std::mt19937                       packets_per_message_gen(packets_per_message_rd());
        std::uniform_int_distribution<int> packets_per_message_dist(0, 7);

        addrs.resize(max_batch_wr);
        for (int wr_i = 0; wr_i < max_batch_wr; wr_i++) {
            int last_packet_size    = last_packet_size_dist(last_packet_size_gen);
            int packets_per_message = packets_per_message_dist(packets_per_message_gen);
            size_t length           = last_packet_size + int_pmtu * packets_per_message;
    
            channel.send(length);

            addrs[wr_i].length = length;
            buff_size         += length;
        }
    }
    else {
        addrs.resize(max_batch_wr);
        for (int wr_i = 0; wr_i < max_batch_wr; wr_i++) {
            size_t length = channel.receive<size_t>();

            addrs[wr_i].length = length;
            buff_size         += length;
        }
    }
    
        //-----------------//
        // ALLOCATE BUFFER //
        //-----------------//
    buffer = std::malloc(buff_size);
	if (buffer == nullptr) 
		throw std::runtime_error("[ERROR][INIT] malloc() failed");

        //---------------------------//
        // CALCULATE LOCAL ADDRESSES //
        //---------------------------//
    void *offset = buffer;
    for (int wr_i = 0; wr_i < max_batch_wr; wr_i++) {
        addrs[wr_i].start_addr = offset;
        offset += addrs[wr_i].length; 
    }
        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    buffer allocated successfully"                          << "\n" <<
                 "        start addr: " << reinterpret_cast<uint64_t>(buffer) << "\n" <<
                 "        size:       " << buff_size                          << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// REGISTER MR ///////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INIT] REGISTER MR" << "\n";

        //-------------//
        // REGISTER MR //
        //-------------//
    int mr_access = is_initiator ? IBV_ACCESS_LOCAL_WRITE : IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
		
	mr = ibv_reg_mr(
		pd,
		buffer,
		buff_size,
		mr_access
	);
	if(mr == nullptr) 
		throw std::runtime_error("[ERROR][INIT] ibv_reg_mr() failed");

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    mr registered successfully"        << "\n" <<
                 "        start addr: " << mr->addr      << "\n" <<
                 "        length:     " << mr->length    << "\n" <<
                 "        lkey:       " << mr->lkey      << "\n" <<
                 "        rkey:       " << mr->rkey      << "\n" << 
                 "        context:    " << mr->context   << "\n" <<
                 "        pd:         " << mr->pd        << "\n\n";
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// EXCHANGE MR INFO //////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INIT] EXCHANGE MR INFO" << "\n";

        //------------------//
        // EXCHANGE MR INFO //
        //------------------//
    if (is_initiator) {
        remote_mr_info = channel.receive<MRInfo>();

            //---------//
            // SUCCESS //
            //---------//
        std::cout << "    recv data"                                           << "\n" <<
                     "        r_key:         " << remote_mr_info.r_key         << "\n" <<
                     "        mr_start_addr: " << remote_mr_info.mr_start_addr << "\n\n";
    }
    else {
        MRInfo local_mr_info{
            .r_key         = mr->rkey,
            .mr_start_addr = reinterpret_cast<uint64_t>(buffer)
        };

        channel.send(local_mr_info);

            //---------//
            // SUCCESS //
            //---------//
        std::cout << "    send data"                                          << "\n" <<
                     "        r_key:         " << local_mr_info.r_key         << "\n" <<
                     "        mr_start_addr: " << local_mr_info.mr_start_addr << "\n\n";
    }
        //----------------------------//
        // CALCULATE REMOTE ADDRESSES //
        //----------------------------//
    if (is_initiator) {
        remote_addrs.resize(max_batch_wr);

        uint64_t offset = remote_mr_info.mr_start_addr;
        for (int wr_i = 0; wr_i < max_batch_wr; wr_i++) {
            remote_addrs[wr_i].length     = addrs[wr_i].length;
            remote_addrs[wr_i].start_addr = offset;
            offset                       += remote_addrs[wr_i].length;
        }
    } 

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////// RESET -> INIT QP STATE TRANSITION /////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////		
    std::cout << "[INIT] RESET -> INIT QP STATE TRANSITION" << "\n";

        //-----------------------------------//
        // RESET -> INIT QP STATE TRANSITION //
        //-----------------------------------//
	int qp_access = is_initiator ? IBV_ACCESS_LOCAL_WRITE : IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
	
	ibv_qp_attr init_transition_attr{};
	init_transition_attr.qp_state        = IBV_QPS_INIT;
	init_transition_attr.port_num        = port_num;
	init_transition_attr.qp_access_flags = qp_access;
		
	int init_transition_attr_mask = 
		IBV_QP_STATE      |
		IBV_QP_PKEY_INDEX |
		IBV_QP_PORT       |
		IBV_QP_ACCESS_FLAGS;
	
	ret = ibv_modify_qp(
		qp, 
		&init_transition_attr, 
		init_transition_attr_mask
		);
	if (ret) 
		throw std::runtime_error("[ERROR][INIT] RESET -> INIT qp state transition failed");
        
        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    qp state transitioned RESET -> INIT successfully"                    << "\n" <<
                 "        port_num:   " << static_cast<int>(init_transition_attr.port_num) << "\n" <<
                 "        qp_access:  " << init_transition_attr.qp_access_flags            << "\n\n";
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////// INIT -> RTR QP STATE TRANSITION //////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////		
    std::cout << "INIT -> RTR QP STATE TRANSITION" << "\n";
        
        //---------------------------------//
        // INIT -> RTR QP STATE TRANSITION //
        //---------------------------------//
	ibv_qp_attr rtr_transition_attr{};
	rtr_transition_attr.qp_state               = IBV_QPS_RTR;
	rtr_transition_attr.path_mtu               = pmtu;
	rtr_transition_attr.dest_qp_num            = remote_connection_info.qpn;
	rtr_transition_attr.rq_psn 			       = remote_connection_info.psn;
	rtr_transition_attr.max_dest_rd_atomic     = is_initiator ? 0 : max_batch_rdma;
	rtr_transition_attr.min_rnr_timer          = 12; 
	rtr_transition_attr.ah_attr.is_global      = 1; 
	rtr_transition_attr.ah_attr.port_num       = port_num;
	rtr_transition_attr.ah_attr.grh.dgid       = remote_connection_info.gid;
	rtr_transition_attr.ah_attr.grh.sgid_index = gid_index;
	rtr_transition_attr.ah_attr.grh.hop_limit  = 1;  
	
	int rtr_transition_attr_mask = 
		IBV_QP_STATE    		  |
		IBV_QP_PATH_MTU           |
		IBV_QP_DEST_QPN           |
		IBV_QP_RQ_PSN             |
		IBV_QP_MAX_DEST_RD_ATOMIC |
		IBV_QP_MIN_RNR_TIMER      |
		IBV_QP_AV;
	
	ret = ibv_modify_qp(
		qp, 
		&rtr_transition_attr, 
		rtr_transition_attr_mask
		);
	if (ret) 
		throw std::runtime_error("[ERROR][INIT] INIT -> RTR qp state transition failed");

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    qp state transitioned INIT -> RTR successfully"                                           << "\n" <<
                 "        path_mtu:           " << rtr_transition_attr.path_mtu                                 << "\n" <<
                 "        dest_qp_num:        " << rtr_transition_attr.dest_qp_num                              << "\n" <<
                 "        rq_psn:             " << rtr_transition_attr.rq_psn                                   << "\n" <<
                 "        max_dest_rd_atomic: " << static_cast<int>(rtr_transition_attr.max_dest_rd_atomic)     << "\n" <<
                 "        min_rnr_timer:      " << static_cast<int>(rtr_transition_attr.min_rnr_timer)          << "\n" <<
                 "        is_global:          " << static_cast<int>(rtr_transition_attr.ah_attr.is_global)      << "\n" <<
                 "        port_num:           " << static_cast<int>(rtr_transition_attr.ah_attr.port_num)       << "\n" <<
                 "        dgid:               " << remote_gid_str                                               << "\n" << 
                 "        sgid_index:         " << static_cast<int>(rtr_transition_attr.ah_attr.grh.sgid_index) << "\n" <<
                 "        hop_limit:          " << static_cast<int>(rtr_transition_attr.ah_attr.grh.hop_limit)  << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////  RTR -> RTS QP STATE TRANSITION ////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////		
    std::cout << "[INIT] RTR -> RTS QP STATE TRANSITION" << "\n";

	ibv_qp_attr rts_transition_attr{};
	rts_transition_attr.qp_state      = IBV_QPS_RTS;            
	rts_transition_attr.sq_psn        = psn;
	rts_transition_attr.timeout       = 12;
	rts_transition_attr.retry_cnt     = 7;  
	rts_transition_attr.rnr_retry     = 7;  
	rts_transition_attr.max_rd_atomic = is_initiator ? max_batch_rdma : 0;
	
	int rts_transition_attr_mask = 
		IBV_QP_STATE     |
		IBV_QP_SQ_PSN    |
		IBV_QP_TIMEOUT   |
		IBV_QP_RETRY_CNT |
		IBV_QP_RNR_RETRY |
		IBV_QP_MAX_QP_RD_ATOMIC;
		
	ret = ibv_modify_qp(
		qp, 
		&rts_transition_attr, 
		rts_transition_attr_mask
		);
	if (ret) 
		throw std::runtime_error("RTR -> RTS state transition failed");
        
        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    qp state transitioned RTR -> RTS successfully"                              << "\n" <<
                 "        sq_psn:        " << rts_transition_attr.sq_psn                          << "\n" <<
                 "        timeout:       " << static_cast<int>(rts_transition_attr.timeout)       << "\n" <<
                 "        retry_cnt:     " << static_cast<int>(rts_transition_attr.retry_cnt)     << "\n" <<
                 "        rnr_retry:     " << static_cast<int>(rts_transition_attr.rnr_retry)     << "\n" <<
                 "        max_rd_atomic: " << static_cast<int>(rts_transition_attr.max_rd_atomic) << "\n\n";
}

void RC::initiator() {
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////// START PROCESSING /////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INITIATOR] START PROCESSING" << "\n\n";

    if (!is_initiator)
        throw std::runtime_error("[ERROR][INITIATOR] access to initiator() denied");

    int ret;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////// WRS GENERATION ///////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INITIATOR] WRS GENERATION" << "\n";

    std::vector<ibv_send_wr>          wrs(max_batch_wr);
    std::vector<std::vector<ibv_sge>> sges(max_batch_wr);
    std::vector<RecvExp>              exps(max_batch_wr);
    
    std::random_device                 sge_rd;
    std::mt19937                       sge_gen(sge_rd());
    std::uniform_int_distribution<int> sge_dist(1, max_sge);

    std::random_device                 opcode_rd;
    std::mt19937                       opcode_gen(opcode_rd());
    std::uniform_int_distribution<int> opcode_dist(0, 4);

        //----------//
        // WRS LOOP //
        //----------//
    int rdma_counter = 0;    
    for (int wr_i = 0; wr_i < max_batch_wr; wr_i++) {
        std::cout << "{"     << "\n";
        std::cout << "    {" << "\n";

            //-------------------------------------//
            // GET SGE AMOUNT AND CALCULATE LENGTH //
            //-------------------------------------//
        int sge_amount = sge_dist(sge_gen);
        int sge_length = addrs[wr_i].length / sge_amount;
        sges[wr_i].resize(sge_amount);
        
            //----------//
            // SGE LOOP //
            //----------//
        for (int sge_i = 0; sge_i < sge_amount; sge_i++) {

                //------------//
                // CREATE SGE //
                //------------//
            ibv_sge &sge = sges[wr_i][sge_i];
            sge.addr     = reinterpret_cast<uint64_t>(addrs[wr_i].start_addr) + (sge_length * sge_i);
            sge.length   = sge_length;
            sge.lkey     = mr->lkey;

                //----------------------------//
                // LAST SGE LENGTH CORRECTION //
                //----------------------------//
            if (sge_i == sge_amount-1)
                sge.length = addrs[wr_i].length - (sge_length * sge_i);

                //---------//
                // SUCCESS //
                //---------//
            std::cout << "        sge created"                << "\n" <<
                         "            num:    " << sge_i + 1  << "\n" <<  
                         "            addr:   " << sge.addr   << "\n" << 
                         "            length: " << sge.length << "\n" <<
                         "            lkey:   " << sge.lkey   << "\n";
        }
        std::cout << "    }" << "\n";

            //------------//
            // GET OPCODE //
            //------------//
        int           opcode_num = opcode_dist(opcode_gen);
        ibv_wr_opcode opcode;

        switch(opcode_num) {
            case 0:
                opcode = IBV_WR_SEND;
                break;
            case 1:
                opcode = IBV_WR_SEND_WITH_IMM;
                break;
            case 2:
                opcode = IBV_WR_RDMA_WRITE;
                break;
            case 3:
                opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
                break;
            case 4:
                opcode = IBV_WR_RDMA_READ; 
        }

        bool is_rdma      = opcode == IBV_WR_RDMA_WRITE || opcode == IBV_WR_RDMA_WRITE_WITH_IMM || opcode == IBV_WR_RDMA_READ;
        bool with_imm     = opcode == IBV_WR_SEND_WITH_IMM || opcode == IBV_WR_RDMA_WRITE_WITH_IMM;
        bool with_payload = opcode != IBV_WR_RDMA_READ; 

        if (rdma_counter == max_batch_rdma && opcode == IBV_WR_RDMA_READ) {
            opcode       = IBV_WR_SEND;
            is_rdma      = false;
            with_imm     = false;
            with_payload = true;
        }

            //---------------------------------//
            // FILL BUFFER AND CALCULATE CRC32 //
            //---------------------------------//
        uint32_t crc32 = 0;    
        if (with_payload) {
            fillBuffer(addrs[wr_i].start_addr, addrs[wr_i].length, wr_i);
            crc32 = calcCrc32(addrs[wr_i].start_addr, addrs[wr_i].length);
        }

            //-----------//
            // CREATE WR //
            //-----------//
        ibv_send_wr &wr = wrs[wr_i];
        wr.wr_id        = wr_i;
        wr.sg_list      = sges[wr_i].data();
        wr.num_sge      = sge_amount;
        wr.opcode       = opcode;
        wr.send_flags   = IBV_SEND_SIGNALED;

            //---------//
            // SUCCESS //
            //---------//
        std::cout << "    wr created"                                       << "\n" <<
                     "        wr_id:       " << wr.wr_id                    << "\n" <<
                     "        num_sge:     " << wr.num_sge                  << "\n" <<
                     "        opcode:      " << wrOpcodeToString(wr.opcode) << "\n";

            //----------------//
            // ADD RDMA ATTRS //
            //----------------//
        if (is_rdma) {
            wr.wr.rdma.remote_addr = remote_addrs[wr_i].start_addr;
            wr.wr.rdma.rkey        = remote_mr_info.r_key;
            
            if (opcode == IBV_WR_RDMA_READ)
                rdma_counter++;

                //---------//
                // SUCCESS //
                //---------//
            std::cout << "        remote_addr: " << wr.wr.rdma.remote_addr << "\n" <<
                         "        rkey:        " << wr.wr.rdma.rkey        << "\n";
        }

            //---------------//
            // ADD IMM ATTRS //
            //---------------//
        if (with_imm) {
            wr.imm_data = htonl(wr_i);

                //---------//
                // SUCCESS //
                //---------//
            std::cout << "        imm_data:    " << wr.imm_data << "\n";
        }
        std::cout << "}" << "\n";

            //------------------------//
            // BATCH LIST PREPARATION //
            //------------------------//
        if (wr_i + 1 < max_batch_wr)
            wr.next = &wrs[wr_i + 1];
        else
            wr.next = nullptr;

            //-------------------------//
            // SEND EXP DATA TO TARGET //
            //-------------------------//
        RecvExp exp{};
        exp.recv_wr_required = !is_rdma || with_imm;
        exp.with_imm         = with_imm;
        exp.with_payload     = with_payload;
        exp.crc32            = crc32;
        exp.wr_id            = wr_i;
        exp.opcode           = opcode;
        channel.send(exp);

            //---------------------------//
            // RECV EXP DATA FROM TARGET //
            //---------------------------//
        if (!with_payload) {
            exps[wr_i] = channel.receive<RecvExp>();
        }
        
        std::cout << "\n";
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////// POST SEND PROCESSING /////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INITIATOR] POST SEND PROCESSING" << "\n";
    
        //------------------------------//
        // WAITING FOR TARGET POST RECV //
        //------------------------------//
    bool ready = channel.receive<bool>();
    if (!ready)
        throw std::runtime_error("[ERROR][INITIATOR] remote side error");

        //-----------//
        // POST SEND //
        //-----------//
    ibv_send_wr *bad_wr = nullptr;
    
    ret = ibv_post_send(
        qp,
        &wrs[0],
        &bad_wr
    );
    if (ret)
        throw std::runtime_error("[ERROR][INITATOR] ibv_post_send() failed");
    
    if (bad_wr != nullptr)
        throw std::runtime_error("[ERROR][INITIATOR] bad_wr occured while processing ibv_post_send()");
    
        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    wrs posted to SQ successfully" << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////// CHECK WC PROCESSING //////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[INITIATOR] CHECK WC PROCESSING" << "\n";

    for (int wr_i = 0; wr_i < max_batch_wr; wr_i++) {

            //---------//
            // POLL WC //
            //---------//
        ibv_wc wc{};
	    int num_polled = 0;
			
	    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	    bool timeout  = true;
        
        while (std::chrono::steady_clock::now() < deadline) {
	    	num_polled = ibv_poll_cq(
			    cq,
		    	1,
			    &wc
		    );
		
		    if (num_polled > 0) {
			    timeout = false;
			    break;
		    }
	    }

        if (timeout) 
		    throw std::runtime_error("[ERROR][INITIATOR] ibv_poll_cq() timout error");

            //----------//
            // WC CHECK //
            //----------//
        std::cout << "    wc " << wr_i << " check" << "\n";

            //--------------//
            // STATUS CHECK //
            //--------------//
        if (wc.status != IBV_WC_SUCCESS) 
		    throw std::runtime_error(
                "[ERROR][INITIATOR] wc.status: " + std::to_string(wc.status) + "\n" +
                "is not matching exp:          " + std::to_string(IBV_WC_SUCCESS)
                );

            //---------//
            // SUCCESS //
            //---------//
        std::cout << "        status: " << wc.status <<" exp: " << IBV_WC_SUCCESS << " | check passed" << "\n";
            
            //-------------//
            // WR_ID CHECK //
            //-------------//
	    if (wc.wr_id != wrs[wr_i].wr_id) 
		    throw std::runtime_error(
                "[ERROR][INITIATOR] wc.wr_id: " + std::to_string(wc.wr_id) + "\n" + 
                "is not matching wr.wr_id:    " + std::to_string(wrs[wr_i].wr_id)
                );

        std::cout << "        wr_id:  " << wc.wr_id << " exp: " << wrs[wr_i].wr_id << " | check passed" << "\n";
            
            //--------------//
            // OPCODE CHECK //
            //--------------//
	    ibv_wc_opcode expected_opcode;
	    switch (wrs[wr_i].opcode) {
		    case IBV_WR_SEND:
			    expected_opcode = IBV_WC_SEND;
			    break;
            case IBV_WR_SEND_WITH_IMM:
                expected_opcode = IBV_WC_SEND;
                break;
            case IBV_WR_RDMA_WRITE:
			    expected_opcode = IBV_WC_RDMA_WRITE;
		    	break;
            case IBV_WR_RDMA_WRITE_WITH_IMM:
                expected_opcode = IBV_WC_RDMA_WRITE;
                break;
		    case IBV_WR_RDMA_READ:
			    expected_opcode = IBV_WC_RDMA_READ;
		    	break;
		    default:
			    throw std::runtime_error("[ERROR][INITIATOR] unexpected opcode error");
	    }
	    if (wc.opcode != expected_opcode) 
		    throw std::runtime_error(
                "[ERROR][INITIATOR] wc.opcode: "    + wcOpcodeToString(wc.opcode) + "\n" +
                "is not matching expected opcode: " + wcOpcodeToString(expected_opcode)
                );

            //---------//
            // SUCCESS //
            //---------//
        std::cout << "        opcode: " << wcOpcodeToString(wc.opcode) << " exp: " << wcOpcodeToString(expected_opcode) << " | check passed" << "\n";

            //-----------//
            // CRC CHECK //
            //-----------//
        if (exps[wr_i].with_payload) {
            uint32_t crc32 = calcCrc32(addrs[wr_i].start_addr, addrs[wr_i].length);
            if (crc32 != exps[wr_i].crc32) 
                throw std::runtime_error(
                    "[ERROR][INITIATOR] local crc32: " + std::to_string(crc32) + "\n" +
                    "is not matching remote crc32:   " + std::to_string(exps[wr_i].crc32)
                    );
                
                //---------//
                // SUCCESS //
                //---------//
            std::cout << "        crc32:  " << crc32 << " exp: " << exps[wr_i].crc32 << " | check passed" << "\n";
        }
        std::cout << "\n";
    }

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "[INITIATOR] ALL WCS ARE PASSED CHECKS" << "\n\n";

        //-------------------//
        // SEND TARGET READY //
        //-------------------//
    channel.send(true);
}

void RC::target() {
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////// START PROCESSING ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[TARGET] START PROCESSING" << "\n\n";

    if (is_initiator)
        throw std::runtime_error("[ERROR][TARGET] access to target() denied");

    int ret;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////// WRS GENERATION //////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[TARGET] WRS GENERATION" << "\n";

    std::vector<ibv_recv_wr>          wrs(max_batch_wr);
    std::vector<std::vector<ibv_sge>> sges(max_batch_wr);
    std::vector<RecvExp>              exps(max_batch_wr);
    
    std::random_device                 sge_rd;
    std::mt19937                       sge_gen(sge_rd());
    std::uniform_int_distribution<int> sge_dist(1, max_sge);

        //----------//
        // WRS LOOP //
        //----------//
    int prev_wr_i  = -1;
    int first_wr_i = -1;
    for (int wr_i = 0; wr_i < max_batch_wr; wr_i++) {
        
        //------------------------------//
        // RECV EXP DATA FROM INITIATOR //
        //------------------------------//
        exps[wr_i] = channel.receive<RecvExp>();

            //-------------------------------------------------------//
            // FILL BUFFER AND CALCULATE AND SEND CRC32 TO INITIATOR //
            //-------------------------------------------------------//
        if (!exps[wr_i].with_payload) {
            fillBuffer(addrs[wr_i].start_addr, addrs[wr_i].length, wr_i);
            uint32_t crc32 = calcCrc32(addrs[wr_i].start_addr, addrs[wr_i].length);
            
            RecvExp exp{};
            exp.with_payload     = true;
            exp.crc32            = crc32;
            channel.send(exp);
        }

        if (!exps[wr_i].recv_wr_required) {
            continue;
        }

        std::cout << "{"     << "\n";
        std::cout << "    {" << "\n";

            //-------------------------------------//
            // GET SGE AMOUNT AND CALCULATE LENGTH //
            //-------------------------------------//
        int sge_amount = sge_dist(sge_gen);
        int sge_length = addrs[wr_i].length / sge_amount;
        sges[wr_i].resize(sge_amount);
            
            //----------//
            // SGE LOOP //
            //----------//
        for (int sge_i = 0; sge_i < sge_amount; sge_i++) {

                //------------//
                // CREATE SGE //
                //------------//
            ibv_sge &sge = sges[wr_i][sge_i];
            sge.addr   = reinterpret_cast<uint64_t>(addrs[wr_i].start_addr) + (sge_length * sge_i);
            sge.length = sge_length;
            sge.lkey   = mr->lkey;

                //----------------------------//
                // LAST SGE LENGTH CORRECTION //
                //----------------------------//
            if (sge_i == sge_amount-1)
                sge.length = addrs[wr_i].length - (sge_length * sge_i);
                
                //---------//
                // SUCCESS //
                //---------//
            std::cout << "        sge created"                << "\n" <<
                         "            num:    " << sge_i + 1  << "\n" <<  
                         "            addr:   " << sge.addr   << "\n" << 
                         "            length: " << sge.length << "\n" <<
                         "            lkey:   " << sge.lkey   << "\n";
        }
        std::cout << "    }" << "\n";

            //-----------//
            // CREATE WR //
            //-----------//
        ibv_recv_wr &wr = wrs[wr_i];
        wr.wr_id        = wr_i;
        wr.sg_list      = sges[wr_i].data();
        wr.num_sge      = sge_amount;

            //---------//
            // SUCCESS //
            //---------//
        std::cout << "    wr created"                  << "\n" <<
                     "        wr_id:   " << wr.wr_id   << "\n" <<
                     "        num_sge: " << wr.num_sge << "\n";

        std::cout << "}" << "\n";

            //------------------------//
            // BATCH LIST PREPARATION //
            //------------------------//
        if (prev_wr_i >= 0)
            wrs[prev_wr_i].next = &wr;
        else 
            first_wr_i = wr_i;  
        
        std::cout << "\n";
        prev_wr_i = wr_i;
    }

        //------------------------//
        // BATCH LIST PREPARATION //
        //------------------------//
    if (prev_wr_i > 0)
        wrs[prev_wr_i].next = nullptr;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////// POST RECV PROCESSING ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[TARGET] POST RECV PROCESSING" << "\n";

        //-----------//
        // POST RECV //
        //-----------//
    ibv_recv_wr *bad_wr = nullptr;
    
    ret = ibv_post_recv(
        qp,
        &wrs[first_wr_i],
        &bad_wr
    );
    if (ret)
        throw std::runtime_error("[ERROR][TARGET] ibv_recv_send() failed");
    
    if (bad_wr != nullptr)
        throw std::runtime_error("[ERROR][TARGET] bad_wr occured while processing ibv_recv_send()");
    
        //---------//
        // SUCCESS //
        //---------//
    std::cout << "    wrs posted to RQ successfully" << "\n\n";

        //-------------------------//
        // SEND READY TO INITIATOR //
        //-------------------------//
    bool ready = true;
    channel.send(ready);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////// CHECK WC PROCESSING ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[TARGET] CHECK WC PROCESSING" << "\n";

    for (int wr_i = 0; wr_i < max_batch_wr; wr_i++) {

        if (!exps[wr_i].recv_wr_required) {
            continue;
        }    

            //---------//
            // POLL WC //
            //---------//
        ibv_wc wc{};
	    int num_polled = 0;
			
	    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	    bool timeout  = true;
        
        while (std::chrono::steady_clock::now() < deadline) {
	    	num_polled = ibv_poll_cq(
			    cq,
		    	1,
			    &wc
		    );
		
		    if (num_polled > 0) {
			    timeout = false;
			    break;
		    }
	    }

        if (timeout) 
		    throw std::runtime_error("[ERROR][TARGET] ibv_poll_cq() timout error");

            //----------//
            // CHECK WC //
            //----------//
        std::cout << "    wc " << wr_i << " check" << "\n";

            //--------------//
            // CHECK STATUS //
            //--------------//
        if (wc.status != IBV_WC_SUCCESS) 
		    throw std::runtime_error(
                "[ERROR][TARGET] wc.status: " + std::to_string(wc.status) + "\n" + 
                "is not matching exp:       " + std::to_string(IBV_WC_SUCCESS)
                );
            
            //---------//
            // SUCCESS //
            //---------//
        std::cout << "        status:   " << wc.status << " exp: " << IBV_WC_SUCCESS << " | passed check" << "\n";
                
            //-------------//
            // CHECK WC_ID //
            //-------------//
	    if (wc.wr_id != wrs[wr_i].wr_id) 
		    throw std::runtime_error(
                "[ERROR][TARGET] wc.wr_id: " + std::to_string(wc.wr_id) + "\n" 
                "is not matching wr.wr_id: " + std::to_string(wrs[wr_i].wr_id)
                );
            
            //---------//
            // SUCCESS //
            //---------//
        std::cout << "        wr_id:    " << wc.wr_id << " exp: " << wrs[wr_i].wr_id << " | passed check" << "\n";
            
            //--------------//
            // OPCODE CHECK //
            //--------------//
        ibv_wc_opcode expected_opcode = exps[wr_i].opcode == IBV_WR_RDMA_WRITE_WITH_IMM ? IBV_WC_RECV_RDMA_WITH_IMM : IBV_WC_RECV;

	    if (wc.opcode != expected_opcode) 
		    throw std::runtime_error(
                "[ERROR][TARGET] wc.opcode:       " + wcOpcodeToString(wc.opcode) + "\n" +  
                "is not matching expected opcode: " + wcOpcodeToString(expected_opcode)
                );

            //---------//
            // SUCCESS //
            //---------//
        std::cout << "        opcode:   " << wcOpcodeToString(wc.opcode) << " exp: " << wcOpcodeToString(expected_opcode) << " | passed check" << "\n";
            
            //----------------//
            // BYTE LEN CHECK //
            //----------------//
        if (wc.byte_len != addrs[wr_i].length)
            throw std::runtime_error(
                "[ERROR][TARGET] wc.byte_len:       " + std::to_string(wc.byte_len) + "\n" +
                "is not matching expected byte_len: " + std::to_string(addrs[wr_i].length)
                );
        
            //---------//
            // SUCCESS //
            //---------//
        std::cout << "        byte_len: " << wc.byte_len << " exp: " << addrs[wr_i].length << " | passed check" << "\n";
            
            //-----------//
            // IMM CHECK //
            //-----------//
        if (exps[wr_i].with_imm) {
            if (wc.imm_data != htonl(exps[wr_i].wr_id))
                throw std::runtime_error(
                    "[ERROR][TARGET] wc.imm_data:       " + std::to_string(wc.imm_data) + "\n" +
                    "is not matching expected imm_data: " + std::to_string(htonl(exps[wr_i].wr_id))
                    );
                
                //---------//
                // SUCCESS //
                //---------//
            std::cout << "        imm_data: " << wc.imm_data << " exp: " << htonl(exps[wr_i].wr_id) << " | passed check" << "\n";
        }
        std::cout << "\n";
    }

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "[TARGET] ALL WCS ARE PASSED CHECKS" << "\n\n";

        //-------------------------------//
        // RDMA WRITE LAST WR PROTECTION //
        //-------------------------------//
    if (!channel.receive<bool>()) 
        throw std::runtime_error("[ERROR][TARGET] remote error");

    for (int wr_i = 0; wr_i < max_batch_wr; wr_i++) {
            //-----------//
            // CRC CHECK //
            //-----------//
        if (exps[wr_i].with_payload) {
            std::cout << "    wr " << wr_i << " crc check" << "\n";
            uint32_t crc32 = calcCrc32(addrs[wr_i].start_addr, addrs[wr_i].length);
            if (crc32 != exps[wr_i].crc32) 
                throw std::runtime_error(
                    "[ERROR][TARGET] local crc32:  " + std::to_string(crc32) + "\n" +
                    "is not matching remote crc32: " + std::to_string(exps[wr_i].crc32)
                    );
                
                //---------//
                // SUCCESS //
                //---------//
            std::cout << "        crc32:  " << crc32 << " exp: " << exps[wr_i].crc32 << " | check passed" << "\n\n";
        }
    }

        //---------//
        // SUCCESS //
        //---------//
    std::cout << "[TARGET] ALL CRC CHECKS ARE PASSED" << "\n\n";
}

void RC::exit() {
    std::cout << "[EXIT] PROCESSING EXIT" << "\n";

	int ret = 0;
	int err = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // LOG ------------- DESTROY QP ////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[EXIT] DESTROY QP" << "\n"; 

	if (qp != nullptr) {
		ret = ibv_destroy_qp(qp);
		if (ret) {
			err++;
            std::cout << "[ERROR][EXIT] QP DESTRUCTION FAILED" << "\n\n";
		} 
        else 
            std::cout << "qp destroyed successfully" << "\n\n";
		qp = nullptr;
	}
    else
        std::cout << "qp is nullptr" << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // LOG ------------- DESTROY CQ ////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[EXIT] DESTROY CQ" << "\n";
    
	if (cq != nullptr) {
		ret = ibv_destroy_cq(cq);
		if (ret) {
			err++;
            std::cout << "[ERROR][EXIT] CQ DESTRUCTION FAILED" << "\n\n";
		} 
        else
            std::cout << "cq destroyed successfully" << "\n\n"; 
		cq = nullptr;
	}
    else 
        std::cout << "cq is nullptr" << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // LOG ------------- DEREGISTER MR /////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[EXIT] DEREGISTER MR" << "\n";
    
	if (mr != nullptr) {
		ret = ibv_dereg_mr(mr);
		if (ret) {
			err++;
            std::cout << "[ERROR][EXIT] MR DEREGISTRATION FAILED" << "\n";
        }
        else 
            std::cout << "mr deregistered successfully" << "\n\n";
		mr = nullptr;
	}
    else 
        std::cout << "mr is nullptr" << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // LOG ------------- FREE BUFFER ///////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[EXIT] FREE BUFFER" << "\n";
    
	if (buffer != nullptr) {
		std::free(buffer);
		buffer = nullptr;
        
        std::cout << "buffer freed successfully" << "\n\n";
	}
    else 
        std::cout << "buffer is nullptr" << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // LOG ------------- DEALLOCATE PD /////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[EXIT] DEALLOCATE PD" << "\n";

	if (pd != nullptr) {
		ret = ibv_dealloc_pd(pd);
		if (ret) {
			err++;
            std::cout << "[ERROR][EXIT] PD DEALLOCATION FAILED" << "\n\n";
		}
        else 
            std::cout << "pd deallocated successfully" << "\n\n";
		pd = nullptr;
	}
    else 
        std::cout << "pd is nullptr" << "\n\n";

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // LOG ------------- CLOSE DEVICE //////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << "[EXIT] CLOSE DEVICE" << "\n";

	if (context != nullptr) {
		ret = ibv_close_device(context);
		if (ret) {
			err++;
            std::cout << "[ERROR][EXIT] DEVICE CLOSING FALIED" << "\n\n";
		}
        else 
            std::cout << "device closed successfully" << "\n\n";
		context = nullptr;
	}
    else
        std::cout << "context is nullptr" << "\n\n";
	
	if (err)
        std::cout << "[ERROR][EXIT] " << err << " ERRORS HAPPENED DURING EXIT" << "\n\n";
}

uint32_t RC::calcCrc32(void* buff_start_addr, size_t length) {
    const auto* bytes = static_cast<const uint8_t*>(buff_start_addr);

    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; ++i)
    {
        crc ^= bytes[i];

        for (int bit = 0; bit < 8; ++bit)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }

    return crc ^ 0xFFFFFFFF;
}

void RC::fillBuffer(void* buff_start_addr, size_t length, uint64_t wr_id) {
    auto* ptr = static_cast<uint64_t*>(buff_start_addr);

    size_t count = length / sizeof(uint64_t);

    for (size_t i = 0; i < count; ++i)
        ptr[i] = wr_id;

    size_t remainder = length % sizeof(uint64_t);

    if (remainder != 0)
    {
        std::memcpy(
            reinterpret_cast<uint8_t*>(ptr + count),
            &wr_id,
            remainder
        );
    }
}

std::string RC::wrOpcodeToString(ibv_wr_opcode opcode) {
    std::string ret = "";
    
    switch(opcode) {
        case IBV_WR_SEND:
            ret = "IBV_WR_SEND";
            break;
        case IBV_WR_SEND_WITH_IMM:
            ret = "IBV_WR_SEND_WITH_IMM";
            break;
        case IBV_WR_RDMA_WRITE:
            ret = "IBV_WR_RDMA_WRITE";
            break;
        case IBV_WR_RDMA_WRITE_WITH_IMM:
            ret = "IBV_WR_RDMA_WRITE_WITH_IMM";
            break;
        case IBV_WR_RDMA_READ:
            ret = "IBV_WR_RDMA_READ";
    }

    return ret;
}

std::string RC::wcOpcodeToString(ibv_wc_opcode opcode) {
    std::string ret = "";

    switch(opcode) {
        case IBV_WC_SEND:
            ret = "IBV_WC_SEND";
            break;
        case IBV_WC_RDMA_WRITE:
            ret = "IBV_WC_RDMA_WRITE";
            break;
        case IBV_WC_RDMA_READ:
            ret = "IBV_WC_RDMA_READ";
            break;
        case IBV_WC_RECV:
            ret = "IBV_WC_RECV";
            break;
        case IBV_WC_RECV_RDMA_WITH_IMM:
            ret = "IBV_WC_RECV_RDMA_WITH_IMM";
    }

    return ret;
}