#
# Makefile do projeto
#

SOURCES=*.c
OPTIONS=-lstdc++ -shared -s
WARNINGS=-Wall -Wextra
OUTPUT=-o libhack.dll
OBJECT_FILES=*.o
OUTLIB=-Wl,--out-implib,libhack.a
CC=gcc
DEFS=-DVERBOSE

.PHONY: examples dll

# Compila a biblioteca por completo
all: dll examples

# Compila apenas a biblioteca
dll:
	$(CC) -c -fpic $(WARNINGS) $(SOURCES)
	$(CC) $(OPTIONS) $(WARNINGS) $(OUTPUT) $(OBJECT_FILES) $(DEFS) $(OUTLIB)
	$(RM) *.o

# Compila os exemplos
examples:
	$(CC) examples/read_addr.c -o examples/read_addr -L. -lhack
	$(CC) examples/write_addr.c -o examples/write_addr -L. -lhack
	$(CC) examples/pid.c -o examples/pid -L. -lhack

# Limpa os arquivos binários
clean:
	$(RM) *.o *.dll *.exe *.so *.a -vv
	$(RM) examples/*.o examples/*.dll examples/*.exe examples/*.so examples/*.a -vv