################################
# Authors: Mike Hale 004620459 #
#          Edwin Do  904637634 #
# Date:    02/18/2018          #
# File:    Makefile            #
################################
.SILENT:
build:
	gcc -std=gnu99 -pthread -o client client.c sel_repeat.c
	gcc -std=gnu99 -pthread -o server server.c sel_repeat.c

gdb:
	gcc -g -std=gnu99 -pthread -o client_d client.c sel_repeat.c
	gcc -g -std=gnu99 -pthread -o server_d server.c sel_repeat.c

clean:
	rm -f server client server_d client_d
	rm -r -f client_d.dSYM server_d.dSYM
	rm -f project2_004620459_904637634.tar.gz

dist:
	tar -czf project2_004620459_904637634.tar.gz client.c server.c sel_repeat.c sel_repeat.h Makefile