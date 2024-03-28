CC = gcc
CPPFLAGS = -pthread -O3 -Wall -Wextra

TARGET := solution

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all run
