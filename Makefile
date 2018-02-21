################################
# Authors: Mike Hale 004620459 #
#          Edwin Do  xx4xxxxxx #
# Date:    02/18/2018          #
# File:    Makefile            #
################################

build:
	gcc -std=gnu99 -pthread -o client client.c sel_repeat.c
	gcc -std=gnu99 -pthread -o server server.c sel_repeat.c

gdb:
	gcc -g -std=gnu99 -pthread -o client_d client.c sel_repeat.c
	gcc -g -std=gnu99 -pthread -o server_d server.c sel_repeat.c
