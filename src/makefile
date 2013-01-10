CC = gcc
LIBS = -lrt

all: kftserver kftclient

kftclient: kftclient.o socket_util.o helperlib.o dropper.o rdt_udp.o
	$(CC) $(LIBS) kftclient.o socket_util.o helperlib.o dropper.o rdt_udp.o -o kftclient -Wall
kftserver: kftserver.o socket_util.o helperlib.o dropper.o rdt_udp.o
	$(CC) $(LIBS) kftserver.o socket_util.o helperlib.o dropper.o rdt_udp.o -o kftserver -Wall
kftclient.o: kftclient.c socket_util.h helperlib.h dropper.h rdt_udp.h
	$(CC) -c kftclient.c -Wall
kftserver.o: kftserver.c socket_util.h helperlib.h dropper.h rdt_udp.h
	$(CC) -c kftserver.c -Wall
rdt_udp.o: rdt_udp.c rdt_udp.h dropper.h helperlib.h
	$(CC) -c rdt_udp.c -Wall
dropper.o: dropper.h dropper.c
	$(CC) -c dropper.c -Wall
socket_util.o: socket_util.c socket_util.h helperlib.h
	$(CC) -c socket_util.c -Wall
helperlib.o: helperlib.c helperlib.h
	$(CC) -c helperlib.c -Wall
