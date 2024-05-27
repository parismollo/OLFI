# Variables
CC = gcc
# CFLAGS = -Wall
TARGET = benchmark
SOURCES = main.c benchmark.c

# Default target
all: $(TARGET)

# Rule to link the executable
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

# Clean up
clean:
	rm -f test_files/*
	rm -f /mnt/ouichefs/test_files/*
	rm -f $(TARGET)
