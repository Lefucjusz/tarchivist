CC = gcc
CFLAGS = -W -Wall -std=c99 -O3
TARGET = tarchiver
SOURCES = example/main.c example/tar.c tarchiver.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

clean:
	@rm -rf $(TARGET) $(OBJECTS)
	@echo "Cleanup finished!"

$(TARGET) : $(OBJECTS)
	@$(CC) $^ -o $@
	@echo "Linking finished!"

%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"
