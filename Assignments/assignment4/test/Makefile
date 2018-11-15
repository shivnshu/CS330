all:put get delete update rename get_one create_one create_threaded get_threaded
CC = gcc
LDFLAGS = -pthread
OBJS = kvstore.o create.o find_and_read.o delete.o update.o rename.o find_one_and_read.o create_one.o create_threaded.o find_and_read_threaded.o

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@
put: $(OBJS)   
	$(CC) -o $@ create.o kvstore.o $(LDFLAGS)
create_one: $(OBJS)   
	$(CC) -o $@ create_one.o kvstore.o $(LDFLAGS)
create_threaded: $(OBJS)   
	$(CC) -o $@ create_threaded.o kvstore.o $(LDFLAGS)
get: $(OBJS)    
	$(CC) -o $@ find_and_read.o kvstore.o $(LDFLAGS)
get_one: $(OBJS)    
	$(CC) -o $@ find_one_and_read.o kvstore.o $(LDFLAGS)
get_threaded: $(OBJS)    
	$(CC) -o $@ find_and_read_threaded.o kvstore.o $(LDFLAGS)
delete: $(OBJS)    
	$(CC) -o $@ delete.o kvstore.o $(LDFLAGS)
update: $(OBJS)    
	$(CC) -o $@ update.o kvstore.o $(LDFLAGS)
rename: $(OBJS)    
	$(CC) -o $@ rename.o kvstore.o $(LDFLAGS)

.Phony: clean
clean:
	rm -f *.o; rm -f put get delete update rename get_one create_one;
