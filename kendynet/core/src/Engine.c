#include "KendyNet.h"
#include "link_list.h"
#include <stdlib.h>
#include <assert.h>
#include "common.h"
#include "Engine.h"

#include "epoll.h"
engine_t create_engine()
{
	engine_t e = malloc(sizeof(*e));
	if(e)
	{
		e->Init = epoll_init;
		e->Loop = epoll_loop;
		e->Register = epoll_register;
		e->UnRegister = epoll_unregister;
		double_link_clear(&e->actived);
		double_link_clear(&e->connecting);
	}
	return e;
}

void   free_engine(engine_t *e)
{
	assert(e);
	assert(*e);
	free(*e);
	*e = 0;
}
