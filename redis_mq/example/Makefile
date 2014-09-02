EXEC=example
#REDIS_MQ_LIB=/usr/local/lib/libredismq.a
#READCONF_LIB=/usr/local/lib/libreadconf.a
all:
	gcc -pthread -lhiredis -lredismq -o $(EXEC) example.c $(REDIS_MQ_LIB) $(READCONF_LIB)
	gcc -pthread -lhiredis -lredismq -o $(EXEC) example.c -lreadconf -lreadismq
clean:
	rm $(EXEC)
install:
	#cp  ./$(EXEC) /home/sippbx/sbin

