# --- Targets ---
CLIENT_TARGET = main.js
SERVER_TARGET = dserver

# --- Compilers ---
EMCC = emcc
CC = clang

# --- Compiler Flags ---
# Client flags (WebAssembly/SDL)
EMCC_FLAGS = -g4 -O0 -Isrc -s USE_SDL=2 --pre-js src/client/pre.js --preload-file doom.wad

# Server flags (Native)
CFLAGS = -g -O0 -Wall

# --- Source Files ---
# Shared sources in the root src/ directory (e.g., wad.c, player.c, etc.)
SHARED_SOURCES = $(wildcard src/*.c)

# Environment-specific sources
CLIENT_SOURCES = $(wildcard src/client/*.c)
SERVER_SOURCES = $(wildcard src/server/*.c)

# Combined sources for each build
CLIENT_ALL_SOURCES = $(SHARED_SOURCES) $(CLIENT_SOURCES)
SERVER_ALL_SOURCES = $(SHARED_SOURCES) $(SERVER_SOURCES)

# --- Rules ---
all: client server

client: $(CLIENT_TARGET)

server: $(SERVER_TARGET)

# Build WebAssembly Client
$(CLIENT_TARGET): $(CLIENT_ALL_SOURCES) src/client/pre.js
	@echo "Compiling client to WebAssembly..."
	$(EMCC) $(EMCC_FLAGS) $(CLIENT_ALL_SOURCES) -o $(CLIENT_TARGET)

# Build Native Server
$(SERVER_TARGET): $(SERVER_ALL_SOURCES)
	@echo "Compiling server natively..."
	$(CC) $(CFLAGS) $(SERVER_ALL_SOURCES) -o $(SERVER_TARGET)

clean:
	@echo "Cleaning build files..."
	@rm -f $(CLIENT_TARGET) main.wasm $(SERVER_TARGET)

.PHONY: all client server clean