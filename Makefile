CC = gcc
CFLAGS = -Wall -Wextra -I$(INC_DIR)

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
IMG_DIR = images

TARGETS = frontend dispatcher worker
BINARIES = $(addprefix $(BIN_DIR)/,$(TARGETS))

.PHONY: all release debug run run_rlwrap clean

all: $(BINARIES)

release: CFLAGS += -O2
release: all

debug: CFLAGS += -g -O0 -DDEBUG
debug: all

sleep: CFLAGS += -DSLEEP
sleep: debug

$(BIN_DIR)/frontend: $(OBJ_DIR)/frontend.o $(OBJ_DIR)/util.o $(OBJ_DIR)/pipe_utils.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/dispatcher: $(OBJ_DIR)/dispatcher.o $(OBJ_DIR)/util.o $(OBJ_DIR)/pipe_utils.o $(OBJ_DIR)/stack.o| $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/worker: $(OBJ_DIR)/worker.o $(OBJ_DIR)/util.o $(OBJ_DIR)/pipe_utils.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

$(OBJ_DIR)/frontend.o $(OBJ_DIR)/dispatcher.o $(OBJ_DIR)/worker.o $(OBJ_DIR)/util.o: $(INC_DIR)/util.h
$(OBJ_DIR)/frontend.o $(OBJ_DIR)/dispatcher.o $(OBJ_DIR)/worker.o $(OBJ_DIR)/pipe_utils.o: $(INC_DIR)/pipe_utils.h
$(OBJ_DIR)/dispatcher.o $(OBJ_DIR)/stack.o: $(INC_DIR)/stack.h

IN  ?= nature.ppm
OUT ?= output.ppm

run: all
	./$(BIN_DIR)/frontend $(IMG_DIR)/$(IN) $(IMG_DIR)/$(OUT) 2>logs.txt

run_rlwrap: all
	rlwrap ./$(BIN_DIR)/frontend $(IMG_DIR)/$(IN) $(IMG_DIR)/$(OUT) 2>logs.txt

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) logs.txt
