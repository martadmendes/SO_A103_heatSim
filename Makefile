# Makefile, versao 02
# Sistemas Operativos, DEI/IST/ULisboa 2017-18

CFLAGS= -g -Wall -pedantic
CC=gcc

heatSim: main.o matrix2d.o mplib3.o leQueue.o
	$(CC) $(CFLAGS) -o heatSim main.o matrix2d.o mplib3.o leQueue.o

main.o: main.c matrix2d.h
	$(CC) $(CFLAGS) -c main.c

matrix2d.o: matrix2d.c matrix2d.h
	$(CC) $(CFLAGS) -c matrix2d.c

mplib3.o: mplib3.c mplib3.h
	$(CC) $(CFLAGS) -c mplib3.c

leQueue.o: leQueue.c leQueue.h
	$(CC) $(CFLAGS) -c leQueue.c

clean:
	rm -f *.o heatSim

zip:
	zip heatSim_ex03_solucao.zip main.c matrix2d.c matrix2d.h mplib3.c mplib3.h leQueue.c leQueue.h Makefile

run:
	./heatSim 8 10 10 0 0 10 4 1 1

