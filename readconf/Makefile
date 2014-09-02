INSTALL_LIB=/usr/local/lib
STATIC_LIB=libreadconf.a
DYNAIMIC_LIB=libreadconf.so

.PHONY:libreadconf
libreadconf: readconf.o
	ar -cvq $(STATIC_LIB) readconf.o
	gcc -Wall readconf.o -shared -o $(DYNAIMIC_LIB)
readconf.o:
	gcc -Wall -c readconf.c
install: libreadconf.a
	install readconf.h /usr/local/include
	install $(STATIC_LIB) $(DYNAIMIC_LIB) $(INSTALL_LIB)
uninstall: 
	rm /usr/local/include/readconf.h
	rm $(INSTALL_LIB)/$(STATIC_LIB)
	rm $(INSTALL_LIB)/$(DYNAIMIC_LIB)

clean:
	rm *.o  $(STATIC_LIB) $(DYNAIMIC_LIB)
