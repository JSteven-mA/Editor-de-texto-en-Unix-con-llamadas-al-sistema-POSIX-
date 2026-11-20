CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -g -D_GNU_SOURCE
ARCHSALIDA = eafitOS
SRCS = main.c cat_datos.c cat_memoria.c cat_monitoreo.c cat_util.c cat_editor.c
OBJS = $(SRCS:.c=.o)

EDITOR = editor
EDITOR_OBJS = editor_main.o editor.o

# editor se compila primero: el shell lo lanza con fork/execv
all: $(EDITOR) $(ARCHSALIDA)

$(ARCHSALIDA): $(OBJS)
	$(CC) $(CFLAGS) -o $(ARCHSALIDA) $(OBJS)
	./$(ARCHSALIDA)

$(EDITOR): $(EDITOR_OBJS)
	$(CC) $(CFLAGS) -o $@ $(EDITOR_OBJS)

editor_main.o editor.o: editor.h

%.o: %.c shell.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ARCHSALIDA) $(EDITOR) $(OBJS) $(EDITOR_OBJS)

.PHONY: all clean
