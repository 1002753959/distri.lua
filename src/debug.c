#include "Hook.h"
#include "debug.h"

int (*ori_luaL_loadfilex)(lua_State *L, const char *filename,const char *mode) = NULL;

int my_luaL_loadfilex(lua_State *L, const char *filename,const char *mode){
	printf("%s\n",filename);//记录导入的lua文件，供调试器使用
	return ori_luaL_loadfilex(L,filename,mode);
}

int debug_init(){
	ori_luaL_loadfilex = HookFunction(luaL_loadfilex,my_luaL_loadfilex);
	if(!ori_luaL_loadfilex){
		return -1;
	}	
	return 0;
}
