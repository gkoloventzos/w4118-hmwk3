CC := arm-none-linux-gnueabi-gcc
LD := arm-none-linux-gnueabi-gcc
CFLAGS := -g -Wall -Werror
LDFLAGS := -static

OBJECTS := set_acceleration.c
EXE := set_acc

all : $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(EXE) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $^

clean:
	rm -f $(OBJECTS)
	rm -f $(EXE)

.PHONY: clean
