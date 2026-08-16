all: frontend dispatcher worker

CC = gcc
CFLAGS = -Wall -Wextra -g -O0
#CFLAGS = -Wall -O2
#CFLAGS =  -O2  

frontend: frontend.o util.o
	$(CC) $(CFLAGS) $^ -o $@
	
dispatcher: dispatcher.o util.o
	$(CC) $(CFLAGS) $^ -o $@
	
worker: worker.o util.o
	$(CC) $(CFLAGS) $^ -o $@
	
%.o: %.c
	$(CC) $(CFLAGS) -c $<

frontend.o dispatcher.o worker.o util.o: util.h

clean: 
	rm -f *.o frontend dispatcher worker
