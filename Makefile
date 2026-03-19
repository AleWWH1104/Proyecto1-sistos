# ============================================================================
# Cross-platform Makefile for Proyecto1-sistos
# Supports: Linux, macOS, Windows (MinGW/MSYS2 with GNU Make)
# ============================================================================

CXX       = g++
CXXFLAGS  = -std=c++17 -Wall -Wextra
PROTOC    = protoc
PROTO_DIR = protos
GEN_DIR   = gen

# ---------------------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
    PLATFORM := windows
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        PLATFORM := macos
    else
        PLATFORM := linux
    endif
endif

# ---------------------------------------------------------------------------
# Platform-specific variables
# ---------------------------------------------------------------------------
ifeq ($(PLATFORM),windows)
    EXE_EXT    = .exe
    CXXFLAGS  += -D_WIN32_WINNT=0x0600
    LDFLAGS    = -lws2_32
    LDFLAGS   += $(shell pkg-config --libs protobuf 2>/dev/null || echo -lprotobuf)
    MKDIR      = mkdir
    RM_GEN     = if exist $(GEN_DIR) rmdir /S /Q $(GEN_DIR)
    RM_SERVER  = if exist server$(EXE_EXT) del /Q server$(EXE_EXT)
    RM_CLIENT  = if exist client$(EXE_EXT) del /Q client$(EXE_EXT)
    PROTO_SRCS = $(wildcard $(PROTO_DIR)/*.proto) \
                 $(wildcard $(PROTO_DIR)/cliente-side/*.proto) \
                 $(wildcard $(PROTO_DIR)/server-side/*.proto)
else ifeq ($(PLATFORM),macos)
    EXE_EXT    =
    CXXFLAGS  += -pthread $(shell pkg-config --cflags protobuf)
    LDFLAGS    = $(shell pkg-config --libs protobuf) -lpthread
    MKDIR      = mkdir -p
    RM_GEN     = rm -rf $(GEN_DIR)
    RM_SERVER  = rm -f server
    RM_CLIENT  = rm -f client
    PROTO_SRCS = $(shell find $(PROTO_DIR) -name '*.proto')
else
    EXE_EXT    =
    CXXFLAGS  += -pthread
    LDFLAGS    = $(shell pkg-config --libs protobuf) -lpthread
    MKDIR      = mkdir -p
    RM_GEN     = rm -rf $(GEN_DIR)
    RM_SERVER  = rm -f server
    RM_CLIENT  = rm -f client
    PROTO_SRCS = $(shell find $(PROTO_DIR) -name '*.proto')
endif

# ---------------------------------------------------------------------------
# Output binaries
# ---------------------------------------------------------------------------
SERVER_BIN = server$(EXE_EXT)
CLIENT_BIN = client$(EXE_EXT)

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
SERVER_SRCS = src/server/server.cpp \
              src/server/user_registry.cpp \
              src/server/session.cpp \
              src/common/net_utils.cpp

CLIENT_SRCS = src/client/client.cpp \
              src/client/input_handler.cpp \
              src/client/receiver.cpp \
              src/common/net_utils.cpp

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.PHONY: all proto server client clean

all: server client

proto:
	@$(MKDIR) $(GEN_DIR)
	$(PROTOC) -I$(PROTO_DIR)/cliente-side -I$(PROTO_DIR)/server-side -I$(PROTO_DIR) \
	    --cpp_out=$(GEN_DIR) $(PROTO_SRCS)

server: proto
	$(CXX) $(CXXFLAGS) -I. -I$(GEN_DIR) -Isrc \
	    $(SERVER_SRCS) \
	    $(wildcard $(GEN_DIR)/*.pb.cc) \
	    -o $(SERVER_BIN) $(LDFLAGS)

client: proto
	$(CXX) $(CXXFLAGS) -I. -I$(GEN_DIR) -Isrc \
	    $(CLIENT_SRCS) \
	    $(wildcard $(GEN_DIR)/*.pb.cc) \
	    -o $(CLIENT_BIN) $(LDFLAGS)

clean:
	$(RM_GEN)
	$(RM_SERVER)
	$(RM_CLIENT)
