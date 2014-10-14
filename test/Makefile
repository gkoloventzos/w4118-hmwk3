CC := arm-none-linux-gnueabi-gcc
LD := arm-none-linux-gnueabi-gcc
CFLAGS := -g -Wall -Werror
LDFLAGS := -static

OBJECTS := shake.c
EXE := shake

all : $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(EXE) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $^

clean:
	rm -f $(EXE)

.PHONY: clean
