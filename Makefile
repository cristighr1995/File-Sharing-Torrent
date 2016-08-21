CC=g++
LIBSOCKET=-lnsl
CCFLAGS=-Wall -g
SRV=server
CLT=client

build: $(SRV) $(CLT)

$(SEL_SRV):$(SRV).cpp
	$(CC) -o $(SRV) $(LIBSOCKET) $(SRV).cpp

$(CLT):	$(CLT).c
	$(CC) -o $(CLT) $(LIBSOCKET) $(CLT).c

clean:
	rm -f *.o *~
	rm -f $(SRV) $(CLT)


