################################
# Authors: Mike Hale 004620459 #
#          Edwin Do  904637634 #
# Date:    02/18/2018          #
# File:    Makefile            #
################################
.SILENT:
build:
	gcc -D CON_CTRL=0 -std=gnu99 -pthread -o client client.c sel_repeat.c
	gcc -D CON_CTRL=0 -std=gnu99 -pthread -o server server.c sel_repeat.c

congest:
	gcc -D CON_CTRL=1 -std=gnu99 -pthread -o client_c client.c sel_repeat.c
	gcc -D CON_CTRL=1 -std=gnu99 -pthread -o server_c server.c sel_repeat.c

gdb:
	gcc -D CON_CTRL=0 -g -std=gnu99 -pthread -o client_d client.c sel_repeat.c
	gcc -D CON_CTRL=0 -g -std=gnu99 -pthread -o server_d server.c sel_repeat.c

clean:
	rm -f server client server_d client_d
	rm -r -f client_d.dSYM server_d.dSYM
	rm -f project2_004620459_904637634.tar.gz
	rm -f output output.txt

dist:
	tar -czf project2_004620459_904637634.tar.gz client.c server.c sel_repeat.c sel_repeat.h Makefile README report.pdf