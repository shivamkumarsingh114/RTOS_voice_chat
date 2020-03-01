BUILD   = ./build
BIN     = ./bin
INCLUDE = ./
EX = ./examples
ds_src = $(wildcard src/ds/*.c)
serv_src = $(wildcard src/core/server.c src/main.c)
client_core_src = $(wildcard src/core/client.c)
ds_obj = $(ds_src:.c=.o)
serv_obj = $(serv_src:.c=.o)
client_core_obj = $(client_core_src:.c=.o)
client_obj = src/ext/client.o
objs = $(ds_obj) $(serv_obj) $(client_core_obj) $(client_obj)

LDFLAGS = -lpthread
CC = cc -I include/


all: server client $(BIN)/rec $(BIN)/play

server: $(serv_obj) $(ds_obj)
	$(CC) -o bin/$@ $^ $(LDFLAGS)
client_core: $(ds_obj) $(client_core_obj)
	$(CC) $^ $(LDFLAGS)

client: $(client_obj) $(ds_obj) $(client_core_obj)
	$(CC) -o bin/$@ $^ $(LDFLAGS)
$(BIN)/play:
	gcc $(EX)/playaudio.c -lpulse -lpulse-simple -o $(BIN)/play

$(BIN)/rec:
	gcc $(EX)/recordaudio.c -lpulse -lpulse-simple -o $(BIN)/rec

.PHONY: clean
clean:
	rm -rf $(BIN)/*
