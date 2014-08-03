#include "kendynet.h"
#include "lua_util.h"

#define MAX_BUFSIZE  1024
#define MAX_WSEND_SIZE  1024

#define SOCKET_METATABLE "socket_metatable"

static __thread engine_t g_engine = NULL;
//static __lua_State *g_L;

typedef struct bytebuffer{
	struct bytebuffer *next;
	int    cap;
	int    size;
	char   data[0];
}*bytebuffer_t;

bytebuffer_t new_bytebuffer(int cap)
{
	bytebuffer_t b = calloc(1,sizeof(*b)+sizeof(char)*cap);
	b->cap = cap;
	b->size = 0;
	return b;
}

static inline int bytebuffer_read(bytebuffer_t b,uint32_t pos,int8_t *out,uint32_t size)
{
	uint32_t copy_size;
	while(size){
        if(!b) return -1;
        if(pos >= b->size) return -1;
        copy_size = b->size - pos;
		copy_size = copy_size > size ? size : copy_size;
		memcpy(out,&b->data[pos],copy_size);
		size -= copy_size;
		pos += copy_size;
		out += copy_size;
		if(pos >= b->size){
			pos = 0;
			b = b->next;
		}
	}
	return 0;
}

typedef struct lua_socket{
	handle_t     sock;		
	st_io        send_overlap;
	st_io        recv_overlap;
	struct       iovec wrecvbuf[2];
	struct       iovec wsendbuf[MAX_WSEND_SIZE];
	uint8_t      is_close;	
	kn_list      send_list;
	uint32_t     unpack_size; //还未解包的数据大小
	uint32_t     unpack_pos;
	uint32_t     recv_pos;
	bytebuffer_t recv_buf;
	bytebuffer_t unpack_buf;
	luaTabRef_t  callback;
	char         name[512];  //use by lua
	char         packet[MAX_BUFSIZE];	
}*lua_socket_t;

inline static lua_socket_t _getsock(lua_State *L, int index) {
    lua_socket_t sock = (lua_socket_t)luaL_checkudata(L, index, SOCKET_METATABLE);
    return sock;
}

inline static void _setsock(lua_State *L, handle_t sock) {
    lua_socket_t nsock = (lua_socket_t) lua_newuserdata(L, sizeof(*nsock));
    luaL_getmetatable(L, SOCKET_METATABLE);
    lua_setmetatable(L, -2);
    nsock->sock = sock;
    kn_sock_setud(sock,nsock);
}

static int _socket(lua_State *L) {
    int domain    = luaL_checkint(L, 1);
    int type      = luaL_checkint(L, 2);
    int protocol  = luaL_optint(L,3,0);

	handle_t sock = kn_new_sock(domain,type,protocol);
	if(!sock){ 
		lua_pushnil(L);
		return 1;
	}
    _setsock(L,sock);
    return 1;
}
   
void on_connect(handle_t s,int err,void *ud)
{
	lua_socket_t sock = (lua_socket_t)kn_sock_getud(s);
	luaTabRef_t *callback = &sock->callback;
	lua_State *L = luaTabRef_LState(*callback);	
	lua_rawgeti(L,LUA_REGISTRYINDEX,obj->rindex);
	lua_pushstring(L,"on_connect");
	lua_gettable(L,-2);
	lua_pushstring(L,"sock");
	lua_gettable(L,-2);		
	lua_remove(L,-3);
	
	lua_pushinteger(L,err);
	if(0 != lua_pcall(L,2,0,0)){
		const char * error = lua_tostring(L, -1);
		printf("on_connect:%s\n",error);
		lua_pop(L,1);		
	}
	release_luaTabRef(callback);
}	

int _sock_connect(lua_State *L){
    lua_socket_t sock = _getsock(L, 1);
    const char *host = luaL_checkstring(L, 2);
    luaL_checkint(L, 3);
	int port = lua_tointeger(L, 3);
	luaTabRef_t callback = create_luaTabRef(L,4);
	if(!sock || !host || !callback){
		lua_pushstring(L,"invaild arg");
		return 1;
	}
	kn_sockaddr remote;
	kn_addr_init_in(&remote,host,port);
	sock->callback = callback;	
	if(0 != kn_sock_connect(g_engine,sock->sock,&remote,NULL,on_connect,NULL){
		release_luaTabRef(&sock->callback);
		lua_pushstring(L,"connect error");
	}else
		lua_pushnil(L);
	return 1;	
}

static void transfer_finish(handle_t s,st_io *io,int32_t bytestransfer,int32_t err){
	
}

static void on_accept(handle_t s,void *ud){
	luaTabRef_t *callback = (luaTabRef_t*)ud;
	lua_rawgeti(callback->L,LUA_REGISTRYINDEX,obj->rindex);
	lua_pushstring(callback->L,"on_newclient");
	lua_gettable(callback->L,-2);
	if(callback->L != g_L){ 
		lua_xmove(callback->L,g_L,1);
		lua_remove(callback->L,-1);
	}else{
		lua_remove(callback->L,-2);
	}		
	_setsock(g_L,s);
	if(0 != lua_pcall(g_L,1,0,0)){
		const char * error = lua_tostring(g_L, -1);
		printf("on_accept:%s\n",error);
		lua_pop(g_L,1);
		kn_close_sock(s);		
	}				
}

int _sock_listen(lua_State *L){
    lua_socket_t sock = _getsock(L, 1);
    const char *host = luaL_checkstring(L, 2);
    luaL_checkint(L, 3);
	int port = lua_tointeger(L, 3);
	luaTabRef_t callback = create_luaTabRef(L,4);
	if(!sock || !host || !callback){
		lua_pushstring(L,"invaild arg");
		return 1;
	}
	sock->callback = callback;
	kn_sockaddr local;
	kn_addr_init_in(&local,host,port);		
	if(0 != kn_sock_listen(g_engine,sock->sock,&local,on_accept,&sock->callback)){
		release_luaTabRef(&sock->callback);
		lua_pushstring(L,"listen error");
	}else
		lua_pushnil(L);
	return 1;
}

int reg_socket_c(lua_State *L){
    luaL_Reg socket_mt[] = {
        {"__gc", _sock_close},
        //{"__tostring",  _sock_tostring},
        {NULL, NULL}
    };
    
    luaL_Reg socket_methods[] = {
        {"connect", _sock_connect},
        {"recv",_sock_recv},
        {"send",_sock_send},
        {"listen",_sock_listen},
        {"accept",_sock_accept},
        {"close",_sock_close},
        {NULL, NULL}
    };
    luaL_newmetatable(L, SOCKET_METATABLE);
    luaL_setfuncs(L, socket_mt, 0);
    luaL_newlib(L, socket_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
    
	lua_newtable(L);
	
	lua_pushstring(L,"socket");
	lua_pushcfunction(L,&_socket);
	lua_settable(L, -3);
				
	lua_setglobal(L,"socket");    
}





/*#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "lua/lua_util.h"
#include "kendynet.h"
#include "kn_time.h"
#include "kn_ref.h"
#include "kn_dlist.h"
#include "kn_list.h"
#include "kn_time.h"
#include "kn_timer.h"
#include "kn_thread.h"

#define MAX_BUFSIZE  1024
#define MAX_WSEND_SIZE  1024

static __thread kn_proactor_t g_proactor = NULL;
static __thread lua_State*    g_L = NULL;
static __thread kn_channel_t  channel;

static int           recv_sigint = 0;
static int           recv_count = 0;

static void sig_int(int sig){
	recv_sigint = 1;
}

typedef struct bytebuffer{
	struct bytebuffer *next;
	int    cap;
	int    size;
	char   data[0];
}*bytebuffer_t;

bytebuffer_t new_bytebuffer(int cap)
{
	bytebuffer_t b = calloc(1,sizeof(*b)+sizeof(char)*cap);
	b->cap = cap;
	b->size = 0;
	return b;
}

static inline int bytebuffer_read(bytebuffer_t b,uint32_t pos,int8_t *out,uint32_t size)
{
	uint32_t copy_size;
	while(size){
        if(!b) return -1;
        if(pos >= b->size) return -1;
        copy_size = b->size - pos;
		copy_size = copy_size > size ? size : copy_size;
		memcpy(out,&b->data[pos],copy_size);
		size -= copy_size;
		pos += copy_size;
		out += copy_size;
		if(pos >= b->size){
			pos = 0;
			b = b->next;
		}
	}
	return 0;
}

enum{
	LUA_DATASOCKET = 1,
	LUA_LISTENER,
};

enum{
	NET_DATA = 1,
	AFTER_ACCEPT,
	CONNECT
};

typedef struct lua_socket{
	uint8_t type;
	kn_fd_t sock;
	luaObject_t  callbackObj;//存放lua回调函数
	char         name[512];  //use by lua	
}*lua_socket_t;

typedef struct lua_data_socket{
	struct lua_socket base;
	void        (*fd_destroy_fn)(void *arg);		
	st_io        send_overlap;
	st_io        recv_overlap;
	struct       iovec wrecvbuf[2];
	struct       iovec wsendbuf[MAX_WSEND_SIZE];
	uint8_t      is_close;
	//uint8_t      status;	
	kn_list      send_list;
	uint32_t     unpack_size; //还未解包的数据大小
	uint32_t     unpack_pos;
	uint32_t     recv_pos;
	bytebuffer_t recv_buf;
	bytebuffer_t unpack_buf;
	char         name[512];  //use by lua
	char         packet[MAX_BUFSIZE];	
}*lua_data_socket_t;

typedef struct lua_listener{
	struct lua_socket base;
}*lua_listener_t;

typedef struct sendbuf{
	kn_list_node node;
	kn_sockaddr  remote;
	int          index;
	int          size;
	char         buf[0];
}sendbuf;

static void datasocket_destroyer(void *ptr){
	kn_fd_t fd = (kn_fd_t)((char*)ptr - sizeof(kn_dlist_node));
	lua_data_socket_t l = (lua_data_socket_t)kn_fd_getud(fd);	
	release_luaObj(l->base.callbackObj);
	while(kn_list_size(&l->send_list)){
		sendbuf *buf = (sendbuf*)kn_list_pop(&l->send_list);
		free(buf);
	}
	while(l->unpack_buf){
		bytebuffer_t tmp = l->unpack_buf;
		l->unpack_buf = l->unpack_buf->next;
		free(tmp);
	}		
	l->fd_destroy_fn(ptr);
	free(l);
}

static lua_listener_t new_listener(kn_fd_t sock,luaObject_t callbackObj){
	lua_listener_t l = calloc(1,sizeof(*l));
	l->base.sock = sock;
	l->base.type = LUA_LISTENER;
	l->base.callbackObj = callbackObj;
	return l;
}

static lua_data_socket_t new_data_socket(kn_fd_t sock){
	lua_data_socket_t l = calloc(1,sizeof(*l));
	l->base.sock = sock;
	l->base.type = LUA_DATASOCKET;
	l->fd_destroy_fn = kn_fd_get_destroyer(sock);
	kn_fd_set_destroyer(sock,datasocket_destroyer);
	kn_fd_setud(sock,(void*)l);
	return l;
}

static void data_socket_close(lua_data_socket_t l)
{
	if(l->is_close != 0) return;
	l->is_close = 1;
	if(!kn_list_size(&l->send_list)){
		//没有数据等待发送，直接关闭
		kn_closefd(l->base.sock);
	}	
}


static void update_recv_pos(lua_data_socket_t c,int32_t _bytestransfer)
{
    assert(_bytestransfer >= 0);
    uint32_t bytestransfer = (uint32_t)_bytestransfer;
    uint32_t size;
	do{
		size = c->recv_buf->cap - c->recv_pos;
        size = size > bytestransfer ? bytestransfer:size;
		c->recv_buf->size += size;
		c->recv_pos += size;
		bytestransfer -= size;
		if(c->recv_pos >= c->recv_buf->cap)
		{
			if(!c->recv_buf->next)
				c->recv_buf->next = new_bytebuffer(MAX_BUFSIZE);
			c->recv_buf = c->recv_buf->next;
			c->recv_pos = 0;
		}
	}while(bytestransfer);
}


static void do_recv_callback(lua_data_socket_t l,char *packet,int err){
	//callbackObj只是一个普通的表所以不能直接使用CALL_OBJ_FUNC
	luaObject_t callbackObj = l->base.callbackObj;
	lua_rawgeti(callbackObj->L,LUA_REGISTRYINDEX,callbackObj->rindex);
	lua_pushstring(callbackObj->L,"recvfinish");
	lua_gettable(callbackObj->L,-2);
	if(callbackObj->L != g_L) lua_xmove(callbackObj->L,g_L,1);
	lua_pushlightuserdata(g_L,l);
	if(!packet){
		lua_pushnil(g_L);   
	}else{
		lua_pushstring(g_L,packet);
		recv_count++;
	}
	lua_pushnumber(g_L,err);
	if(0 != lua_pcall(g_L,3,0,0)){
		const char * error = lua_tostring(g_L, -1);
		printf("stream_recv_finish:%s\n",error);
		lua_pop(g_L,1);
	}
	lua_pop(callbackObj->L,1);
}

static int unpack(lua_data_socket_t c)
{
	uint32_t pk_len = 0;
	uint32_t pk_total_size;
	uint32_t pos;
	do{

		if(c->unpack_size <= sizeof(uint32_t))
			return 1;
		bytebuffer_read(c->unpack_buf,c->unpack_pos,(int8_t*)&pk_len,sizeof(pk_len));
		pk_total_size = pk_len+sizeof(pk_len);
		
		if(pk_total_size > MAX_BUFSIZE)
			return -1;
		
		if(pk_total_size > c->unpack_size)
			return 1;
		pos = 0;
		//调整unpack_buf和unpack_pos
		do{
			uint32_t size = c->unpack_buf->size - c->unpack_pos;
			size = pk_total_size > size ? size:pk_total_size;
			memcpy(&c->packet[pos],&c->unpack_buf->data[c->unpack_pos],size);
			pos += size;
			c->unpack_pos  += size;
			pk_total_size  -= size;
			c->unpack_size -= size;
			if(c->unpack_pos >= c->unpack_buf->cap)
			{
				assert(c->unpack_buf->next);
				bytebuffer_t tmp = c->unpack_buf;
				c->unpack_pos = 0;
				c->unpack_buf = c->unpack_buf->next;
				free(tmp);
			}
		}while(pk_total_size);
		do_recv_callback(c,&c->packet[sizeof(int)],0);
		if(c->is_close)
			return 0;
	}while(1);

	return 1;
}

static void luasocket_post_recv(lua_data_socket_t l){
	if(kn_fd_get_type(l->base.sock) == STREAM_SOCKET){
		if(!l->recv_buf){
			l->recv_buf = new_bytebuffer(MAX_BUFSIZE);
			l->recv_pos = 0;
		}
		
		int size = MAX_BUFSIZE;
		int c = 0;
		bytebuffer_t buf = l->recv_buf;
		int pos = l->recv_pos;
		do
		{
			int free_buffer_size = buf->cap - pos;
			free_buffer_size = size > free_buffer_size ? free_buffer_size:size;
			l->wrecvbuf[c].iov_len = free_buffer_size;
			l->wrecvbuf[c].iov_base = buf->data + pos;
			size -= free_buffer_size;
			pos += free_buffer_size;
			if(size && pos >= buf->cap)
			{
				pos = 0;
				if(!buf->next)
					buf->next = new_bytebuffer(MAX_BUFSIZE);
				buf = buf->next;
			}
			++c;
		}while(size);
		l->recv_overlap.iovec_count = c;
		l->recv_overlap.iovec = l->wrecvbuf;
		if(!l->unpack_buf) l->unpack_buf = l->recv_buf;
		kn_post_recv(l->base.sock,&l->recv_overlap);	
	}
}

static void luasocket_post_send(lua_data_socket_t l){
	if(kn_fd_get_type(l->base.sock) == STREAM_SOCKET){
		int c = 0;
		sendbuf *buf = (sendbuf*)kn_list_head(&l->send_list);
		int size = 0;
		int send_size = 0;
		while(c < MAX_WSEND_SIZE && buf){
			l->wsendbuf[c].iov_base = buf->buf + buf->index;
			l->wsendbuf[c].iov_len  = buf->size;
			send_size += size;
			size += buf->size;
			++c;
			buf = (sendbuf*)buf->node.next;
		}			
		l->send_overlap.iovec_count = c;
		l->send_overlap.iovec = l->wsendbuf;
		kn_post_send(l->base.sock,&l->send_overlap);		
	}
}

static void stream_recv_finish(lua_data_socket_t l,st_io *io,int32_t bytestransfer,int32_t err)
{
	int ret;
	if(bytestransfer <= 0){
		do_recv_callback(l,NULL,err);
	}else{
		update_recv_pos(l,bytestransfer);
		l->unpack_size += bytestransfer;
		ret = unpack(l);
		if(ret > 0)
			luasocket_post_recv(l);
	}
}

static void stream_send_finish(lua_data_socket_t l,st_io *io,int32_t bytestransfer,int32_t err)
{
	//printf("stream_send_finish %d ,%x\n",bytestransfer,(int)l);
	if(bytestransfer <= 0)
		do_recv_callback(l,NULL,err);
	else{
		while(bytestransfer > 0){
			sendbuf *buf = (sendbuf*)kn_list_head(&l->send_list);
			int size = buf->size;
			if(size <= bytestransfer)
			{
				bytestransfer -= size;
				kn_list_pop(&l->send_list);
				free(buf);
			}else{
				buf->index += bytestransfer;
				buf->size -= bytestransfer;
				break;
			}
		}
		if(kn_list_size(&l->send_list)){
			luasocket_post_send(l);
		}else if(l->is_close)
			//关闭lua_socket
			kn_closefd(l->base.sock);
			//data_socket_close(l);
	}
}

static void stream_transfer_finish(kn_fd_t s,st_io *io,int32_t bytestransfer,int32_t err)
{   
    lua_data_socket_t l = (lua_data_socket_t)kn_fd_getud(s);
    //防止l在callback中被释放
    if(!io){
		do_recv_callback(l,NULL,err);              		
	}else if(io == &l->send_overlap)
		stream_send_finish(l,io,bytestransfer,err);
	else if(io == &l->recv_overlap)
		stream_recv_finish(l,io,bytestransfer,err);
}


static void on_accept(kn_fd_t s,void *ud){
	//printf("c on_accept\n");
	lua_data_socket_t c = new_data_socket(s);
	luaObject_t  callbackObj = (luaObject_t)ud;
	lua_rawgeti(callbackObj->L,LUA_REGISTRYINDEX,callbackObj->rindex);
	lua_pushstring(callbackObj->L,"onaccept");
	lua_gettable(callbackObj->L,-2);
	if(callbackObj->L != g_L) lua_xmove(callbackObj->L,g_L,1);
	lua_pushlightuserdata(g_L,c);			
	if(0 != lua_pcall(g_L,1,0,0))
	{
		const char * error = lua_tostring(g_L, -1);
		printf("on_accept:%s\n",error);
		lua_pop(g_L,1);		
	}
	lua_pop(callbackObj->L,1);		
}


static int lua_close_socket(lua_State *L){
	lua_socket_t l = lua_touserdata(L,1);
	if(l->type == LUA_LISTENER){
		kn_closefd(l->sock);
		if(l->callbackObj) release_luaObj(l->callbackObj);
		free(l);
	}else if(l->type == LUA_DATASOCKET){
		data_socket_close((lua_data_socket_t)l);
	}
	return 0;
}


static void on_connect(kn_fd_t s,struct kn_sockaddr *remote,void *ud,int err)
{	
	luaObject_t obj = (luaObject_t)ud;	
	lua_data_socket_t l = NULL;
	if(s){
		l = new_data_socket(s);
	}
	lua_rawgeti(obj->L,LUA_REGISTRYINDEX,obj->rindex);
	lua_pushstring(obj->L,"onconnected");
	lua_gettable(obj->L,-2);
	if(obj->L != g_L) lua_xmove(obj->L,g_L,1);
	if(l) lua_pushlightuserdata(g_L,l);
	else  lua_pushnil(g_L);
	
	lua_newtable(g_L);
	lua_pushstring(g_L,"type");
	lua_pushnumber(g_L,remote->addrtype);
	lua_settable(g_L, -3);
	if(remote->addrtype == AF_INET){
		char ip[32];
		inet_ntop(AF_INET,&remote->in,ip,(socklen_t )sizeof(remote->in));
		lua_pushstring(g_L,"ip");
		lua_pushstring(g_L,ip);		
		lua_settable(g_L, -3);
		lua_pushstring(g_L,"port");
		lua_pushnumber(g_L,ntohl(remote->in.sin_port));		
		lua_settable(g_L, -3);			
	}
	lua_pushnumber(g_L,err);	
	if(0 != lua_pcall(g_L,3,0,0))
	{
		const char * error = lua_tostring(g_L, -1);
		printf("on_connect:%s\n",error);
		lua_pop(g_L,1);		
	}
	lua_pop(obj->L,1);
	release_luaObj(obj);
}

int lua_connect(lua_State *L){
	int sock_type = lua_tonumber(L,1);
	luaObject_t remote = create_luaObj(L,2);
	luaObject_t local = create_luaObj(L,3);
	luaObject_t obj = create_luaObj(L,4);
	if(!remote || !obj){
		release_luaObj(local);
		release_luaObj(remote);
		release_luaObj(obj);
		lua_pushboolean(L,0);
		return 1;
	}
	kn_sockaddr addr_local;
	kn_sockaddr addr_remote;
	if(remote){
		int type = GET_OBJ_FIELD(remote,"type",int,lua_tonumber);
		if(type == AF_INET){
			kn_addr_init_in(&addr_remote,
						    GET_OBJ_FIELD(remote,"ip",const char*,lua_tostring),
						    GET_OBJ_FIELD(remote,"port",int,lua_tonumber));	
		}
	}
	if(local){
		int type = GET_OBJ_FIELD(local,"type",int,lua_tonumber);
		if(type == AF_INET){
			kn_addr_init_in(&addr_local,
						    GET_OBJ_FIELD(local,"ip",const char*,lua_tostring),
						    GET_OBJ_FIELD(local,"port",int,lua_tonumber));	
		}	
	}
			
	if(0 != kn_asyn_connect(g_proactor,sock_type,local?&addr_local:NULL,
					&addr_remote,on_connect,(void*)obj))
	{
		release_luaObj(obj);
		lua_pushboolean(L,0);
	}else
		lua_pushboolean(L,1);
		
	release_luaObj(local);
	release_luaObj(remote);	
	return 1;
}


int lua_listen(lua_State *L){
	luaObject_t addr = create_luaObj(L,1);
	kn_sockaddr addr_local;
	luaObject_t callbackObj;
	int type = GET_OBJ_FIELD(addr,"type",int,lua_tonumber);
	if(type == AF_INET){
		kn_addr_init_in(&addr_local,
						GET_OBJ_FIELD(addr,"ip",const char*,lua_tostring),
						GET_OBJ_FIELD(addr,"port",int,lua_tonumber));
		callbackObj =  create_luaObj(L,2);			 
		lua_listener_t l = new_listener(kn_listen(g_proactor,&addr_local,on_accept,(void*)callbackObj),
									   callbackObj);
		lua_pushlightuserdata(L,l);
		release_luaObj(addr);
		return 1;			
	}
	release_luaObj(addr);
	lua_pushnil(L);
	return 1;
}

int lua_send(lua_State *L){
	lua_socket_t s  = lua_touserdata(L,1);
	if(s->type != LUA_DATASOCKET) {
		lua_pushboolean(L,0);
		lua_pushstring(L,"invaild socket");	
		return 2;
	}
	lua_data_socket_t l = (lua_data_socket_t)s;
	if(l->is_close){
		lua_pushboolean(L,0);
		lua_pushstring(L,"socket is close");		
	}
	sendbuf *buf;
	const char* str = NULL;	
	if(lua_isstring(L,2)){
		str = lua_tostring(L,2);
		int size = strlen(str)+1;
		if(size > MAX_BUFSIZE - sizeof(int)){
			//记录日志，包长度过长
			lua_pushboolean(L,0);
			lua_pushstring(L,"packet to lagre");	
			return 2;
		}
		buf = calloc(1,sizeof(*buf)+size+sizeof(int));
		*(int*)&buf->buf[0] = size;
		strcpy(buf->buf+sizeof(int),str);
		buf->index = 0;
		buf->size = size+sizeof(int);
	}else
	{
		lua_pushboolean(L,0);
		lua_pushstring(L,"can only send string");	
		return 2;
	}
	luaObject_t  addr_remote = NULL;
	if(!lua_isnil(L,3))
		addr_remote = create_luaObj(L,3);
	
	release_luaObj(addr_remote);
	//if(addr_remote)
	//	buf->remote = *addr_remote;
	
	int size = kn_list_size(&l->send_list);
	kn_list_pushback(&l->send_list,(kn_list_node*)buf);
	if(!size)
		luasocket_post_send(l);
	lua_pushboolean(L,1);	
	return 1;
}

int lua_run(lua_State *L){
	if(!recv_sigint){
		if(lua_isnumber(L,-1)){
			uint32_t ms = lua_tonumber(L,-1);
			kn_proactor_run(g_proactor,ms);
		}else{
		 	uint64_t tick,now;
		    tick = now = kn_systemms64();	
			while(!recv_sigint){
				kn_proactor_run(g_proactor,50);
		        now = kn_systemms64();
				if(now - tick > 1000)
				{
		            //uint32_t elapse = (uint32_t)(now-tick);
		            printf("count:%d/s\n",recv_count);
					tick = now;
					recv_count = 0;
				}			
			}
		}
	}
	lua_pushboolean(L,recv_sigint?0:1);
	return 1;	
}

int lua_bind(lua_State *L){
	lua_socket_t s = lua_touserdata(L,1);
	if(s->type != LUA_DATASOCKET){
		lua_pushboolean(L,0);
		return 1;
	}
	//lua_pushboolean(L,1);
	if(0 == kn_proactor_bind(g_proactor,s->sock,stream_transfer_finish)){
		s->callbackObj = create_luaObj(L,2);
		luasocket_post_recv((lua_data_socket_t)s);	
		lua_pushboolean(L,1);
	}
	else
		lua_pushboolean(L,0);
	return 1;
}

int lua_getsystick(lua_State *L){
	lua_pushnumber(L,kn_systemms());
	return 1;
}

int lua_setname(lua_State *L){
	lua_socket_t l = lua_touserdata(L,1);
	const char *name = lua_tostring(L,2);
	strncpy(l->name,name,512);
	return 0;
}

int lua_getname(lua_State *L){
	lua_socket_t l = lua_touserdata(L,1);
	lua_pushstring(L,l->name);
	return 1;
}

struct start_arg{
	char start_file[256];
	kn_channel_t channel;
};

void RegisterNet();
static void start(const char *start_file)
{
	printf("start\n");
	char buf[4096];
	snprintf(buf,4096,"\
		local Sche = require \"lua/scheduler\"\
		local Thread = require \"lua/thread\"\
		function channel_msg_callback(from,msg)\
		  Thread.On_channel_msg(from,msg)\
		end\
		local main,err= loadfile(\"%s\")\
		if err then print(err) end\
		Sche.Spawn(function ()\
					Thread.Setup()\
					main()\
				  end)\
		local ms = 5\
		while C.run(ms) do\
			if Sche.Schedule() > 0 then\
				ms = 0\
			else\
				ms = 5\
			end\
		end",start_file);
	    luaL_loadstring(g_L,buf);
	    if(0 != lua_pcall(g_L,0,0,0)){
			const char * error = lua_tostring(g_L, -1);
			lua_pop(g_L,1);
			printf("%s\n",error);
		}
}


void* thread_func(void *arg){
	struct start_arg *start_arg = (struct start_arg*)arg;
	g_L = luaL_newstate();
	luaL_openlibs(g_L);
	channel = start_arg->channel;	
	RegisterNet(g_L);		
	start(start_arg->start_file);
	free(arg);
    kn_channel_close(channel);
	lua_close(g_L);
	return NULL;
}

//启动一个线程运行独立的lua虚拟机
int lua_fork(lua_State *L){
	const char *start_file = lua_tostring(L,1);
	kn_thread_t t = kn_create_thread(THREAD_JOINABLE);
	struct start_arg *arg = calloc(1,sizeof(*arg));
	kn_channel_t c = kn_new_channel(kn_thread_getid(t));
	arg->channel = c;
	strncpy(arg->start_file,start_file,256);
	kn_thread_start_run(t,thread_func,arg);
	lua_pushlightuserdata(L,t);
	PUSH_TABLE4(L,lua_pushinteger(L,c._data[0]),
				  lua_pushinteger(L,c._data[1]),
				  lua_pushinteger(L,c._data[2]),
				  lua_pushinteger(L,c._data[3]));
	return 2;
}

static void channel_callback(kn_channel_t c,
							 kn_channel_t sender,
							 void*msg,void *ud){
	lua_getglobal(g_L,"channel_msg_callback");
	if(sender.ptr){
		PUSH_TABLE4(g_L,lua_pushinteger(g_L,sender._data[0]),
				    lua_pushinteger(g_L,sender._data[1]),
				    lua_pushinteger(g_L,sender._data[2]),
				    lua_pushinteger(g_L,sender._data[3]));	
	}
	else lua_pushnil(g_L);
	lua_pushstring(g_L,(const char *)msg);
	if(0 != lua_pcall(g_L,2,0,0)){
		const char * error = lua_tostring(g_L, -1);
		printf("channel_callback:%s\n",error);
		lua_pop(g_L,1);
	}
	//lua_pop(callbackObj->L,1);
}

int lua_setup_channel(lua_State *L){
	kn_channel_bind(g_proactor,channel,channel_callback,NULL);
	return 0;
}

int lua_thread_join(lua_State *L){
	kn_thread_t t = (kn_thread_t)lua_touserdata(L,1);
	kn_thread_join(t);
	kn_thread_destroy(t);
	return 0;
}

int lua_channel_send(lua_State *L){
	kn_channel_t to;
	GET_ARRAY(L,1,to._data,lua_tointeger);	
	const char *tmp = lua_tostring(L,2);
	size_t len = strlen(tmp);
	char *msg = calloc(1,len+1);
	strcpy(msg,tmp); 
	if(0 != kn_channel_putmsg(to,&channel,msg,NULL)){
		free(msg);
		lua_pushstring(L,"invaild channel");
	}else
		lua_pushnil(L);
	return 1;
}

void kn_fd_addref(kn_fd_t);
void kn_fd_subref(kn_fd_t);

int lua_socket_addref(lua_State *L){
	lua_socket_t s = lua_touserdata(L,1);
	if(s->type != LUA_DATASOCKET){
		return 0;
	}
	kn_fd_addref(s->sock);
	//kn_ref_acquire((kn_ref*)s->sock);
	return 0;	
}

int lua_socket_subref(lua_State *L){
	lua_socket_t s = lua_touserdata(L,1);
	if(s->type != LUA_DATASOCKET){
		return 0;
	}
	kn_fd_subref(s->sock);
	//kn_ref_release((kn_ref*)s->sock);
	return 0;	
}

void RegisterNet(){  
    
    lua_getglobal(g_L,"_G");
	if(!lua_istable(g_L, -1))
	{
		lua_pop(g_L,1);
		lua_newtable(g_L);
		lua_pushvalue(g_L,-1);
		lua_setglobal(g_L,"_G");
	}

	lua_pushstring(g_L, "AF_INET");
	lua_pushinteger(g_L, AF_INET);
	lua_settable(g_L, -3);
	
	lua_pushstring(g_L, "AF_INET6");
	lua_pushinteger(g_L, AF_INET6);
	lua_settable(g_L, -3);
	
	lua_pushstring(g_L, "AF_LOCAL");
	lua_pushinteger(g_L, AF_LOCAL);
	lua_settable(g_L, -3);
	
	lua_pushstring(g_L, "IPPROTO_TCP");
	lua_pushinteger(g_L, IPPROTO_TCP);
	lua_settable(g_L, -3);
	
	lua_pushstring(g_L, "IPPROTO_UDP");
	lua_pushinteger(g_L, IPPROTO_UDP);
	lua_settable(g_L, -3);			
	
	lua_pushstring(g_L, "SOCK_STREAM");
	lua_pushinteger(g_L, SOCK_STREAM);
	lua_settable(g_L, -3);
	
	lua_pushstring(g_L, "SOCK_DGRAM");
	lua_pushinteger(g_L, SOCK_DGRAM);
	lua_settable(g_L, -3);	
	
	lua_pop(g_L,1);
    
	lua_newtable(g_L);
	
	lua_pushstring(g_L,"stream_listen");
	lua_pushcfunction(g_L,&lua_listen);
	lua_settable(g_L, -3);

	lua_pushstring(g_L,"connect");
	lua_pushcfunction(g_L,&lua_connect);
	lua_settable(g_L, -3);

	lua_pushstring(g_L,"send");
	lua_pushcfunction(g_L,&lua_send);
	lua_settable(g_L, -3);

	lua_pushstring(g_L,"close");
	lua_pushcfunction(g_L,&lua_close_socket);
	lua_settable(g_L, -3);

	lua_pushstring(g_L,"bind");
	lua_pushcfunction(g_L,&lua_bind);
	lua_settable(g_L, -3);

	lua_pushstring(g_L,"run");
	lua_pushcfunction(g_L,&lua_run);
	lua_settable(g_L, -3);
	
	lua_pushstring(g_L,"GetSysTick");
	lua_pushcfunction(g_L,&lua_getsystick);
	lua_settable(g_L, -3);
	
	lua_pushstring(g_L,"set_name");
	lua_pushcfunction(g_L,&lua_setname);
	lua_settable(g_L, -3);
	
	lua_pushstring(g_L,"get_name");
	lua_pushcfunction(g_L,&lua_getname);
	lua_settable(g_L, -3);		
		
	lua_pushstring(g_L,"fork");
	lua_pushcfunction(g_L,&lua_fork);
	lua_settable(g_L, -3);

	lua_pushstring(g_L,"thread_join");
	lua_pushcfunction(g_L,&lua_thread_join);
	lua_settable(g_L, -3);	
		
	lua_pushstring(g_L,"setup_channel");
	lua_pushcfunction(g_L,&lua_setup_channel);
	lua_settable(g_L, -3);
	
	lua_pushstring(g_L,"channel_send");
	lua_pushcfunction(g_L,&lua_channel_send);
	lua_settable(g_L, -3);
	
	lua_pushstring(g_L,"socket_addref");
	lua_pushcfunction(g_L,&lua_socket_addref);
	lua_settable(g_L, -3);
	
	lua_pushstring(g_L,"socket_subref");
	lua_pushcfunction(g_L,&lua_socket_subref);
	lua_settable(g_L, -3);		
			
	lua_setglobal(g_L,"C");
    g_proactor = kn_new_proactor();
    printf("load c function finish\n");  
}

int main(int argc,char **argv)
{
	if(argc < 2){
		printf("usage luanet luafile\n");
		return 0;
	}
	g_L = luaL_newstate();
	luaL_openlibs(g_L);
    signal(SIGINT,sig_int);
    signal(SIGPIPE,SIG_IGN);	
	RegisterNet();
	channel = kn_new_channel(pthread_self());
	start(argv[1]);
    //kn_channel_close(channel);
	//lua_close(g_L);		
	return 0;
} 
*/
