CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I../src

# Source files
SRCS = ../src/tinyfs_eeprom_mock.c ../src/tinyfs_storage.c ../src/tinyfs_crc.c ../src/tinyfs_record.c ../src/tinyfs_metadata.c ../src/tinyfs_gc.c ../src/tinyfs_cli.c ../src/tinyfs.c test_tinyfs.c
OBJS = $(SRCS:.c=.o)
TARGET = test_tinyfs

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: all
	./$(TARGET)

.PHONY: all clean run
