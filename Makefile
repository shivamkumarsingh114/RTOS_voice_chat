BUILD   = ./build
BIN     = ./bin
INCLUDE = ./
EX = ./examples

all: $(BIN)/rec $(BIN)/play


$(BIN)/play:
	gcc $(EX)/play_sample.c -lpulse -lpulse-simple -o $(BIN)/play

$(BIN)/rec:
	gcc $(EX)/recordaudio.c -lpulse -lpulse-simple -o $(BIN)/rec
