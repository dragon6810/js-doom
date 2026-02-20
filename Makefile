# Target for WebAssembly build
WASM_TARGET = main.js

# Emscripten compiler
EMCC = emcc

# Compiler flags
# -O3 for optimization
# -s USE_SDL=2 to include SDL2 support
# --pre-js to specify the canvas
EMCC_FLAGS = -O3 -s USE_SDL=2 --pre-js pre.js --preload-file doom.wad

# Source files
SOURCES = main.c wad.c player.c level.c tex.c render.c math.c info.c

all: $(WASM_TARGET)

$(WASM_TARGET): $(SOURCES) pre.js
	@echo "Compiling to WebAssembly..."
	$(EMCC) $(EMCC_FLAGS) $(SOURCES) -o $(WASM_TARGET)

clean:
	@echo "Cleaning build files..."
	@rm -f main.js main.wasm

.PHONY: all clean
