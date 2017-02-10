gcc -c -o hexquest64.o hexquest.c -std=gnu99 -m64
gcc -o hexquest64.dll -s -shared hexquest64.o -Wl,--subsystem,windows -m64
pause