#include "net_utils.h"

#include "common/platform.h"
#include <cstring>

// Helper: read exactly `n` bytes from sockfd into buf.
// Returns true on success, false if connection closed or error.
static bool recv_exact(int sockfd, void *buf, size_t n) {
  size_t received = 0;
  uint8_t *ptr = static_cast<uint8_t *>(buf);
  while (received < n) {
    ssize_t r = recv(sockfd, ptr + received, n - received, 0);
    if (r <= 0)
      return false; // connection closed or error
    received += r;
  }
  return true;
}

// Helper: send exactly `n` bytes from buf over sockfd.
// Returns true on success, false on error.
static bool send_exact(int sockfd, const void *buf, size_t n) {
  size_t sent = 0;
  const uint8_t *ptr = static_cast<const uint8_t *>(buf);
  while (sent < n) {
    ssize_t s = send(sockfd, ptr + sent, n - sent, MSG_NOSIGNAL);
    if (s <= 0)
      return false;
    sent += s;
  }
  return true;
}

bool send_message(int sockfd, MessageType type, const std::string &payload) {
  // Build 5-byte header
  uint8_t header[5];
  header[0] = static_cast<uint8_t>(type);

  // Payload length in big-endian
  uint32_t len_be = htonl(static_cast<uint32_t>(payload.size()));
  memcpy(header + 1, &len_be, 4);

  // Send header then payload
  if (!send_exact(sockfd, header, 5))
    return false;
  if (!payload.empty()) {
    if (!send_exact(sockfd, payload.data(), payload.size()))
      return false;
  }
  return true;
}

bool recv_message(int sockfd, MessageType &out_type, std::string &out_payload) {
  // Read 5-byte header
  uint8_t header[5];
  if (!recv_exact(sockfd, header, 5))
    return false;

  out_type = static_cast<MessageType>(header[0]);

  uint32_t len_be;
  memcpy(&len_be, header + 1, 4);
  uint32_t length = ntohl(len_be);

  // Read payload
  out_payload.resize(length);
  if (length > 0) {
    if (!recv_exact(sockfd, &out_payload[0], length))
      return false;
  }
  return true;
}
