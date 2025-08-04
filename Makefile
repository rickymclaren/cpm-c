CC=cc
CFLAGS=-Iinclude -Wall -Wextra
SRC=src/main.c
OBJ=$(SRC:.c=.o)
EXEC=cpm-c

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)