/*
*  һ���̰߳�ȫ�ĵ�����Ϣ����,�ṩ���̶߳Զ��е�N��Nд.
*  ÿ��������һ���������̹߳���Ĺ�������,ͬʱÿ���߳����Լ�˽�е�push/pop����
*  һ���߳�����Ϣ������push��Ϣʱ,���Ƚ���Ϣpush���Լ�˽�е�push������,Ȼ���жϵ�ǰ
*  ˽��push���е���Ϣ�����Ƿ񳬹���Ԥ���ķ�ֵ,��������ͽ�˽�ж����е���Ϣȫ��ͬ����
*  ������������.
*  ��һ���̴߳���Ϣ������pop��Ϣʱ,���Ȳ鿴�Լ���˽��pop�������Ƿ�����Ϣ,�����ֱ�Ӵ�
*  ˽����Ϣ������popһ������,���˽����Ϣ������û����Ϣ,���Դӹ�������н�������Ϣ
*  ͬ����˽�ж���.
*
*  ͨ����˽�к͹�����еĻ���,�����ļ�������Ϣ���в�������������ʹ�ô���.
*
*  [ע��1]
*  һ���̶߳�ͬһ������ֻ�ܴ���һ��ģʽ,
*  �̵߳�һ�ζԶ���ִ�в���(д/��)���Ժ�����̶߳�������о�ֻ�ܴ��ڴ�ģʽ��.
*  �����߳�A��һ�ζԶ���ִ����push��������ô�Ժ�A��������о�ֻ��ִ��push,
*  �������A�Զ���ִ����pop������������
*
*  [ע��2]
*  �����޷���һ��Ƶ��ִ�ж��г�ˢ����(������push�����е���Ϣͬ�����������)���߳�
*  Ϊ���������߳��ּ������ʹ��push_now,�ú����ڽ���Ϣpush�����ض��к�����ִ��ͬ������
*/

#ifndef _MSG_QUE_H
#define _MSG_QUE_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "refbase.h"
#include "double_link.h"
#include "link_list.h"
#include "sync.h"

//�������ٶ����в�������Ϣ
typedef void (*item_destroyer)(void*);

typedef struct msg_que
{
	struct refbase      refbase;
	struct link_list    share_que;
	uint32_t            syn_size;
	volatile uint8_t    wait4destroy;
	pthread_key_t       t_key;
	mutex_t             mtx;
	struct double_link  blocks;
	item_destroyer      destroy_function;
}*msgque_t;

//Ĭ�����ٺ����Բ���Ԫ��ִ��free
void default_item_destroyer(void* item);

//���Ӷ�que������
static inline struct msg_que* msgque_acquire(msgque_t que){
    ref_increase((struct refbase*)que);
    return que;
}

//�ͷŶ�que������,��������Ϊ0,que�ᱻ����
void msgque_release(msgque_t que);

struct msg_que* new_msgque(uint32_t syn_size,item_destroyer);

void delete_msgque(void*);

/*
* ����ر���Ϣ���е����ͷ�����,Ҫ�ͷ����ã���������msg_que_release,ȷ��acquire��release����Ե���
* �ر�ʱ��������̵߳ȴ��ڴ���Ϣ�����лᱻȫ������
*/
void close_msgque(msgque_t que);


/* pushһ����Ϣ��local����,���local�����е���Ϣ����������ִֵ��ͬ��������ͬ��
* ����0������-1�������ѱ�����ر�
*/
int8_t msgque_put(msgque_t,list_node *msg);

/* pushһ����Ϣ��local����,��������ͬ��
* ����0������-1�������ѱ�����ر�
*/
int8_t msgque_put_immeda(msgque_t,list_node *msg);

/*��local����popһ����Ϣ,���local������û����Ϣ,���Դӹ������ͬ����Ϣ����
* ������������û��Ϣ�����ȴ�timeout����
* ����0������-1�������ѱ�����ر�
*/
int8_t msgque_get(msgque_t,list_node **msg,uint32_t timeout);


//��ˢ���̳߳��е�������Ϣ���е�local push����
void   msgque_flush();


#endif
