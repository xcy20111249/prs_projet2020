all: serveur1

serveur1: serveur1.o
	gcc serveur1.o -o serveur1

serveur1.o: serveur1.c
	gcc -Wall -c serveur1.c -o serveur1.o

clean:
	\rm -rf *.o *~
