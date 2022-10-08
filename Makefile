CC = gcc
CCFLAGS = -W -Wall -pedantic -std=c99
TARGET = packer
SOURCES = example/main.c example/tar.c tarchiver.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all release debug clean

all: release

release: CCFLAGS += -O3
release: build

debug: CCFLAGS += -Og -ggdb3
debug: build

clean:
	@rm -rf $(TARGET) $(OBJECTS)
	@echo "Cleanup finished!"

build: $(OBJECTS)
	@$(CC) $^ -o $(TARGET)
	@echo "Linking finished!"

%.o: %.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"
