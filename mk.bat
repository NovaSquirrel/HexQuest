gcc -Wall -Os -DWIN32 -c hexquest.c
dllwrap --def plugin.def --dllname hexquest.dll hexquest.o
pause