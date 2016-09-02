CC=cc

#LFLAGS=-lsocket        # SCO Open Server
LFLAGS=-lsocket -lnsl # Sun Solaris
#LFLAGS=               # Linux

all: proxyd

clean: 
	rm *.o

proxyd: proxyd.o 
	$(CC) -o proxyd proxyd.o $(LFLAGS)

proxyd.o: proxyd.c 
