#ifndef _CORONET_H
#define _CORONET_H

#include "netservice.h"
#include "thread.h"
#include "msg_que.h"

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

struct coronet;
struct engine_struct
{
	msgque_t         mq_in;          //���ڽ��մ��߼����������Ϣ 
	netservice*      netpoller;      //�ײ��poller
	thread_t         thread_engine;
	struct coronet*  _coronet;
	atomic_32_t  flag;
};

#define MAX_NETPOLLER 64         //���POLLER����

/* ���ӳɹ���ص�����ʱ���ӻ�δ�󶨵�ͨ��poller,�����շ���Ϣ
*  �û�����ѡ��ֱ�ӹرմ����������ӣ����߽����Ӱ󶨵�ͨ��poller
*/

typedef void (*coro_on_connect)(struct coronet*,sock_ident,const char *ip,int32_t port);

/*
*  �󶨵�ͨ��poller�ɹ���ص��ã���ʱ���ӿ������շ���Ϣ
*/
typedef void (*coro_on_connected)(struct coronet*,sock_ident,const char *ip,int32_t port);

/*
*  ���ӶϿ���ص���
*/
typedef void (*coro_on_disconnect)(struct coronet*,sock_ident,const char *ip,int32_t port,uint32_t err);


/*
*   ����1��coro_process_packet���ú�rpacket_t�Զ�����
*   ����,����ʹ�����Լ�����
*/
typedef int32_t (*coro_process_packet)(struct coronet*,sock_ident,rpacket_t);


typedef void (*coro_connect_failed)(struct coronet*,const char *ip,int32_t port,uint32_t reason);

/*
*  listen�Ļص�������ɹ�����һ���Ϸ���sock_ident
*  ����reason˵��ʧ��ԭ��
*/
typedef void (*coro_listen_ret)(struct coronet*,sock_ident,const char *ip,int32_t port,uint32_t reason);

typedef struct coronet
{
	uint32_t  poller_count;
	msgque_t  mq_out;                                 //�������߼��㷢����Ϣ
	struct engine_struct accptor_and_connector;       //��������������
	struct engine_struct netpollers[MAX_NETPOLLER];
	coro_on_connect     _coro_on_connect;
	coro_on_connected   _coro_on_connected;
	coro_on_disconnect  _coro_on_disconnect;
	coro_process_packet _coro_process_packet;
	coro_connect_failed _coro_connect_failed;
	coro_listen_ret     _coro_listen_ret;
	atomic_32_t         flag;
}*coronet_t;

coronet_t create_coronet(uint8_t  pollercount,
	                     coro_on_connect,
						 coro_on_connected,
						 coro_on_disconnect,
						 coro_connect_failed,
						 coro_listen_ret,
						 coro_process_packet);


void      stop_coronet(coronet_t);

void      destroy_coronet(coronet_t);

int32_t   coronet_bind(coronet_t,sock_ident sock,void *ud,int8_t raw,uint32_t send_timeout,uint32_t recv_timeout);

int32_t   coronet_listen(coronet_t,const char *ip,int32_t port);

int32_t   coronet_connect(coronet_t,const char *ip,int32_t port,uint32_t timeout);

void      peek_msg(coronet_t,uint32_t timeout);

int32_t   close_datasock(sock_ident);

int32_t   get_addr_local(sock_ident,char *buf,uint32_t buflen);
int32_t   get_addr_remote(sock_ident,char *buf,uint32_t buflen);

int32_t   get_port_local(sock_ident,int32_t *port);
int32_t   get_port_remote(sock_ident,int32_t *port);

#endif
