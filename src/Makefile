all: serveur1 serveur2 slidingWindow

serveur1: serveur1.o
	gcc serveur1.o -o serveur1

serveur1.o: serveur1.c
	gcc -Wall -c serveur1.c -o serveur1.o

slidingWindow: slidingWindow.o
	gcc slidingWindow.o -o slidingWindow

slidingWindow.o: slidingWindow.c
	gcc -Wall -c slidingWindow.c -o slidingWindow.o

serveur2: serveur2.o
	gcc serveur2.o -o serveur2

serveur2.o: serveur2.c
	gcc -Wall -c serveur2.c -o serveur2.o

clean:
	\rm -rf *.o *~
