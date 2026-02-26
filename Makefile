# --- Targets ---
CLIENT_TARGET = main.js
SERVER_TARGET = doom_server

# --- Compilers ---
EMCC = emcc
CC = clang

# --- Submodule Paths ---
# Adjust these if your submodules are in a different folder!
CJSON_DIR = cJSON
LDC_DIR = libdatachannel
LDC_BUILD_DIR = $(LDC_DIR)/build

PREJS = src/client/pre.js

# --- Compiler Flags ---
EMCC_FLAGS = -gsource-map -O0 -Isrc -s USE_SDL=2 --pre-js $(PREJS) --preload-file doom.wad

# Server needs the include paths (-I) for both submodules
CFLAGS = -g -O0 -Wall -Isrc -I$(CJSON_DIR) -I$(LDC_DIR)/include

# Server Linker Flags: Point to the libdatachannel build folder
SERVER_LDFLAGS = -L$(LDC_BUILD_DIR) -Wl,-rpath,$(LDC_BUILD_DIR) -ldatachannel -pthread -lm

# --- Source Files ---
SHARED_SOURCES = $(wildcard src/*.c)
CLIENT_SOURCES = $(wildcard src/client/*.c)
SERVER_SOURCES = $(wildcard src/server/*.c)

CLIENT_ALL_SOURCES = $(SHARED_SOURCES) $(CLIENT_SOURCES)

# Notice we inject cJSON.c directly into the server's build sources!
SERVER_ALL_SOURCES = $(SHARED_SOURCES) $(SERVER_SOURCES) $(CJSON_DIR)/cJSON.c

# --- Rules ---
all: client server

client: $(CLIENT_TARGET)

# Make sure libdatachannel is built BEFORE the server compiles
server: libdatachannel $(SERVER_TARGET)

$(CLIENT_TARGET): $(CLIENT_ALL_SOURCES) $(PREJS)
	@echo "Compiling client to WebAssembly..."
	$(EMCC) $(EMCC_FLAGS) $(CLIENT_ALL_SOURCES) -o $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_ALL_SOURCES)
	@echo "Compiling server natively..."
	$(CC) $(CFLAGS) $(SERVER_ALL_SOURCES) $(SERVER_LDFLAGS) -o $(SERVER_TARGET)

# --- Submodule Build Rules ---
# This rule creates the CMake build environment for libdatachannel
$(LDC_BUILD_DIR)/Makefile:
	@echo "Configuring libdatachannel via CMake..."
	mkdir -p $(LDC_BUILD_DIR)
	cd $(LDC_BUILD_DIR) && cmake ..

# This actually builds libdatachannel
libdatachannel: $(LDC_BUILD_DIR)/Makefile
	@echo "Building libdatachannel..."
	$(MAKE) -C $(LDC_BUILD_DIR)

clean:
	@echo "Cleaning build files..."
	@rm -f $(CLIENT_TARGET) main.wasm $(SERVER_TARGET)
	# Optional: Clean libdatachannel build too
	# @rm -rf $(LDC_BUILD_DIR)

.PHONY: all client server clean libdatachannel