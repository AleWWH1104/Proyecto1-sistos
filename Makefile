CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread
PROTO_FLAGS := $(shell pkg-config --cflags --libs protobuf)

# ── Directories ──────────────────────────────────────────────────────────────
SRC_CLIENT  := src/client
SRC_SERVER  := src/server
SRC_COMMON  := src/common
PROTO_DIR   := protos
BUILD_DIR   := build

# ── Proto sources ─────────────────────────────────────────────────────────────
PROTO_CLIENT_SRC := $(wildcard $(PROTO_DIR)/cliente-side/*.proto)
PROTO_SERVER_SRC := $(wildcard $(PROTO_DIR)/server-side/*.proto)
PROTO_COMMON_SRC := $(PROTO_DIR)/common.proto

# Generated .pb.cc files (placed next to .proto files)
PROTO_CLIENT_CC  := $(PROTO_CLIENT_SRC:.proto=.pb.cc)
PROTO_SERVER_CC  := $(PROTO_SERVER_SRC:.proto=.pb.cc)
PROTO_COMMON_CC  := $(PROTO_COMMON_SRC:.proto=.pb.cc)

# ── Object files ─────────────────────────────────────────────────────────────
COMMON_OBJ := $(BUILD_DIR)/net_utils.o

CLIENT_OBJ := \
	$(BUILD_DIR)/client_main.o      \
	$(BUILD_DIR)/input_handler.o    \
	$(BUILD_DIR)/receiver.o

SERVER_OBJ := \
	$(BUILD_DIR)/server_main.o      \
	$(BUILD_DIR)/session.o          \
	$(BUILD_DIR)/user_registry.o

PROTO_CLIENT_OBJ := $(patsubst $(PROTO_DIR)/%.pb.cc,$(BUILD_DIR)/proto_%.o,\
	$(PROTO_CLIENT_CC) $(PROTO_COMMON_CC))

PROTO_SERVER_OBJ := $(patsubst $(PROTO_DIR)/%.pb.cc,$(BUILD_DIR)/proto_%.o,\
	$(PROTO_SERVER_CC) $(PROTO_COMMON_CC))

# ── Targets ───────────────────────────────────────────────────────────────────
.PHONY: all clean protos

all: protos client server

# ── Generate protobuf sources ─────────────────────────────────────────────────
protos:
	protoc -I $(PROTO_DIR) --cpp_out=$(PROTO_DIR) \
		$(PROTO_COMMON_SRC) $(PROTO_CLIENT_SRC) $(PROTO_SERVER_SRC)
	@echo "Protos generados."

# ── Build directory ───────────────────────────────────────────────────────────
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ── Compile common ────────────────────────────────────────────────────────────
$(BUILD_DIR)/net_utils.o: $(SRC_COMMON)/net_utils.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PROTO_FLAGS) -I$(PROTO_DIR) -c $< -o $@

# ── Compile client sources ────────────────────────────────────────────────────
$(BUILD_DIR)/client_main.o: $(SRC_CLIENT)/client.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PROTO_FLAGS) -I$(PROTO_DIR) -I$(PROTO_DIR)/cliente-side -I$(PROTO_DIR)/server-side -c $< -o $@

$(BUILD_DIR)/input_handler.o: $(SRC_CLIENT)/input_handler.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PROTO_FLAGS) -I$(PROTO_DIR) -I$(PROTO_DIR)/cliente-side -c $< -o $@

$(BUILD_DIR)/receiver.o: $(SRC_CLIENT)/receiver.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PROTO_FLAGS) -I$(PROTO_DIR) -I$(PROTO_DIR)/server-side -c $< -o $@

# ── Compile server sources ────────────────────────────────────────────────────
$(BUILD_DIR)/server_main.o: $(SRC_SERVER)/server.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PROTO_FLAGS) -I$(PROTO_DIR) -I$(PROTO_DIR)/cliente-side -I$(PROTO_DIR)/server-side -c $< -o $@

$(BUILD_DIR)/session.o: $(SRC_SERVER)/session.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PROTO_FLAGS) -I$(PROTO_DIR) -I$(PROTO_DIR)/cliente-side -I$(PROTO_DIR)/server-side -c $< -o $@

$(BUILD_DIR)/user_registry.o: $(SRC_SERVER)/user_registry.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ── Compile proto stubs ───────────────────────────────────────────────────────
$(BUILD_DIR)/proto_cliente-side/%.o: $(PROTO_DIR)/cliente-side/%.pb.cc | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/proto_cliente-side
	$(CXX) $(CXXFLAGS) $(PROTO_FLAGS) -I$(PROTO_DIR) -c $< -o $@

$(BUILD_DIR)/proto_server-side/%.o: $(PROTO_DIR)/server-side/%.pb.cc | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/proto_server-side
	$(CXX) $(CXXFLAGS) $(PROTO_FLAGS) -I$(PROTO_DIR) -c $< -o $@

$(BUILD_DIR)/proto_common.o: $(PROTO_DIR)/common.pb.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PROTO_FLAGS) -I$(PROTO_DIR) -c $< -o $@

# ── Link client ───────────────────────────────────────────────────────────────
client: $(CLIENT_OBJ) $(COMMON_OBJ) \
        $(patsubst $(PROTO_DIR)/cliente-side/%.pb.cc,$(BUILD_DIR)/proto_cliente-side/%.o,$(PROTO_CLIENT_CC)) \
        $(patsubst $(PROTO_DIR)/server-side/%.pb.cc,$(BUILD_DIR)/proto_server-side/%.o,$(PROTO_SERVER_CC)) \
        $(BUILD_DIR)/proto_common.o
	$(CXX) $(CXXFLAGS) $^ $(PROTO_FLAGS) -o $@
	@echo "Cliente compilado -> ./client"

# ── Link server ───────────────────────────────────────────────────────────────
server: $(SERVER_OBJ) $(COMMON_OBJ) \
        $(patsubst $(PROTO_DIR)/cliente-side/%.pb.cc,$(BUILD_DIR)/proto_cliente-side/%.o,$(PROTO_CLIENT_CC)) \
        $(patsubst $(PROTO_DIR)/server-side/%.pb.cc,$(BUILD_DIR)/proto_server-side/%.o,$(PROTO_SERVER_CC)) \
        $(BUILD_DIR)/proto_common.o
	$(CXX) $(CXXFLAGS) $^ $(PROTO_FLAGS) -o $@
	@echo "Servidor compilado -> ./server"

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR) client server
	@echo "Limpieza completa."