# Filename: Makefile
# Description: The make file is to build up the libredismq.a
# Warning:  See how to use CFLAGS1 to build file.c
#
CREATED_STATIC_LIB=libredismq.a
READCONF_LIB=/usr/local/lib/libreadconf.a -lhiredis
CONF_FILE=redis_mq.conf

.PHONY:clean install uninstall
all:$(CREATED_STATIC_LIB)

$(CREATED_STATIC_LIB): redismq.o
	ar -cvq $(CREATED_STATIC_LIB) redismq.o
redismq.o:
	gcc -Wall -lphtread -lhiredis -c  redismq.c -L $(READCONF_LIB)
clean:
	rm *.o  $(CREATED_STATIC_LIB)
install:$(CREATED_STATIC_LIB)
	cp redismq.h /usr/local/include
	cp libredismq.a /usr/local/lib/
	cp $(CONF_FILE) /etc/
uninstall:
	rm /usr/local/include/redismq.h 
	rm /usr/local/lib/$(CREATED_STATIC_LIB)
	rm /etc/$(CONF_FILE)
