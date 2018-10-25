CFLAGS = -Wall
IFLAGS = -I/usr/include/lua5.1
LFLAGS = -L/usr/lib/arm-linux-gnueabihf

temper: temper.c lutils.lua
	gcc $(CFLAGS) $(IFLAGS) $(LFLAGS) $< -o $@ -llua5.1

clean:
	rm -f temper
