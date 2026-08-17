CC = gcc
CFLAGS = -Wall -Wextra -g -O0

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
IMG_DIR = images

CFLAGS += -I$(INC_DIR)

TARGETS = frontend dispatcher worker
BINARIES = $(addprefix $(BIN_DIR)/,$(TARGETS))

.PHONY: all run clean

all: $(BINARIES)

$(BIN_DIR)/frontend: $(OBJ_DIR)/frontend.o $(OBJ_DIR)/util.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/dispatcher: $(OBJ_DIR)/dispatcher.o $(OBJ_DIR)/util.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/worker: $(OBJ_DIR)/worker.o $(OBJ_DIR)/util.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

$(OBJ_DIR)/frontend.o $(OBJ_DIR)/dispatcher.o $(OBJ_DIR)/worker.o $(OBJ_DIR)/util.o: $(INC_DIR)/util.h

IN  ?= input.ppm
OUT ?= output.ppm

run: $(BIN_DIR)/frontend
	./$(BIN_DIR)/frontend $(IMG_DIR)/$(IN) $(IMG_DIR)/$(OUT) 2>logs.txt

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) logs.txt