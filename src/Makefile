SOURCES=*.c
OPTIONS=-lstdc++ -shared -s
WARNINGS=-Wall -Wextra
OUTPUT=-o libhack.dll
OBJECT_FILES=*.o
OUTLIB=-Wl,--out-implib,libhack.a
CC=gcc
DEFS=-DVERBOSE

all:
	$(CC) -c -fpic $(WARNINGS) $(SOURCES)
	$(CC) $(OPTIONS) $(WARNINGS) $(OUTPUT) $(OBJECT_FILES) $(DEFS) $(OUTLIB)
	$(RM) *.o

clean:
	$(RM) *.o *.dll *.exe *.so *.a -vv