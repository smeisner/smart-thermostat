CC=gcc
CFLAGS=-I. --std=gnu99 -Wunused -Wall -Wextra
DEPS = thermostat.h
OBJ = thermostat.o am2321.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

thermostat: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
