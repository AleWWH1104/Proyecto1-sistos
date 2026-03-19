#pragma once

#include <cstdint>
#include <string>

// Message type codes defined by the protocol
enum MessageType : uint8_t {
  // Client -> Server
  MSG_REGISTER = 1,
  MSG_GENERAL = 2,
  MSG_DM = 3,
  MSG_CHANGE_STATUS = 4,
  MSG_LIST_USERS = 5,
  MSG_GET_USER_INFO = 6,
  MSG_QUIT = 7,

  // Server -> Client
  MSG_SERVER_RESPONSE = 10,
  MSG_ALL_USERS = 11,
  MSG_FOR_DM = 12,
  MSG_BROADCAST_DELIVERY = 13,
  MSG_GET_USER_INFO_RESP = 14,
};

// Sends a framed message over a socket.
// Header: [1 byte type][4 bytes length big-endian][N bytes payload]
// Returns true on success, false on error.
bool send_message(int sockfd, MessageType type, const std::string &payload);

// Reads a framed message from a socket.
// Fills out_type and out_payload. Blocks until full message is received.
// Returns true on success, false if connection closed or error.
bool recv_message(int sockfd, MessageType &out_type, std::string &out_payload);
