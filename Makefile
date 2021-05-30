TARGET = bin/main
CC = gcc

SRCS = $(shell find src/ -type f -name '*.c')
CFLAGS = -Wall -Os -s

LIBDIR = $(shell find lib/ -type f -name '*.a')
LIBS = -lm -ldl -lpthread

$(TARGET): $(SRCS) $(LIBDIR)
	@mkdir -p bin/
	$(CC) $^ -o $@ $(CFLAGS) $(LIBS)

run: $(TARGET)
	./bin/main

clean:
	rm -r bin/
