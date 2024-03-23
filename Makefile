CC = gcc
CPPFLAGS = -pthread -O3

TARGET := solution

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all run
