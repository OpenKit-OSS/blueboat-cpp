#pragma once

#include <map>
#include <optional>
#include <string>

#include "blueboat/common/socket.hpp"

namespace blueboat::http {

struct Request {
  std::string method;
  std::string path;
  std::string query;
  std::map<std::string, std::string> headers;
  std::string body;
};

std::optional<Request> read_request(Socket &sock);

std::map<std::string, std::string> parse_query(const std::string &query);

void write_response(
  Socket &sock, int status, const std::string &status_text, const std::string &content_type, const std::string &body,
  const std::map<std::string, std::string> &extra_headers = {}
);

} // namespace blueboat::http
