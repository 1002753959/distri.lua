#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include "kendynet.h"
#include "kn_thread.h"
#include "kn_time.h"

kn_channel_t channel1;
kn_channel_t channel2;

int count = 0;
void *routine1(void *arg)
{
	printf("routine1\n");
	kn_proactor_t p = kn_new_proactor();
	kn_channel_bind(p,channel1);
	int i = 0;
	for(; i < 100; ++i){
		kn_channel_putmsg(channel2,NULL,malloc(1));
	}
 	uint64_t tick,now;
    tick = now = kn_systemms64();	
	while(1){
		kn_proactor_run(p,50);
        now = kn_systemms64();
		if(now - tick > 1000)
		{
            uint32_t elapse = (uint32_t)(now-tick);
			printf("count:%d\n",count);
			tick = now;
			count = 0;
		}		
	}	
    return NULL;
}

void *routine2(void *arg)
{
	printf("routine2\n");
	kn_proactor_t p = kn_new_proactor();
	kn_channel_bind(p,channel2);
	while(1){
		kn_proactor_run(p,50);	
	}		
    return NULL;
}

void  on_msg1(struct kn_channel *c, struct kn_channel *sender,void *msg){
	++count;
	kn_channel_putmsg(channel2,NULL,malloc(1));
}

void  on_msg2(struct kn_channel *c, struct kn_channel *sender,void *msg){
	//printf("recv msg\n");
	kn_channel_putmsg(channel1,NULL,malloc(1));
}

int main(){	
	kn_thread_t t1 = kn_create_thread(THREAD_JOINABLE);
	kn_thread_t t2 = kn_create_thread(THREAD_JOINABLE);
	//kn_thread_t t3 = kn_create_thread(THREAD_JOINABLE);
	channel1 = kn_new_channel(kn_thread_getid(t1),on_msg1);
	channel2 = kn_new_channel(kn_thread_getid(t2),on_msg2);
	kn_thread_start_run(t1,routine1,NULL);
	kn_thread_start_run(t2,routine2,NULL);
	//kn_thread_start_run(t3,routine2,NULL);
	while(1){
		kn_sleepms(100);
	}
	return 0;
}


