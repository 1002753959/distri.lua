#ifndef _ASYNNET_H
#define _ASYNNET_H

#include "netservice.h"
#include "thread.h"
#include "msgque.h"

typedef struct sock_ident{
	ident _ident;
}sock_ident;

#define CAST_2_SOCK(IDENT) (*(sock_ident*)&IDENT)

typedef struct msg
{
	list_node next;
	uint8_t   type;
	union{
		uint64_t  usr_data;
		void*     usr_ptr;
		ident     _ident;
	};
	void (*msg_destroy_function)();
}*msg_t;

struct asynnet;
struct poller_st
{
	msgque_t         mq_in;          //���ڽ��մ��߼����������Ϣ 
	netservice*      netpoller;      //�ײ��poller
    thread_t         poller_thd;
    struct asynnet*  _coronet;
	atomic_32_t  flag;
};

#define MAX_NETPOLLER 64         //���POLLER����

/* ���ӳɹ���ص�����ʱ���ӻ�δ�󶨵�ͨ��poller,�����շ���Ϣ
*  �û�����ѡ��ֱ�ӹرմ����������ӣ����߽����Ӱ󶨵�ͨ��poller
*/

typedef void (*coro_on_connect)(struct asynnet*,sock_ident,const char *ip,int32_t port);

/*
*  �󶨵�ͨ��poller�ɹ���ص��ã���ʱ���ӿ������շ���Ϣ
*/
typedef void (*coro_on_connected)(struct asynnet*,sock_ident,const char *ip,int32_t port);

/*
*  ���ӶϿ���ص���
*/
typedef void (*coro_on_disconnect)(struct asynnet*,sock_ident,const char *ip,int32_t port,uint32_t err);


/*
*   ����1��coro_process_packet���ú�rpacket_t�Զ�����
*   ����,����ʹ�����Լ�����
*/
typedef int32_t (*coro_process_packet)(struct asynnet*,sock_ident,rpacket_t);


typedef void (*coro_connect_failed)(struct asynnet*,const char *ip,int32_t port,uint32_t reason);

/*
*  listen�Ļص�������ɹ�����һ���Ϸ���sock_ident
*  ����reason˵��ʧ��ԭ��
*/
typedef void (*coro_listen_ret)(struct asynnet*,sock_ident,const char *ip,int32_t port,uint32_t reason);

typedef struct asynnet
{
	uint32_t  poller_count;
	msgque_t  mq_out;                                 //�������߼��㷢����Ϣ
    struct poller_st    accptor_and_connector;       //��������������
    struct poller_st    netpollers[MAX_NETPOLLER];
	coro_on_connect     _coro_on_connect;
	coro_on_connected   _coro_on_connected;
	coro_on_disconnect  _coro_on_disconnect;
	coro_process_packet _coro_process_packet;
	coro_connect_failed _coro_connect_failed;
	coro_listen_ret     _coro_listen_ret;
	atomic_32_t         flag;
}*asynnet_t;

asynnet_t asynnet_new(uint8_t  pollercount,
	                     coro_on_connect,
						 coro_on_connected,
						 coro_on_disconnect,
						 coro_connect_failed,
						 coro_listen_ret,
						 coro_process_packet);


void      asynnet_stop(asynnet_t);

void      asynnet_coronet(asynnet_t);

int32_t   asynnet_bind(asynnet_t,sock_ident sock,void *ud,int8_t raw,uint32_t send_timeout,uint32_t recv_timeout);

int32_t   asynnet_listen(asynnet_t,const char *ip,int32_t port);

int32_t   asynnet_connect(asynnet_t,const char *ip,int32_t port,uint32_t timeout);

void      peek_msg(asynnet_t,uint32_t timeout);

int32_t   asynsock_close(sock_ident);

int32_t   get_addr_local(sock_ident,char *buf,uint32_t buflen);
int32_t   get_addr_remote(sock_ident,char *buf,uint32_t buflen);

int32_t   get_port_local(sock_ident,int32_t *port);
int32_t   get_port_remote(sock_ident,int32_t *port);

#endif
