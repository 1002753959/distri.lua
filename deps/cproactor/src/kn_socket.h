#ifndef _KN_SOCKET_H
#define _KN_SOCKET_H

#include <stdint.h>
#include "kn_common_include.h"
#include "kn_ref.h"
#include "kn_dlist.h"
#include "kn_sockaddr.h"
#include "kn_common_define.h"


enum{
	readable  = 1,
	writeable = 1 << 1,
};

struct kn_proactor;
typedef struct kn_socket{
	kn_dlist_node       node;
    kn_ref              ref;
	uint8_t             type;
	int32_t             fd;
	void*               ud;
	//uint8_t             status;
	struct kn_proactor* proactor;		
	void (*on_active)(struct kn_socket*,int event);
	int8_t (*process)(struct kn_socket*);//如果process返回非0,则需要重新投入到activedlist中
}kn_socket,*kn_socket_t;

void kn_closesocket(kn_socket_t);

#endif
