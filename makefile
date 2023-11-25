
CC := gcc
CFLAGS := -Wno-pointer-to-int-cast `pkg-config --libs --cflags icu-uc icu-io`

SRC_DIR := ./src
OBJECTS := $(patsubst %.c,%.o,$(wildcard $(SRC_DIR)/*.c))

EXE_NAME := captioncompiler
 
compile: $(OBJECTS)
	$(CC) -O3 $(OBJECTS) -o $(EXE_NAME) $(CFLAGS)

clean:
	rm -f $(SRC_DIR)/*.o
