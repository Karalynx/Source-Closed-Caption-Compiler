CC = gcc
CFLAGS = `pkg-config --libs --cflags icu-uc icu-io`
OBJECTS = $(EXE_NAME).o caption_list.o caption.o ustring.o buffer.o valve_crc32.o
EXE_NAME = captioncompiler
 
$(EXE_NAME): $(OBJECTS)
	$(CC) -O2 $(OBJECTS) -o ../$(EXE_NAME) $(CFLAGS)

$(EXE_NAME).o: $(EXE_NAME).c
	$(CC) -O2 -c $(EXE_NAME).c

caption_list.o: caption_list.c caption_list.h
	$(CC) -O2 -c caption_list.c

caption.o: caption.c caption.h
	$(CC) -O2 -c caption.c

ustring.o: ustring.c ustring.h
	$(CC) -O2 -c ustring.c

buffer.o: buffer.c buffer.h
	$(CC) -O2 -c buffer.c

valve_crc32.o: valve_crc32.c valve_crc32.h
	$(CC) -O2 -w -c valve_crc32.c

clean:
	rm -f *.o
