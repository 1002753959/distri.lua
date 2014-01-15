/*
 *  一个网关示例
*/

#include <stdio.h>
#include <stdlib.h>
#include "core/msgdisp.h"
#include "testcommon.h"


static msgdisp_t  disp_to_server;
static msgdisp_t  disp_to_client;
sock_ident        to_server;


void to_server_connected(msgdisp_t disp,sock_ident sock,const char *ip,int32_t port)
{
    to_server = sock;
}


int32_t to_client_process(msgdisp_t disp,sock_ident sock,rpacket_t rpk)
{
    if(!eq_sockident(sock,to_server)){
        //from cliet,send to server
        push_msg(disp_to_server,(msg_t)rpk);
    }else
    {
        //from server,send to client
        sock_ident client = rpk_read_sock(rpk);
        asyn_send(client,wpk_create_by_rpacket(rpk,0));
    }
    return 1;
}

void to_client_connect(msgdisp_t disp,sock_ident sock,const char *ip,int32_t port)
{
    //用第3个poller处理到客户端的连接
    disp->bind(disp,3,sock,1,3*1000,0);//由系统选择poller
}


int32_t to_server_process(msgdisp_t disp,sock_ident sock,rpacket_t rpk)
{
    if(!eq_sockident(sock,to_server)){
        //from cliet,send to server
        asyn_send(to_server,wpk_create_by_rpacket(rpk,0));
    }else{
        //from server,send to client
        push_msg(disp_to_client,(msg_t)rpk);
    }
    return 1;
}

void to_server_connect(msgdisp_t disp,sock_ident sock,const char *ip,int32_t port)
{
    //用第二个poller处理到服务器的连接
    disp->bind(disp,2,sock,1,3*1000,0);//由系统选择poller
}



//对客户端的监听
static const char *to_client_ip;
static int32_t to_client_port;


//对内部服务器的监听
static const char *to_server_ip;
static int32_t to_server_port;

static void *service_toclient(void *ud){
    msgdisp_t disp = (msgdisp_t)ud;
    int32_t err = 0;
    //用户的连接比较频繁,用一个单独的poller来处理监听
    disp->listen(disp,1,to_client_ip,to_client_port,&err);
    while(!stop){
        msg_loop(disp,50);
    }
    return NULL;
}

static void *service_toserver(void *ud){
    msgdisp_t disp = (msgdisp_t)ud;
    int32_t err = 0;
    disp->listen(disp,2,to_server_ip,to_server_port,&err);
    while(!stop){
        msg_loop(disp,50);
    }
    return NULL;
}



int main(int argc,char **argv)
{
    setup_signal_handler();
    InitNetSystem();

    asynnet_t asynet = asynnet_new(3);//3个poller,1个用于监听,1个用于处理客户端连接，1个用于处理服务器连接

    msgdisp_t  disp_to_server = new_msgdisp(asynet,
                                  to_server_connect,
                                  to_server_connected,
                                  NULL,
                                  to_server_process,
                                  NULL);

    msgdisp_t  disp_to_client = new_msgdisp(asynet,
                                  to_client_connect,
                                  NULL,
                                  NULL,
                                  to_client_process,
                                  NULL);

    thread_t service1 = create_thread(THREAD_JOINABLE);
    thread_t service2 = create_thread(THREAD_JOINABLE);

    to_client_ip = argv[1];
    to_client_port = atoi(argv[2]);


    to_server_ip = argv[3];
    to_server_port = atoi(argv[4]);

    thread_start_run(service1,service_toserver,(void*)disp_to_server);
    sleepms(1000);
    thread_start_run(service2,service_toclient,(void*)disp_to_client);

    while(!stop){
        sleepms(100);
    }

    thread_join(service1);
    thread_join(service2);

    CleanNetSystem();
    return 0;
}

/*
    广播包示例,场景服务器中的代码
    void BroadCast(wpacket_t wpk,sock_ident gate,sock_ident *client,uint16_t client_size)
    {
        int i = 0;
        wpacket_t broadcast_wpk = wpk_create(4096,0);
        wpk_write_uint16(client_size);
        for(; i < client_size; ++i)
            wpk_write_sock(broadcast_wpk,client[i]);
        //将真正需要发送的包
        wpk_write_wpk(broadcast_wpk,wpk);
        asyn_send(gate,broadcast_wpk);
        wpacket_destroy(wpk);
    }
*/

/*
    网关中的代码
    uint16_t size = rpk_read_uint16(rpk);//这个包需要发给多少个客户端
    int i = 0;
    wpacket_t wpk = wpk_create_by_rpacket(rpk,size*sizeof(sock_ident),PACKET_RAW(rpk));
    //发送给所有需要接收的客户端
    for( ; i < size; ++i)
    {
        sock_ident client = rpk_read_sock(rpk);
        asyn_send(client,wpk);
    }
*/




