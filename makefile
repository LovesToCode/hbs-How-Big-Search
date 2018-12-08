CC = gcc
#CFLAGS = -c -g
CFLAGS = -c
LFLAGS = -o
IDIR = /usr/include
INCL = -I. -I$(IDIR)
W = -Wunused
#OUT = /usr/local/bin/hbs
OUT = hbs

#.SUFFIXES: .o .cpp
.c.o:
	$(CC) $(CFLAGS) $(W) $(INCL) $<

OBJ =	hbs.o

all: $(OBJ)
	$(CC) $(LFLAGS) $(OUT) $(OBJ)

clean:
	@rm -f *.o
