CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = 

TARGET = buf
INSTALL_DIR = /usr/local/bin

SRC_DIR = src
OBJ_DIR = obj
INC_DIR = include

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

install: $(TARGET)
	sudo install -m 755 $(TARGET) $(INSTALL_DIR)/$(TARGET)
	@echo "buf installed to $(INSTALL_DIR)/$(TARGET)"
	@echo "Test with: sudo buf -h"

uninstall:
	sudo rm -f $(INSTALL_DIR)/$(TARGET)
	@echo "buf uninstalled"