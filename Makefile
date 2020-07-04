CC = clang
INCLUDES = -Isrc
DEFINES = -D_GNU_SOURCE
CFLAGS = -Weverything -Wno-padded -Wno-disabled-macro-expansion -std=c11 -c $(DEFINES) $(INCLUDES)
LDFLAGS = -lpthread
SRCDIR = src
OBJDIR = build/cask
SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))
EXECUTABLE = bin/cask

all: DEFINES += -DNDEBUG=0
all: CFLAGS += -O2
all: cask caskmon
debug: DEFINES += -DNDEBUG=1
debug: CFLAGS += -g
debug: cask caskmon

cask: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(EXECUTABLE)

caskmon:
	$(MAKE) -C src/caskmon $(MAKECMDGOALS)

$(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf bin/*
	rm -rf build/*
