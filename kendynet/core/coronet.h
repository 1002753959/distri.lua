#ifndef _CORONET_H
#define _CORONET_H

#include "netservice.h"
#include "thread.h"
#include "msg_que.h"
#include "datasocket.h"


typedef struct msg
{
	list_node next;
	uint8_t   type;
	union{
		uint64_t  usr_data;
		void*     usr_ptr;
		ident     _ident;
	};
	typedef void (*msg_destroy_function)();
}*msg_t;


struct engine_struct
{
	msgque_t     mq_in;          //���ڽ��մ��߼����������Ϣ 
	netservice   netpoller;      //�ײ��poller
	thread_t     thread_engine;
	atomic_32_t  flag;
};

#define MAX_NETPOLLER 64         //���POLLER����

/* ���ӳɹ���ص�����ʱ���ӻ�δ�󶨵�ͨ��poller,�����շ���Ϣ
*  �û�����ѡ��ֱ�ӹرմ����������ӣ����߽����Ӱ󶨵�ͨ��poller
*/

struct coronet;
typedef void (*coro_on_connect)(struct coronet*,ident);               

/*
*  �󶨵�ͨ��poller�ɹ���ص��ã���ʱ���ӿ������շ���Ϣ
*/
typedef void (*coro_on_connected)(ident);

/*
*  ���ӶϿ���ص���
*/
typedef void (*coro_on_disconnect)(ident,uint32_t err);


/*
*  ������Ϣ�ص�
*/
typedef void (*coro_process_packet)(ident,rpacket_t);


typedef void (*coro_connect_failed)(connect_request *creq,uint32_t);

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
	pthread_key_t       t_key;
	uint32_t            coro_count;
	atomic_32_t         flag;
}*coronet_t;

coronet_t create_coronet(uint8_t  pollercount,
	                     uint32_t coro_count,   //Э�̳صĴ�С
	                     coro_on_connect,
						 coro_on_connected,
						 coro_on_disconnect,
						 coro_connect_failed,
						 coro_process_packet);


void      stop_coronet(coronet_t);

void      destroy_coronet(coronet_t);

int32_t   coronet_bind(coronet_t,ident sock,int8_t raw,uint32_t send_timeout,uint32_t recv_timeout);

int32_t   coronet_listen(coronet_t,const char*,int32_t);

int32_t   coronet_connect(coronet_t,connect_request *creq,uint32_t);

void      peek_msg(coronet_t,uint32_t timeout);

#endif