GLIBFLAGS=`pkg-config --cflags glib-2.0`
GLIBLIBS=`pkg-config --libs glib-2.0`

all: memory clean

memory:	memory.o
	gcc -o memory memory.o $(GLIBLIBS)

%.o: %.c
	gcc -c $(GLIBFLAGS) $< -o $@

clean:
	rm -rf *.o
