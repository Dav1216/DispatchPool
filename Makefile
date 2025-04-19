# === Compiler and flags ===
CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -g -Iinclude
# -Wall warnings
# -Wextra extra warnings
# -pedantic strict ISO c++
# -G DEBUG INFO
# -Iinclude adds include directoy to the header search path

# === Project structure ===
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:.cpp=.o)
BIN := bin/main

# === Build target ===
all: $(BIN)
# : dependes on second part
$(BIN): $(OBJ) 
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $^

# === Compile each .cpp to .o ===
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# === Clean build artifacts ===
clean:
	rm -rf bin
	rm -f src/*.o

# === Run the program ===
run: all
	./$(BIN)

# === PHONY targets (not real files) ===
.PHONY: all clean run
