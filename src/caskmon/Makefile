CC = clang
INCLUDES = -I..
DEFINES = -D_GNU_SOURCE
CFLAGS = -Weverything -Wno-padded -std=c11 -c $(DEFINES) $(INCLUDES)
BASEDIR = ../..
LDFLAGS = 
SRCDIR = .
OBJDIR = $(BASEDIR)/build/caskmon
SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))
EXECUTABLE = $(BASEDIR)/bin/caskmon

all: DEFINES += -DNDEBUG=0
all: CFLAGS += -O2
all: caskmon
debug: DEFINES += -DNDEBUG=1
debug: CFLAGS += -g
debug: caskmon

caskmon: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(EXECUTABLE)

$(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)
