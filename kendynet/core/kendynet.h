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
#ifndef _KENDYNET_H
#define _KENDYNET_H
#include <stdint.h>
#include "link_list.h"
#include "common_define.h"

//����ϵͳ֧�ֵ�����׽�������
#define MAX_SOCKET 4096

typedef void* ENGINE;
#define INVALID_ENGINE NULL
typedef void* SOCK;
#define INVALID_SOCK NULL

#include "sock_util.h"

/*IO�������ɶ���ʹ�õĽṹ*/
typedef struct
{
	LIST_NODE;
	struct     iovec *iovec;
	int32_t    iovec_count;
}st_io;

typedef struct 
{
	char     ip[32];
	int32_t  port;
	union{
		uint64_t  usr_data;
		void     *usr_ptr;
	};
}connect_request;

//��ʼ������ϵͳ
int32_t      InitNetSystem();

void   CleanNetSystem();
//IO�������ʱcallback
typedef void (*OnIoFinish)(int32_t,st_io*,uint32_t err_code);
//���ӹر�ʱ,������δ��ɵ�����ִ�е�callback
typedef void (*OnClearPending)(st_io*);

typedef void (*OnAccept)(SOCK,struct sockaddr_in*,void *ud);
typedef void (*OnConnect)(SOCK,struct sockaddr_in*,void *ud,int err);

ENGINE   CreateEngine();
void     CloseEngine(ENGINE);
int32_t  EngineRun(ENGINE,int32_t timeout);
int32_t  Bind2Engine(ENGINE,SOCK,OnIoFinish,OnClearPending);

//when you want to stop listen,CloseSocket(SOCK)
SOCK EListen(ENGINE,const char *ip,int32_t port,void *ud,OnAccept);

//if nonblock connect ms > 0
int32_t EConnect(ENGINE,const char *ip,int32_t port,void *ud,OnConnect,uint32_t ms);

int32_t EWakeUp(ENGINE);


/* return:
*  0,  io pending
*  >0, bytes transfer
*  0<, socket disconnect or error
*/
int32_t Recv(SOCK,st_io*,uint32_t *err_code);
int32_t Send(SOCK,st_io*,uint32_t *err_code);

/*
* return:
* 0, success
* 0<,socket disconnect
*/

int32_t Post_Recv(SOCK,st_io*);
int32_t Post_Send(SOCK,st_io*);

#endif
