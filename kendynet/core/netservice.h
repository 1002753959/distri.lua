/*
    Copyright (C) <2012>  <huangweilook@21cn.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _NETSERVICE_H
#define _NETSERVICE_H
#include "KendyNet.h"
#include "Connection.h"
#include "SysTime.h"
#include "timer.h"

typedef struct netservice{
    ENGINE engine;
    struct timer* timer;
    int32_t (*bind)(struct netservice *,struct connection*,process_packet,on_disconnect,
                    uint32_t,on_recv_timeout,uint32_t,on_send_timeout);
    SOCK    (*listen)(struct netservice*,const char*,int32_t,void*,OnAccept);
    int32_t (*connect)(struct netservice*,const char *ip,int32_t port,void *ud,OnConnect,uint32_t);
    int32_t (*loop)(struct netservice*,uint32_t ms);
	int32_t (*wakeup)(struct netservice*);
}netservice;

struct netservice *new_service();
void   destroy_service(struct netservice**);


#endif
