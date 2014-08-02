#include "kendynet.h"
#include "kn_time.h"
#include "kn_thread_mailbox.h"
#include "kn_thread.h"

volatile int count = 0;

volatile pthread_t pid = 0;

static void on_mail1(kn_thread_mailbox_t *from,void *mail){
		++count;
		kn_send_mail(*from,mail,NULL);
}

static void on_mail2(kn_thread_mailbox_t *from,void *mail){
		kn_send_mail(*from,mail,NULL);
}


void *routine1(void *arg)
{
	printf("routine1\n");
	engine_t p = kn_new_engine();
	kn_setup_mailbox(p,MODE_FAST,on_mail1);
	while(!pid)kn_sleepms(1);
	kn_thread_mailbox_t mailbox = kn_query_mailbox(pid);
	if(is_empty_ident((ident)mailbox)){
		printf("error\n");
		exit(0);
	}
	int i = 0;
	for(; i < 10000; ++i){
		kn_send_mail(mailbox,(void*)1,NULL);
		//kn_channel_putmsg(channel2,NULL,malloc(1),NULL);
	}
	kn_engine_run(p);
    return NULL;
}

void *routine2(void *arg)
{
	printf("routine2\n");
	engine_t p = kn_new_engine();
	kn_setup_mailbox(p,MODE_FAST,on_mail2);
	pid = pthread_self();
	kn_engine_run(p);
    return NULL;
}




int main(){

	kn_thread_t t1 = kn_create_thread(THREAD_JOINABLE);
	kn_thread_t t2 = kn_create_thread(THREAD_JOINABLE);	
	kn_thread_start_run(t1,routine1,NULL);
	kn_thread_start_run(t2,routine2,NULL);		
 	uint64_t tick,now;
    tick = now = kn_systemms64();	
	while(1){
		kn_sleepms(100);
        now = kn_systemms64();
		if(now - tick > 1000)
		{
            uint32_t elapse = (uint32_t)(now-tick);
			printf("count:%d\n",count/elapse*1000);
			tick = now;
			count = 0;
		}		
	}	
	return 0;
}
