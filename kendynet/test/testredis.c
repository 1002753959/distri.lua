#include <stdio.h>
#include <stdlib.h>
#include "core/msgdisp.h"
#include <stdint.h>
#include "testcommon.h"
#include "core/db/asyndb.h"


asyndb_t asydb;


int g = 0;
int count = 0;


void db_setcallback(struct db_result *result);

void db_getcallback(struct db_result *result)
{
	//printf("%s\n",result->result_str);
	count++;
	char req[256];
    snprintf(req,256,"set key%d %d",g,g);
    if(0 != asydb->request(asydb,new_dbrequest(db_set,req,db_setcallback,result->ud,make_by_msgdisp((msgdisp_t)result->ud))))
    	printf("request error\n");
}

void db_setcallback(struct db_result *result)
{
	if(result->ud == NULL) printf("error\n");
	char req[256];
    snprintf(req,256,"get key%d",g);
    g = (g+1)%102400;
    asydb->request(asydb,new_dbrequest(db_get,req,db_getcallback,result->ud,make_by_msgdisp((msgdisp_t)result->ud)));
}


int32_t asynprocesspacket(msgdisp_t disp,msgsender sender,rpacket_t rpk)
{
	uint16_t cmd = rpk_read_uint16(rpk);
	if(cmd != ASYNDB_RESILT)
	{
		printf("error packet\n");
	}else
	{
		struct db_result *result = (struct db_result*)rpk_read_pointer(rpk);
		result->callback(result);
		free_dbresult(result);	
	}
    return 1;
}



static void *service_main(void *ud){
    msgdisp_t disp = (msgdisp_t)ud;
    while(!stop){
        msg_loop(disp,50);
    }
    return NULL;
}




int main(int argc,char **argv)
{
    setup_signal_handler();
    msgdisp_t disp1 = new_msgdisp(NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  asynprocesspacket,
                                  NULL);

    thread_t service1 = create_thread(THREAD_JOINABLE);

    msgdisp_t disp2 = new_msgdisp(NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  asynprocesspacket,
                                  NULL);

    thread_t service2 = create_thread(THREAD_JOINABLE);    
    asydb = new_asyndb();
    asydb->connectdb(asydb,"127.0.0.1",6379);
    asydb->connectdb(asydb,"127.0.0.1",6379);
    //发出第一个请求uu
    char req[256];
    snprintf(req,256,"set key%d %d",g,g);
    
    asydb->request(asydb,new_dbrequest(db_set,req,db_setcallback,disp1,make_by_msgdisp(disp1)));
    thread_start_run(service1,service_main,(void*)disp1);

    asydb->request(asydb,new_dbrequest(db_set,req,db_setcallback,disp2,make_by_msgdisp(disp2)));
    thread_start_run(service2,service_main,(void*)disp2);    
    
    uint32_t tick,now;
    tick = now = GetSystemMs();
    while(!stop){
        sleepms(100);
        now = GetSystemMs();
        if(now - tick > 1000)
        {
            printf("count:%d\n",count);
            tick = now;
            count = 0;
        }
    }
    thread_join(service1);
    thread_join(service2);
    return 0;
}