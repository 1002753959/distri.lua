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

#define MAX_NETPOLLER 64         //���POLLER����

struct poller_st
{
    msgque_t         mq_in;          //���ڽ��մ��߼����������Ϣ
    netservice*      netpoller;      //�ײ��poller
    thread_t         poller_thd;
    atomic_32_t      flag;
};

typedef struct asynnet
{
    uint32_t  poller_count;
    struct poller_st      netpollers[MAX_NETPOLLER];
    atomic_32_t           flag;
}*asynnet_t;


asynnet_t asynnet_new(uint8_t  pollercount);

void       asynnet_stop(asynnet_t);

void       asynnet_coronet(asynnet_t);

int32_t    asynsock_close(sock_ident);

int32_t    get_addr_local(sock_ident,char *buf,uint32_t buflen);
int32_t    get_addr_remote(sock_ident,char *buf,uint32_t buflen);

int32_t    get_port_local(sock_ident,int32_t *port);
int32_t    get_port_remote(sock_ident,int32_t *port);

#endif
