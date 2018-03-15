# Makefile for the Fibonacci Application

# Files
SOURCES=fibonacci-app.c
OBJECTS=$(SOURCES:.c=.o)
EXEC=fibonacci-app

# Define extra flags here
#EXTRACFLAGS=-ansi

all: $(SOURCES) $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) $(EXTRACFLAGS) $^ -o $@

%.o : %.c
	$(CC) $(CFLAGS) $(EXTRACFLAGS) -c $<

install:
	install $(EXEC) $(DESTDIR)/usr/bin

clean:
	rm -rf $(OBJECTS) $(EXEC)
