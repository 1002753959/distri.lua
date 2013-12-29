/*
*   ���߳��첽�����ܣ����߼�������������
*   ����֮��ͨ����Ϣ����ͨ��
*/

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

typedef struct asynnet* asynnet_t;

#define MAX_NETPOLLER 64         //���POLLER����

/* ���ӳɹ���ص�����ʱ���ӻ�δ�󶨵�ͨ��poller,�����շ���Ϣ
*  �û�����ѡ��ֱ�ӹرմ����������ӣ����߽����Ӱ󶨵�ͨ��poller
*/

typedef void (*ASYNCB_CONNECT)(asynnet_t,sock_ident,const char *ip,int32_t port);

/*
*  �󶨵�ͨ��poller�ɹ���ص��ã���ʱ���ӿ������շ���Ϣ
*/
typedef void (*ASYNCB_CONNECTED)(asynnet_t,sock_ident,const char *ip,int32_t port);

/*
*  ���ӶϿ���ص���
*/
typedef void (*ASYNCB_DISCNT)(asynnet_t,sock_ident,const char *ip,int32_t port,uint32_t err);


/*
*   ����1��coro_process_packet���ú�rpacket_t�Զ�����
*   ����,����ʹ�����Լ�����
*/
typedef int32_t (*ASYNCB_PROCESS_PACKET)(asynnet_t,sock_ident,rpacket_t);


typedef void (*ASYNCN_CONNECT_FAILED)(asynnet_t,const char *ip,int32_t port,uint32_t reason);

/*
*  listen�Ļص�������ɹ�����һ���Ϸ���sock_ident
*  ����reason˵��ʧ��ԭ��
*/
typedef void (*ASYNCB_LISTEN)(asynnet_t,sock_ident,const char *ip,int32_t port,uint32_t reason);

asynnet_t asynnet_new(uint8_t  pollercount,
                      ASYNCB_CONNECT,
                      ASYNCB_CONNECTED,
                      ASYNCB_DISCNT,
                      ASYNCB_PROCESS_PACKET,
                      ASYNCN_CONNECT_FAILED,
                      ASYNCB_LISTEN);


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
