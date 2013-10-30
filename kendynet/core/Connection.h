#ifndef _CONNECTION_H
#define _CONNECTION_H
#include "KendyNet.h"
#include "wpacket.h"
#include "rpacket.h"
#include <stdint.h>
#include "timing_wheel.h"
#include "allocator.h"
#include "common_define.h"
#include "common.h"
#include "refbase.h"

struct connection;
struct OVERLAPCONTEXT
{
	st_io   m_super;
	struct  connection *c;
};


typedef void (*process_packet)(struct connection*,rpacket_t);
typedef void (*on_disconnect)(struct connection*,uint32_t reason);
typedef void (*packet_send_finish)(struct connection*,wpacket_t);
typedef void (*on_recv_timeout)(struct connection*);
typedef void (*on_send_timeout)(struct connection*);

#define MAX_WBAF 512
#define MAX_SEND_SIZE 8192

struct send_finish
{
    list_node lnode;
    wpacket_t wpk;
    packet_send_finish _send_finish;
};

struct connection
{
	struct refbase ref;
	SOCK socket;
	struct iovec wsendbuf[MAX_WBAF];
	struct iovec wrecvbuf[2];
	struct OVERLAPCONTEXT send_overlap;
	struct OVERLAPCONTEXT recv_overlap;

	uint32_t unpack_size; //��δ��������ݴ�С
	uint32_t unpack_pos;
	uint32_t next_recv_pos;
	buffer_t next_recv_buf;
	buffer_t unpack_buf;

	struct link_list send_list;//�����͵İ�
	struct link_list on_send_finish;
	process_packet _process_packet;
	on_disconnect  _on_disconnect;
	union{
        uint64_t usr_data;
        void    *usr_ptr;
	};
	uint32_t last_recv;
	struct   WheelItem wheelitem;
	uint32_t recv_timeout;
    uint32_t send_timeout;
    on_recv_timeout _recv_timeout;
    on_send_timeout _send_timeout;
	uint8_t  raw;
	volatile uint8_t is_closed;
};


static inline WheelItem_t con2wheelitem(struct connection *con){
    return &con->wheelitem;
}

static inline struct connection *wheelitem2con(WheelItem_t wit)
{
    struct connection *con = (struct connection*)wit;
    return (struct connection *)((char*)wit - ((char*)&con->wheelitem - (char*)con));
}

struct connection *new_conn(SOCK s,uint8_t is_raw);
void   release_conn(struct connection *con);
void   acquire_conn(struct connection *con);

void   active_close(struct connection*);//active close connection

int32_t send_packet(struct connection*,wpacket_t,packet_send_finish);

int32_t bind2engine(ENGINE,struct connection*,process_packet,on_disconnect);

#endif
