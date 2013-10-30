all:luanet.c
	gcc -g -c -fPIC luanet.c -I./kendynet
	gcc -g -shared -o luanet.so luanet.o kendynet.a -lpthread -lrt -ltcmalloc
	rm -f *.o
