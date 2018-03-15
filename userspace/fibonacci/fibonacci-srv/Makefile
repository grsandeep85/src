# Makefile for the Fibonacci Application
# (c) 2017 Rudolf J Streif, rudolf.streif@ibeeto.com

# Files
SOURCES=fibonacci-srv-tcp.c
OBJECTS=$(SOURCES:.c=.o)
EXEC=fibonacci-srv
LDPATHS = 
LDLIBS =

# Toolchain
INSTALL = install

# Define extra flags here
#EXTRACFLAGS=-ansi

all: $(SOURCES) $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(LDFLAGS) $(EXTRACFLAGS) $^ $(LDPATHS) $(LDLIBS) -o $@

%.o : %.c
	$(CC) $(CFLAGS) $(EXTRACFLAGS) -c $<

install:
	$(INSTALL) -d $(DESTDIR)/usr/bin
	$(INSTALL) -d $(DESTDIR)/etc/init.d
	$(INSTALL) -m 0755 $(EXEC) $(DESTDIR)/usr/bin
	$(INSTALL) -m 0755 fibonacci-srv.init $(DESTDIR)/etc/init.d

clean:
	rm -rf $(OBJECTS) $(EXEC)
