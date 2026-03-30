CC      = clang
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic $(shell sdl2-config --cflags)
LIBS    = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm
OUTDIR  = out
TARGET  = $(OUTDIR)/super-mango
SRCDIR  = src
SRCS    = $(wildcard $(SRCDIR)/*.c)
OBJS    = $(SRCS:.c=.o)
DEPS    = $(OBJS:.o=.d)

.PHONY: all clean run run-debug run-sandbox run-sandbox-debug

all: $(OUTDIR) $(TARGET)

$(OUTDIR):
	mkdir -p $(OUTDIR)

UNAME := $(shell uname -s)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
ifeq ($(UNAME),Darwin)
	codesign --force --sign - $@
endif

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEPS)

run: all
	./$(TARGET)

run-debug: all
	./$(TARGET) --debug

run-sandbox: all
	./$(TARGET) --sandbox

run-sandbox-debug: all
	./$(TARGET) --sandbox --debug

clean:
	rm -f $(SRCDIR)/*.o
	rm -f $(SRCDIR)/*.d
	rm -rf $(OUTDIR)
