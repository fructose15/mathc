# MathC Makefile Template (generated into Makefile by ./configure)

CC       = gcc
CFLAGS   = -O2 -Wall -Wextra -std=c99 -I. -DMCC_INSTALL_PREFIX='"/usr/local/bin"'
LDFLAGS  = -lm
PREFIX   = /usr/local
BINDIR   = /usr/local/bin
PLATFORM = linux

TARGET   = mcc
SRC      = src/mcc.c

.PHONY: all clean install uninstall test

all: $(TARGET)

$(TARGET): $(SRC) config.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

test: $(TARGET)
	./$(TARGET) --help
	./$(TARGET) --command "calculate 1 + 1"
	./$(TARGET) --command "calculate 5 x 5"
	./$(TARGET) --command "compute (10 ÷ 5) x 4"
	./$(TARGET) run examples/basics.mc

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Installed MathC compiler to $(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET) *.o examples/*.o
