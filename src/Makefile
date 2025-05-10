# Install libbsd-dev via "sudo apt-get install libbsd-dev"
CC = gcc
CFLAGS = -Wall -pedantic -std=c99 -Wextra -D_POSIX_SOURCE -lbsd -Iinclude

SRC = main.c error.c fs.c ssfs.c vdisk/vdisk.c
OBJ = $(SRC:.c=.o)

TARGET = fs_test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lbsd

clean:
	rm -f $(TARGET) *.o vdisk/*.o

.PHONY: all clean
