# Makefile, versao 3
# Sistemas Operativos, DEI/IST/ULisboa 2017-18

CC       = gcc
CFLAGS   = -g -std=gnu99 -Wall -pedantic -pthread

.PHONY: all clean zip

all: heatSim

heatSim: main.o matrix2d.o util.o
	$(CC) $(CFLAGS) -o $@ $+

main.o: main.c matrix2d.h util.h
	$(CC) $(CFLAGS) -o $@ -c $<

matrix2d.o: matrix2d.c matrix2d.h
	$(CC) $(CFLAGS) -o $@ -c $<

util.o: util.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o heatSim

zip: heatSim_p4_solucao.zip

heatSim_p4_solucao.zip: Makefile main.c matrix2d.h util.h matrix2d.c matrix2d.h util.c
	zip $@ $+

run:
	./heatSim 8 10 10 0 0 10 4 1 results 2
