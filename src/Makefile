SOURCES=*.cpp
OPTIONS=-lstdc++ -shared -s
WARNINGS=-Wall -Wextra
OUTPUT=-o libhack.dll
OBJECT_FILES=*.o
LIBNAME=libhack.a

all:
	g++ -c -fpic $(WARNINGS) $(SOURCES)
	g++ $(OPTIONS) $(WARNINGS) $(OUTPUT) $(OBJECT_FILES)
	ar rcsv $(LIBNAME) $(OBJECT_FILES)
	./deploy.sh

clean:
	$(RM) *.o *.dll *.exe *.so *.a -vv