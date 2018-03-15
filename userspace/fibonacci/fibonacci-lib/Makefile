# Makefile for the Fibonacci Application and Library
# (c) 2017 Rudolf J Streif, rudolf.streif@ibeeto.com

# Package Info
NAME = fibonacci
VERSION_MAJOR = 1
VERSION_MINOR = 0
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR)

# Files
EXECSRC = fibonacci-app.c
EXECOBJ = $(EXECSRC:.c=.o)
TARGET  = $(NAME)

HEADERS = fibonacci.h
LIBSRC = fibonacci.c
LIBOBJ = $(LIBSRC:.c=.o)
ALIBS = lib$(NAME).a
SOLIBS = $(ALIBS:.a=.so)

# Toolchain
INSTALL ?= install

# Define extra flags here
EXTRACFLAGS = -Wall -fPIC
LDPATH = -L.


all: $(ALIBS) $(SOLIBS) $(TARGET)

$(TARGET): $(EXECOBJ)
	$(CC) $(LDFLAGS) $(LDPATH) $^ -lfibonacci -o $@
	
$(ALIBS): $(LIBOBJ)

$(SOLIBS): $(LIBOBJ)

%.o : %.c
	$(CC) $(CFLAGS) $(EXTRACFLAGS) -c $<

lib%.a : %.o
	$(AR) rcs $@ $^

lib%.so : %.o
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$@.$(VERSION_MAJOR) -o $@.$(VERSION) $^
	
.PHONY: install
install:
	$(INSTALL) -d $(DESTDIR)/usr/bin
	$(INSTALL) -d $(DESTDIR)/usr/include
	$(INSTALL) -d $(DESTDIR)/usr/lib
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)/usr/bin
	$(INSTALL) -m 0644 $(HEADERS) $(DESTDIR)/usr/include
	$(INSTALL) -m 0644 $(ALIBS) $(DESTDIR)/usr/lib
	$(INSTALL) -m 0755 $(SOLIBS).$(VERSION) $(DESTDIR)/usr/lib

.PHONY: clean
clean:
	rm -rf $(TARGET) $(EXECOBJ) $(LIBOBJ) $(ALIBS) $(SOLIBS).$(VERSION)
