#Makefile

CC = gcc
MY_INC = ./include

exe: main.o copy.o
	$(CC) -o $@ $^
main.o: main.c
	$(CC) -c $^ -I$(MY_INC)
copy.o: copy.c
	$(CC) -c $^ -I$(MY_INC)
