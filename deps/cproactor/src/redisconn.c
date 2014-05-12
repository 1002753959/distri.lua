#include "kn_proactor.h"
#include "redisconn.h"

static void redisLibevRead(redisconn_t rc){
    redisAsyncHandleRead(rc->context);
}

static void redisLibevWrite(redisconn_t rc){
    redisAsyncHandleWrite(rc->context);
}

void kn_redisDisconnect(redisconn_t rc);

static void redis_on_active(kn_fd_t s,int event){
	//kn_proactor_t p;
	redisconn_t rc = (redisconn_t)s;
	if(rc->state == REDIS_CONNECTING){
		int err = 0;
		socklen_t len = sizeof(err);
		if (getsockopt(s->fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1) {
			rc->cb_connect(rc,-1);
			kn_closefd(s);
			return;
		}
		if(err){
			errno = err;
			rc->cb_connect(rc,errno);
			kn_closefd(s);
			return;
		}
		//connect success
		//p = rc->base.proactor;
		//p->UnRegister(p,(kn_fd_t)rc);   
		rc->state = REDIS_ESTABLISH;
		//p->Register(p,(kn_fd_t)rc);
		rc->cb_connect(rc,0);			
	}else{
		if(event & (EPOLLERR | EPOLLHUP)){
			kn_redisDisconnect(rc);	
			return;
		}
		if(event & (EPOLLRDHUP | EPOLLIN)){
			redisLibevRead(rc);
		}
		if(event & EPOLLOUT){
			redisLibevWrite(rc);
		}	
	}
}

static void redisconn_destroy(void *ptr){
	//redisconn_t rc = (redisconn_t)((char*)ptr - sizeof(kn_dlist_node));
	//redisAsyncFree(rc->context);
	free(ptr);
}

static void redisAddRead(void *privdata){
	redisconn_t con = (redisconn_t)privdata;
	kn_proactor_t p = con->base.proactor;
	p->addRead(p,(kn_fd_t)con);
}

static void redisDelRead(void *privdata) {
	redisconn_t con = (redisconn_t)privdata;
	kn_proactor_t p = con->base.proactor;
	p->delRead(p,(kn_fd_t)con);
}

static void redisAddWrite(void *privdata) {
	redisconn_t con = (redisconn_t)privdata;
	kn_proactor_t p = con->base.proactor;
	p->addWrite(p,(kn_fd_t)con);
}

static void redisDelWrite(void *privdata) {
	redisconn_t con = (redisconn_t)privdata;
	kn_proactor_t p = con->base.proactor;
	p->delWrite(p,(kn_fd_t)con);
}

static void redisCleanup(void *privdata) {
    redisconn_t con = (redisconn_t)privdata;
	kn_proactor_t p = con->base.proactor;
    p->UnRegister(p,(kn_fd_t)con);
    kn_closefd((kn_fd_t)con); 
}


int kn_redisAsynConnect(struct kn_proactor *p,
						const char *ip,unsigned short port,
						void (*cb_connect)(struct redisconn*,int err))
{
	redisAsyncContext *c = redisAsyncConnect(ip, port);
    if(c->err) {
        printf("Error: %s\n", c->errstr);
        return -1;
    }
    redisconn_t con = calloc(1,sizeof(*con));
    con->context = c;
	con->base.fd =  ((redisContext*)c)->fd;
	con->base.on_active = redis_on_active;
	con->base.type = REDISCONN;
	con->state = REDIS_CONNECTING; 
	con->cb_connect = cb_connect;   
    kn_ref_init(&con->base.ref,redisconn_destroy);
	
    c->ev.addRead =  redisAddRead;
    c->ev.delRead =  redisDelRead;
    c->ev.addWrite = redisAddWrite;
    c->ev.delWrite = redisDelWrite;
    c->ev.cleanup =  redisCleanup;
    c->ev.data = con;		
	if(p->Register(p,(kn_fd_t)con) == 0){
		return 0;
	}else{
		kn_closefd((kn_fd_t)c);
		return -1;
	}    											
}

typedef void (*redis_cb)(redisconn_t,redisReply*,void *pridata);

struct privst{
	redisconn_t rc;
	void*       privdata;
	void (*cb)(redisconn_t,redisReply*,void *pridata);
};

static void kn_redisCallback(redisAsyncContext *c, void *r, void *privdata) {
	redisReply *reply = r;
	redisconn_t rc = ((struct privst*)privdata)->rc;
	redis_cb cb = ((struct privst*)privdata)->cb;
	free(privdata);
	if(cb){
		cb(rc,reply,((struct privst*)privdata)->privdata);
	}
}

int kn_redisCommand(redisconn_t rc,const char *cmd,
					void (*cb)(redisconn_t,redisReply*,void *pridata),void *pridata)
{
	struct privst *privst = NULL;
	if(cb){
		privst = calloc(1,sizeof(*privst));
		privst->rc = rc;
		privst->cb = cb;
		privst->privdata = pridata;
	}
	int status = redisAsyncCommand(rc->context, privst?kn_redisCallback:NULL,privst,cmd);
	if(status != REDIS_OK)
		free(privst);
	return status;
}
					

void kn_redisDisconnect(redisconn_t rc){
	redisAsyncDisconnect(rc->context);
}

