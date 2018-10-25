
LFLAGS   = -L/usr/lib/arm-linux-gnueabihf

temper:  temper.c
	gcc $(LFLAGS) -I/usr/include/lua5.1 -o temper temper.c -llua5.1
