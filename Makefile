# Makefile du projet svg_cli
#   make build : compile le projet
#   make clean : supprime les fichiers générés
#   make test  : lance les scénarios de test (commandes puis menu)

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -O2
LDLIBS  = -lm

SRC_DIR = src
OBJ_DIR = obj
BIN     = svg_cli

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: build clean test

build: $(BIN)

# édition des liens
$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# compilation de chaque .c en .o (-MMD génère les dépendances vers les .h)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

test: build
	./$(BIN) < tests/scenario.txt
	./$(BIN) < tests/menu.txt

clean:
	rm -rf $(OBJ_DIR) $(BIN) test_sortie.dessin test_sortie.svg test_menu.dessin test_menu.svg apercu.html

-include $(OBJ:.o=.d)
