# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -Isrc/include  -lssl -lcrypto -lpthread

# Directories
SRCDIR = src/source
INCDIR = src/include
BINDIR = bin/source
DOCDIR = doc

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)

# Object files
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(BINDIR)/%.o,$(SOURCES))

# Executable
EXECUTABLE = bin/secure-proxy

# Targets
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) src/main.c
	$(CC) $(OBJECTS) src/main.c -o $(EXECUTABLE) $(CFLAGS) 

$(BINDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/*.h | $(BINDIR)
	$(CC)  -c $< -o $@ $(CFLAGS)

$(BINDIR):
	mkdir -p $(BINDIR)

doc:
	doxygen Doxyfile

clean:
	rm -rf bin/*
	rm -rf doc/doxygen

.PHONY: all doc clean
