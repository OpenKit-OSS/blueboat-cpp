#include "blueboat/common/http.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace blueboat::http {

namespace {
constexpr std::size_t MAX_HEADER_SIZE = 64 * 1024;
constexpr std::size_t MAX_BODY_SIZE = 8 * 1024 * 1024;

std::optional<std::string> read_until_double_crlf(int fd) {
  std::string buf;
  char c;
  while (buf.size() < MAX_HEADER_SIZE) {
    ssize_t n = ::recv(fd, &c, 1, 0);
    if (n <= 0) {
      return std::nullopt;
    }
    buf.push_back(c);
    if (buf.size() >= 4 && buf.compare(buf.size() - 4, 4, "\r\n\r\n") == 0) {
      return buf;
    }
  }
  return std::nullopt;
}

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

std::string trim(const std::string &s) {
  auto start = s.find_first_not_of(" \t");
  auto end = s.find_last_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  return s.substr(start, end - start + 1);
}
} // namespace

std::optional<Request> read_request(int fd) {
  auto raw = read_until_double_crlf(fd);
  if (!raw) {
    return std::nullopt;
  }

  std::istringstream stream(*raw);
  std::string request_line;
  if (!std::getline(stream, request_line)) {
    return std::nullopt;
  }
  if (!request_line.empty() && request_line.back() == '\r') {
    request_line.pop_back();
  }

  std::istringstream request_line_stream(request_line);
  Request req;
  std::string target;
  std::string version;
  if (!(request_line_stream >> req.method >> target >> version)) {
    return std::nullopt;
  }

  auto query_pos = target.find('?');
  if (query_pos != std::string::npos) {
    req.path = target.substr(0, query_pos);
    req.query = target.substr(query_pos + 1);
  } else {
    req.path = target;
  }

  std::string header_line;
  while (std::getline(stream, header_line)) {
    if (!header_line.empty() && header_line.back() == '\r') {
      header_line.pop_back();
    }
    if (header_line.empty()) {
      continue;
    }
    auto colon = header_line.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    req.headers[to_lower(header_line.substr(0, colon))] = trim(header_line.substr(colon + 1));
  }

  auto it = req.headers.find("content-length");
  if (it != req.headers.end()) {
    std::size_t content_length = 0;
    try {
      content_length = static_cast<std::size_t>(std::stoul(it->second));
    } catch (const std::exception &) {
      return std::nullopt;
    }
    if (content_length > MAX_BODY_SIZE) {
      return std::nullopt;
    }
    req.body.resize(content_length);
    std::size_t received = 0;
    while (received < content_length) {
      ssize_t n = ::recv(fd, req.body.data() + received, content_length - received, 0);
      if (n <= 0) {
        return std::nullopt;
      }
      received += static_cast<std::size_t>(n);
    }
  }

  return req;
}

std::map<std::string, std::string> parse_query(const std::string &query) {
  std::map<std::string, std::string> result;
  std::istringstream stream(query);
  std::string pair;
  while (std::getline(stream, pair, '&')) {
    auto eq = pair.find('=');
    if (eq == std::string::npos) {
      result[pair] = "";
    } else {
      result[pair.substr(0, eq)] = pair.substr(eq + 1);
    }
  }
  return result;
}

void write_response(
  int fd, int status, const std::string &status_text, const std::string &content_type, const std::string &body, const std::map<std::string, std::string> &extra_headers
) {
  std::ostringstream out;
  out << "HTTP/1.1 " << status << " " << status_text << "\r\n";
  out << "Content-Type: " << content_type << "\r\n";
  out << "Content-Length: " << body.size() << "\r\n";
  out << "Connection: close\r\n";
  for (auto &[key, value] : extra_headers) {
    out << key << ": " << value << "\r\n";
  }
  out << "\r\n" << body;

  std::string data = out.str();
  std::size_t sent = 0;
  while (sent < data.size()) {
    ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
    if (n <= 0) {
      return;
    }
    sent += static_cast<std::size_t>(n);
  }
}

} // namespace blueboat::http
