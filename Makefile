all:main.o ftl.o
	gcc -o ftl main.o ftl.o
	
main.o:ftl.h main.c
	gcc -c main.c

ftl.o: ftl.h ftl.c
	gcc -c ftl.c
