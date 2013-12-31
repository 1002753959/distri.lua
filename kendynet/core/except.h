/*	
    Copyright (C) <2012>  <huangweilook@21cn.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/	
#ifndef _EXCEPT_H
#define _EXCEPT_H
#include <setjmp.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "llist.h"

struct exception_frame
{
    lnode   node;
	jmp_buf jumpbuffer;
	int32_t exception;
	int32_t line; 
    const char   *file;
    const char   *func;
    int8_t  is_process;
};

extern pthread_key_t g_exception_key;
extern pthread_once_t g_exception_key_once;


void exception_once_routine();

static inline struct llist *GetCurrentThdExceptionStack()
{
    pthread_once(&g_exception_key_once,exception_once_routine);
    struct llist *expstack = (struct llist*)pthread_getspecific(g_exception_key);
    if(!expstack)
    {
        expstack = calloc(1,sizeof(*expstack));
        llist_init(expstack);
        pthread_setspecific(g_exception_key,expstack);
    }
    return expstack;
}

static inline void expstack_push(struct exception_frame *frame)
{
    struct llist *expstack = GetCurrentThdExceptionStack();
    //printf("push %s,%s\n",frame->file,frame->func);
    LLIST_PUSH_FRONT(expstack,(lnode*)frame);
}

static inline struct exception_frame* expstack_pop()
{
    struct llist *expstack = GetCurrentThdExceptionStack();
    struct exception_frame *frame = LLIST_POP(struct exception_frame*,expstack);
    //if(frame)
    //{
    //    printf("pop %s,%s\n",frame->file,frame->func);
    //}
    return frame;
}

static inline struct exception_frame* expstack_top()
{
    struct llist *expstack = GetCurrentThdExceptionStack();
    return (struct exception_frame*)llist_head(expstack);
}

extern void exception_throw(int32_t code,const char *file,const char *func,int32_t line);

#define TRY do{\
	struct exception_frame  frame;\
    frame.node.next = NULL;\
    frame.file = __FILE__;\
    frame.func = __FUNCTION__;\
    frame.exception = 0;\
    frame.is_process = 1;\
    expstack_push(&frame);\
	if((frame.exception = setjmp(frame.jumpbuffer)) == 0)
	
#define THROW(EXP) exception_throw(EXP,__FILE__,__FUNCTION__,__LINE__)

#define CATCH(EXP) else if(!frame.is_process && frame.exception == EXP?\
                        frame.is_process=1,frame.is_process:frame.is_process)

#define CATCH_ALL else if(!frame.is_process?\
                        frame.is_process=1,frame.is_process:frame.is_process)

#define ENDTRY if(!frame.is_process)\
                    exception_throw(frame.exception,frame.file,frame.func,frame.line);\
               else expstack_pop();\
			}while(0);					

//#define FINALLY
/*���ݵ�ǰ������try�Ĵ����������������ȷ���쳣ջ,�ٷ���*/
#define RETURN  do{struct exception_frame *top;\
                    while((top = expstack_top())!=NULL){\
                        if(strcmp(top->file,__FILE__) == 0 && strcmp(top->func,__FUNCTION__) == 0)\
                        {\
                            expstack_pop();\
                        }else\
                        break;\
                    };\
                }while(0);return

#define EXPNO frame.exception


#endif
