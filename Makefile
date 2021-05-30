TARGET = bin/main
CC = gcc

INCLUDE = $(shell find src/ -type f -name '*.h')

SRCS = $(shell find src/ -type f -name '*.c')
CFLAGS = -Wall -Os -s

LIBDIR = $(shell find lib/ -type f -name '*.a')
LIBS = -lm -ldl -lpthread

$(TARGET): $(SRCS) $(LIBDIR) $(INCLUDE)
	@mkdir -p bin/
	$(CC) $(SRCS) $(LIBDIR) -o $@ $(CFLAGS) $(LIBS)

run: $(TARGET)
	./bin/main

clean:
	rm -r bin/
