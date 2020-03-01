BUILD   = ./build
BIN     = ./bin
INCLUDE = ./
EX = ./examples

all: $(BIN)/rec $(BIN)/play


$(BIN)/play:
	gcc $(EX)/playaudio.c -lpulse -lpulse-simple -o $(BIN)/play

$(BIN)/rec:
	gcc $(EX)/recordaudio.c -lpulse -lpulse-simple -o $(BIN)/rec

clean:
	rm -rf $(BIN)/*
