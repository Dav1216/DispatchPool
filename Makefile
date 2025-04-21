# === Compiler and flags ===
CXX := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -pedantic -g -Iinclude

# === Project structure ===
BIN_DIR := bin

# Sources
MAIN_SRC := src/main.cpp
DEALER_SRC := src/dealer.cpp
WORKER_SRC := src/worker.cpp
GEN_SRC := src/task_generator.cpp

# Objects
MAIN_OBJ := $(MAIN_SRC:.cpp=.o)
DEALER_OBJ := $(DEALER_SRC:.cpp=.o)
WORKER_OBJ := $(WORKER_SRC:.cpp=.o)
GEN_OBJ := $(GEN_SRC:.cpp=.o)

# Binaries
MAIN_BIN := $(BIN_DIR)/main
DEALER_BIN := $(BIN_DIR)/dealer
WORKER_BIN := $(BIN_DIR)/worker
GEN_BIN := $(BIN_DIR)/task_generator

# === Build targets ===
ALL_BINS := $(MAIN_BIN) $(DEALER_BIN) $(WORKER_BIN) $(GEN_BIN)

all: $(ALL_BINS)

$(BIN_DIR)/%: src/%.o
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# === Compile each .cpp to .o ===
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# === Clean build artifacts ===
clean:
	rm -rf $(BIN_DIR) src/*.o

# === Run the main program ===
run: $(MAIN_BIN)
	./$(MAIN_BIN)

.PHONY: all clean run
