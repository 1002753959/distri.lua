#include "kendynet.h"
#include "stream_conn.h"
#include "kn_timer.h"

int  client_count = 0; 

void  on_packet(stream_conn_t c,packet_t p){
	
	rpacket_t rpk = (rpacket_t)p;
	uint64_t id = rpk_read_uint64(rpk);
	if(id == (uint64_t)c) 
		stream_conn_send(c,make_writepacket(p));	
}

void on_disconnected(stream_conn_t c,int err){
	printf("on_disconnectd\n");
}

void on_connect(handle_t s,int err,void *ud,kn_sockaddr *_)
{
	((void)_);
	if(err == 0){
		printf("connect ok\n");
		engine_t p = (engine_t)ud;
		stream_conn_t conn = new_stream_conn(s,4096,new_rpk_decoder(4096));
		stream_conn_associate(p,conn,on_packet,on_disconnected);		
		wpacket_t wpk = wpk_create(64);
		wpk_write_uint64(wpk,(uint64_t)conn);		
		stream_conn_send(conn,(packet_t)wpk);		
	}else{
		kn_close_sock(s);
		printf("connect failed\n");
	}	
}


int timer_callback(kn_timer_t timer){
	printf("client_count:%d\n",client_count);
	return 1;
}

int main(int argc,char **argv){
	signal(SIGPIPE,SIG_IGN);
	engine_t p = kn_new_engine();
	kn_sockaddr remote;
	kn_addr_init_in(&remote,argv[1],atoi(argv[2]));
	int client_count = atoi(argv[3]);
	int i = 0;
	for(; i < client_count; ++i){
		handle_t c = kn_new_sock(AF_INET);
		int ret = kn_sock_connect(p,c,&remote,NULL);
		if(ret > 0){
			on_connect(c,0,p,&remote);
		}else if(ret == 0){
			kn_sock_set_connect_cb(c,on_connect,p);
		}else{
			kn_close_sock(c);
			printf("connect failed\n");
		}
	}
	
	kn_reg_timer(p,1000,timer_callback,NULL);		
	kn_engine_run(p);
	return 0;
}
