#include "kn_type.h"
#include "kn_list.h"
#include "kendynet_private.h"
#include <assert.h>
#include <arpa/inet.h>
#include "kn_event.h"
#include "kn_stream_socket.h"

handle_t kn_new_sock(int domain,int type,int protocal){	
	int fd = socket(domain,type|SOCK_CLOEXEC,protocal);
	if(fd < 0) return NULL;
	handle_t h = NULL;
	if(type == SOCK_STREAM) 
		h = new_stream_socket(fd,domain);
	else if(type == SOCK_DGRAM){

	}
	if(!h) close(fd);
	return h;
}

static  inline int stream_socket_send(handle_t h,st_io *req){
	kn_socket *s = (kn_socket*)h;
	if(!s->e || h->status != SOCKET_ESTABLISH){
		return -1;
	 }	
	if(0 != kn_list_size(&s->pending_send))
		return kn_sock_post_send(h,req);
	errno = 0;			
	int bytes_transfer = 0;

	if(((kn_stream_socket*)s)->ssl){
		assert(req->iovec_count == 1);
		if(req->iovec_count != 1)
			return -1;			
		bytes_transfer = TEMP_FAILURE_RETRY(SSL_write(((kn_stream_socket*)s)->ssl,req->iovec[0].iov_base,req->iovec[0].iov_len));
		int ssl_error = SSL_get_error(((kn_stream_socket*)s)->ssl,bytes_transfer);
		if(bytes_transfer < 0 && (ssl_error == SSL_ERROR_WANT_WRITE ||
					ssl_error == SSL_ERROR_WANT_READ ||
					ssl_error == SSL_ERROR_WANT_X509_LOOKUP)){
			errno = EAGAIN;
		}		
	}
	else	
		bytes_transfer = TEMP_FAILURE_RETRY(writev(h->fd,req->iovec,req->iovec_count));				

		
	if(bytes_transfer < 0 && errno == EAGAIN)
		return kn_sock_post_send(h,req);				
	return bytes_transfer > 0 ? bytes_transfer:-1;
}

int kn_sock_send(handle_t h,st_io *req){
	if(h->type != KN_SOCKET){ 
		return -1;
	}	
	if(((kn_socket*)h)->type == SOCK_STREAM)
	 	return stream_socket_send(h,req);
	else
	 	return -1;		
}

static inline int stream_socket_recv(handle_t h,st_io *req){
	kn_socket *s = (kn_socket*)h;
	if(!s->e || h->status != SOCKET_ESTABLISH){
		return -1;
	 }	
	errno = 0;	
	if(0 != kn_list_size(&s->pending_send))
		return kn_sock_post_recv(h,req);
		
	int bytes_transfer = 0;

	if(((kn_stream_socket*)s)->ssl){
		assert(req->iovec_count == 1);
		if(req->iovec_count != 1)
			return -1;
		bytes_transfer = TEMP_FAILURE_RETRY(SSL_read(((kn_stream_socket*)s)->ssl,req->iovec[0].iov_base,req->iovec[0].iov_len));
		int ssl_error = SSL_get_error(((kn_stream_socket*)s)->ssl,bytes_transfer);
		if(bytes_transfer < 0 && (ssl_error == SSL_ERROR_WANT_WRITE ||
					ssl_error == SSL_ERROR_WANT_READ ||
					ssl_error == SSL_ERROR_WANT_X509_LOOKUP)){
			errno = EAGAIN;
		}		
	}
	else	
		bytes_transfer = TEMP_FAILURE_RETRY(readv(h->fd,req->iovec,req->iovec_count));				

		
	if(bytes_transfer < 0 && errno == EAGAIN)
		return kn_sock_post_recv(h,req);				
	return bytes_transfer > 0 ? bytes_transfer:-1;		
} 

int kn_sock_recv(handle_t h,st_io *req){
	if(h->type != KN_SOCKET){ 
		return -1;
	}	
	if(((kn_socket*)h)->type == SOCK_STREAM)
		return stream_socket_recv(h,req);
	else
		return -1;
}

static inline int stream_socket_post_send(handle_t h,st_io *req){
	kn_socket *s = (kn_socket*)h;
	if(!s->e || h->status != SOCKET_ESTABLISH){
		return -1;
	 }
	if(((kn_stream_socket*)s)->ssl){
		assert(req->iovec_count == 1);
		if(req->iovec_count != 1)
			return -1;
	}			 
	if(!is_set_write(h)){
	 	if(0 != kn_enable_write(s->e,h))
	 		return -1;
	}
	kn_list_pushback(&s->pending_send,(kn_list_node*)req);	 	
	return 0;	
}

int kn_sock_post_send(handle_t h,st_io *req){
	if(h->type != KN_SOCKET){ 
		return -1;
	}	
	if(((kn_socket*)h)->type == SOCK_STREAM)
		return stream_socket_post_send(h,req);
	else
		return -1;
}

static inline int stream_socket_post_recv(handle_t h,st_io *req){
	kn_socket *s = (kn_socket*)h;
	if(!s->e || h->status != SOCKET_ESTABLISH){
		return -1;
	}
	if(((kn_stream_socket*)s)->ssl){
		assert(req->iovec_count == 1);
		if(req->iovec_count != 1)
			return -1;
	}	
	 if(!is_set_read(h)){
	 	if(0 != kn_enable_read(s->e,h))
	 		return -1;
	 }	
	kn_list_pushback(&s->pending_recv,(kn_list_node*)req);		
	return 0;		
}

int kn_sock_post_recv(handle_t h,st_io *req){
	if(h->type != KN_SOCKET){ 
		return -1;
	}	
	if(((kn_socket*)h)->type == SOCK_STREAM)
		return stream_socket_post_recv(h,req);
	else
		return -1;		
}


int kn_sock_listen(engine_t e,
		   handle_t h,
		   kn_sockaddr *local,
		   void (*cb_accept)(handle_t,void*),
		   void *ud)
{
	if(!(((handle_t)h)->type & KN_SOCKET)) return -1;	
	kn_socket *s = (kn_socket*)h;
	if(s->comm_head.status != SOCKET_NONE) return -1;
	if(s->e) return -1;
	if(s->type == SOCK_STREAM){
		return stream_socket_listen(e,(kn_stream_socket*)s,local,cb_accept,ud);	
	}else if(s->type == SOCK_DGRAM){

	}	
	return -1;		
}


void   kn_sock_set_connect_cb(handle_t h,void (*cb_connect)(handle_t,int,void*,kn_sockaddr*),void*ud){
	if(((handle_t)h)->type == KN_SOCKET && ((kn_socket*)h)->type == SOCK_STREAM){
		((kn_stream_socket*)h)->cb_connect = cb_connect;
		h->ud = ud;
	}
}

int kn_sock_connect(engine_t e,
		        handle_t h,
		        kn_sockaddr *remote,
		        kn_sockaddr *local)
{
	if(((handle_t)h)->type != KN_SOCKET) return -1;
	kn_socket *s = (kn_socket*)h;
	if(s->comm_head.status != SOCKET_NONE) return -1;
	if(s->e) return -1;
	if(s->type == SOCK_STREAM){
		return stream_socket_connect(e,(kn_stream_socket*)s,local,remote);	
	}else if(s->type == SOCK_DGRAM){

	}
	return -1;	
}

void kn_sock_setud(handle_t h,void *ud){
	if(((handle_t)h)->type != KN_SOCKET) return;
	((handle_t)h)->ud = ud;	
}

void* kn_sock_getud(handle_t h){
	if(((handle_t)h)->type != KN_SOCKET) return NULL;	
	return ((handle_t)h)->ud;
}

int kn_sock_fd(handle_t h){
	return ((handle_t)h)->fd;
}

kn_sockaddr* kn_sock_addrlocal(handle_t h){
	if(((handle_t)h)->type != KN_SOCKET) return NULL;		
	kn_socket *s = (kn_socket*)h;
	return &s->addr_local;
}

kn_sockaddr* kn_sock_addrpeer(handle_t h){
	if(((handle_t)h)->type != KN_SOCKET) return NULL;		
	kn_socket *s = (kn_socket*)h;
	return &s->addr_remote;	
}

engine_t kn_sock_engine(handle_t h){
	if(((handle_t)h)->type != KN_SOCKET) return NULL;
	kn_socket *s = (kn_socket*)h;
	return s->e;	
}

int      kn_close_sock(handle_t h){
	if(h->type != KN_SOCKET) 
		return -1;
	if(((kn_socket*)h)->type == SOCK_STREAM)
		return stream_socket_close(h);
	else
		return -1;
}
