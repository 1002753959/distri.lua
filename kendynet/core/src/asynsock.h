#ifndef _ASYNSOCK_H
#define _ASYNSOCK_H
#include "connection.h"
#include "refbase.h"
#include "netservice.h"
#include "double_link.h"
#include "msgque.h"
#include "asynnet.h"

typedef struct asynsock
{
	struct refbase          ref;
	struct double_link_node dn;
	struct connection       *c;
	SOCK                     s;
	void    *usr_ptr;
    sock_ident              sident;
	msgque_t                sndque;//���ڸ�pollerͨ�ŵ���Ϣ����
    msgque_t                que;   //���ڸ�Ӧ�ò�ͨ�ŵ���Ϣ����
}*asynsock_t;


asynsock_t asynsock_new(struct connection *c,SOCK s);

static inline asynsock_t cast_2_asynsock(sock_ident sock)
{
	ident _ident = TO_IDENT(sock);
    struct refbase *r = cast_2_refbase(_ident);
    if(r) return (asynsock_t)r;
	return NULL;
}

static inline void asynsock_release(asynsock_t sock)
{
	ref_decrease((struct refbase*)sock);
}

#endif
