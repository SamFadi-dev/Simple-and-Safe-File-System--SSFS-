# Install libbsd-dev via "sudo apt-get install libbsd-dev"
CC = gcc
FLAGS = -Wall -pedantic -std=c99 -Wextra -D_POSIX_SOURCE -lbsd

SRC = main.c fs.c error.c vdisk/vdisk.c
OBJ = $(SRC:.c=.o)

TARGET = fs_test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(FLAGS) -o $@ $^ -lbsd

clean:
	rm -f $(TARGET) *.o vdisk/*.o

.PHONY: all clean
