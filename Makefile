CXX       = g++
CXXFLAGS  = -std=c++17 -Wall -Wextra -pthread
PROTOC    = protoc
PROTO_DIR = protos
GEN_DIR   = gen
LDFLAGS   = $(shell pkg-config --libs protobuf) -lpthread

# Find all .proto files recursively
PROTO_SRCS = $(shell find $(PROTO_DIR) -name '*.proto')

.PHONY: all proto server client clean

all: server client

proto:
	@mkdir -p $(GEN_DIR)
	$(PROTOC) -I$(PROTO_DIR)/cliente-side -I$(PROTO_DIR)/server-side -I$(PROTO_DIR) \
	    --cpp_out=$(GEN_DIR) $(PROTO_SRCS)

server: proto
	$(CXX) $(CXXFLAGS) -I. -I$(GEN_DIR) -Isrc \
	    src/server/server.cpp \
	    src/server/user_registry.cpp \
	    src/server/session.cpp \
	    src/common/net_utils.cpp \
	    $(wildcard $(GEN_DIR)/*.pb.cc) \
	    -o server $(LDFLAGS)

client: proto
	$(CXX) $(CXXFLAGS) -I. -I$(GEN_DIR) -Isrc \
	    src/client/client.cpp \
	    src/client/input_handler.cpp \
	    src/client/receiver.cpp \
	    src/common/net_utils.cpp \
	    $(wildcard $(GEN_DIR)/*.pb.cc) \
	    -o client $(LDFLAGS)

clean:
	rm -rf $(GEN_DIR) server client
