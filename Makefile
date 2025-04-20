# === Compiler and flags ===
CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -g -Iinclude

# === Project structure ===
MAIN_SRC := $(filter-out src/worker_process.cpp, $(wildcard src/*.cpp))
MAIN_OBJ := $(MAIN_SRC:.cpp=.o)
WORKER_SRC := src/worker_process.cpp
WORKER_OBJ := $(WORKER_SRC:.cpp=.o)

MAIN_BIN := bin/main
WORKER_BIN := bin/worker

# === Build targets ===
all: $(MAIN_BIN) $(WORKER_BIN)

$(MAIN_BIN): $(MAIN_OBJ)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $^

$(WORKER_BIN): $(WORKER_OBJ)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $^

# === Compile each .cpp to .o ===
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# === Clean build artifacts ===
clean:
	rm -rf bin
	rm -f src/*.o

# === Run the main program ===
run: $(MAIN_BIN)
	./$(MAIN_BIN)

.PHONY: all clean run
