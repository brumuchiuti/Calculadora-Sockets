CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
INC_DIR = include
SRC_DIR = src

BIN_SERVER = server
BIN_CLIENT = client

.PHONY: all clean server client

all: server client

server: $(SRC_DIR)/server.c $(INC_DIR)/proto.h
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $(BIN_SERVER) $(SRC_DIR)/server.c

client: $(SRC_DIR)/client.c $(INC_DIR)/proto.h
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $(BIN_CLIENT) $(SRC_DIR)/client.c

clean:
	rm -f $(BIN_SERVER) $(BIN_CLIENT)
