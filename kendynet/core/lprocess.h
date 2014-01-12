#ifndef _LPROCESS_H
#define _LPROCESS_H

#include "minheap.h"
#include "uthread.h"
#include "dlist.h"
#include "llist.h"

/*
* ���������̣��û����̣߳���������,��Ҫ����ʵ�ֶ��û��Ѻõ��첽rpc����
*/

//����������״̬
enum
{
    _YIELD = 0,
    _SLEEP,
    _ACTIVE,
    _DIE,
    _START,
    _BLOCK,
};

typedef struct lprocess
{
    struct lnode   next;
    struct dnode   dblink;
    struct heapele _heapele;
    uthread_t      ut;
    void          *stack;
    uint8_t        status;
    uint32_t       timeout;
    void          *ud;
    void* (*star_func)(void *);

}*lprocess_t;

typedef struct lprocess_scheduler
{
    lprocess_t lp;
    int32_t    stack_size;
    minheap_t  _timer;
    int32_t    size;
}*lprocess_scheduler_t;

//����һ��lprocess��ִ��start_fun
int32_t lp_spawn(void*(*start_fun)(void*),void*arg);

//�õ�ǰlprocess����ms����
void    lp_sleep(int32_t ms);

//��ǰlprocess�ó�ִ��Ȩ
void    lp_yield();

//������ǰlprocess(ֻ��������lprocess�����������)
void    lp_block();

void    lp_schedule();

#endif
