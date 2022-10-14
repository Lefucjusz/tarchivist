CC = gcc
CCFLAGS = -W -Wall -pedantic -std=c99 -O3
PACKSRCS = examples/packer/main.c examples/packer/packer.c tarchivist.c
PACKSTRSRCS = examples/packer-custom-stream/main.c examples/packer-custom-stream/packer.c tarchivist.c
READSRCS = examples/read-demo/main.c tarchivist.c
WRITESRCS = examples/write-demo/main.c tarchivist.c
OBJDIR = build/obj
PACKOBJS = $(PACKSRCS:%.c=$(OBJDIR)/%.o)
PACKSTROBJS = $(PACKSTRSRCS:%.c=$(OBJDIR)/%.o)
READOBJS = $(READSRCS:%.c=$(OBJDIR)/%.o)
WRITEOBJS = $(WRITESRCS:%.c=$(OBJDIR)/%.o)
BINDIR = build/bin

.PHONY: clean

all: packer packer-custom-stream read-demo write-demo
	@echo "All binaries have been built and written to "$(BINDIR)"!"

packer: $(PACKOBJS)
	@echo -n "Linking... "
	@mkdir -p $(BINDIR)
	@$(CC) $^ -o $(BINDIR)/packer
	@echo "Done!"

packer-debug: CCFLAGS += -Og -ggdb3
packer-debug: $(PACKOBJS)
	@echo -n "Linking... "
	@mkdir -p $(BINDIR)
	@$(CC) $^ -o $(BINDIR)/packer-debug
	@echo "Done!"

packer-custom-stream: $(PACKSTROBJS)
	@echo -n "Linking... "
	@mkdir -p $(BINDIR)
	@$(CC) $^ -o $(BINDIR)/packer-custom-stream
	@echo "Done!"

read-demo: $(READOBJS)
	@echo -n "Copying example-read.tar to "$(BINDIR)"... "
	@cp examples/read-demo/example-read.tar $(BINDIR)
	@echo "Done!"
	@echo -n "Linking... "
	@mkdir -p $(BINDIR)
	@$(CC) $^ -o $(BINDIR)/read-demo
	@echo "Done!"

write-demo: $(WRITEOBJS)
	@echo -n "Linking... "
	@mkdir -p $(BINDIR)
	@$(CC) $^ -o $(BINDIR)/write-demo
	@echo "Done!"

clean:
	@echo "Cleaning up..."
	@rm -rf $(OBJDIR) $(BINDIR)
	@echo "Done!"

$(OBJDIR)/%.o: %.c
	@echo -n "Compiling "$<"... "
	@mkdir -p "$(@D)"
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "Done!"
