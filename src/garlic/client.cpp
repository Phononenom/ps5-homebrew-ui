// Garlic Save Companion - bounded loopback HTTP and payload transport.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#include "garlic/client.hpp"

#include "garlic/protocol.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>

#if !defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace garlic
{
namespace
{

std::string lower(std::string_view value)
{
    std::string result(value);
    for (char &c : result)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

#if !defined(__linux__)

constexpr std::size_t kMaxResponseBytes = 16u << 20;

int connect_loopback(int port, std::string *error, int timeout_seconds)
{
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
    {
        *error = "Could not create a loopback socket";
        return -1;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    address.sin_addr.s_addr = htonl(0x7F000001);
    if (connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
    {
        close(fd);
        *error = "Could not connect to 127.0.0.1:" + std::to_string(port);
        return -1;
    }
    timeval timeout{};
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    return fd;
}

bool send_all(int fd, const std::uint8_t *data, std::size_t size, std::string *error)
{
    std::size_t sent = 0;
    while (sent < size)
    {
        const ssize_t result = send(fd, data + sent, size - sent, MSG_NOSIGNAL);
        if (result <= 0)
        {
            *error = "The loopback connection closed during send";
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

HttpResponse get_on_console(std::string_view path, int timeout_seconds)
{
    HttpResponse response;
    if (path.substr(0, 5) != "/api/")
    {
        response.error = "Only Garlic API paths are accepted";
        return response;
    }
    const int fd = connect_loopback(8082, &response.error, timeout_seconds);
    if (fd < 0)
        return response;

    std::string request =
        "GET " + std::string(path) + " HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(request.data());
    if (!send_all(fd, bytes, request.size(), &response.error))
    {
        close(fd);
        return response;
    }

    std::string raw;
    char buffer[16384];
    for (;;)
    {
        const ssize_t count = recv(fd, buffer, sizeof(buffer), 0);
        if (count == 0)
            break;
        if (count < 0)
        {
            response.error = "The Garlic API stopped responding during receive";
            close(fd);
            return response;
        }
        raw.append(buffer, static_cast<std::size_t>(count));
        if (raw.size() > kMaxResponseBytes)
        {
            response.error = "The Garlic API response is too large";
            close(fd);
            return response;
        }
    }
    close(fd);
    return parse_http(raw);
}

#endif

} // namespace

HttpResponse parse_http(std::string_view input)
{
    HttpResponse response;
    const std::size_t line_end = input.find("\r\n");
    if (line_end == std::string_view::npos || input.substr(0, 5) != "HTTP/")
    {
        response.error = "Invalid HTTP status line";
        return response;
    }
    const std::string status_line(input.substr(0, line_end));
    if (std::sscanf(status_line.c_str(), "HTTP/%*s %d", &response.status) != 1)
    {
        response.error = "Invalid HTTP status code";
        return response;
    }
    const std::size_t body_start = input.find("\r\n\r\n");
    if (body_start == std::string_view::npos)
    {
        response.error = "HTTP response has no header terminator";
        return response;
    }
    const std::string_view body = input.substr(body_start + 4);
    std::size_t content_length = body.size();
    bool found_length = false;
    std::size_t position = line_end + 2;
    while (position < body_start)
    {
        const std::size_t next = input.find("\r\n", position);
        const std::size_t end = next == std::string_view::npos ? body_start : next;
        const std::string_view line = input.substr(position, end - position);
        const std::size_t colon = line.find(':');
        if (colon != std::string_view::npos)
        {
            const std::string name = lower(line.substr(0, colon));
            const std::string_view value = line.substr(colon + 1);
            if (name == "content-length")
            {
                if (std::sscanf(value.data(), "%zu", &content_length) != 1)
                {
                    response.error = "Invalid Content-Length";
                    return response;
                }
                found_length = true;
            }
            else if (name == "transfer-encoding" &&
                     lower(value).find("chunked") != std::string_view::npos)
            {
                response.error = "Chunked responses are not supported by this client";
                return response;
            }
        }
        if (next == std::string_view::npos)
            break;
        position = next + 2;
    }
    if (found_length && body.size() != content_length)
    {
        response.error = "Response body length does not match Content-Length";
        return response;
    }
    response.ok = true;
    response.body = body;
    return response;
}

HttpResponse get(std::string_view path, int timeout_seconds)
{
#if defined(__linux__)
    (void)path;
    (void)timeout_seconds;
    return {false, 0, {}, "Network access is unavailable in host previews"};
#else
    return get_on_console(path, timeout_seconds);
#endif
}

bool send_payload(const std::vector<std::uint8_t> &data, std::string *error)
{
    if (!valid_elf(data, error))
        return false;
#if defined(__linux__)
    *error = "Payload launch is unavailable in host previews";
    return false;
#else
    const int fd = connect_loopback(9021, error, 3);
    if (fd < 0)
        return false;
    if (!send_all(fd, data.data(), data.size(), error))
    {
        close(fd);
        return false;
    }
    (void)shutdown(fd, SHUT_WR);
    close(fd);
    return true;
#endif
}

} // namespace garlic
