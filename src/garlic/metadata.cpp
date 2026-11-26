// Garlic Save Companion - read-only game artwork from installed title metadata.
// Copyright (C) 2026 SvenGDK
// SPDX-License-Identifier: GPL-3.0-or-later

#include "garlic/metadata.hpp"

#include <algorithm>
#include <cstdio>

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

#if !defined(__linux__)

constexpr std::size_t kMaxImageBytes = 16u << 20;

struct Socket
{
    int fd = -1;
    ~Socket()
    {
        if (fd >= 0)
            close(fd);
    }
};

bool connect_socket(int port, Socket *out, std::string *error)
{
    out->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (out->fd < 0)
    {
        *error = "Could not create the metadata socket";
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    address.sin_addr.s_addr = htonl(0x7F000001);
    if (connect(out->fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
    {
        *error = "Could not connect to the metadata service";
        return false;
    }
    timeval timeout{};
    timeout.tv_sec = 3;
    (void)setsockopt(out->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(out->fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    return true;
}

bool send_text(int fd, const std::string &text, std::string *error)
{
    std::size_t sent = 0;
    while (sent < text.size())
    {
        const ssize_t count = send(fd, text.data() + sent, text.size() - sent, MSG_NOSIGNAL);
        if (count <= 0)
        {
            *error = "The metadata connection closed during a command";
            return false;
        }
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

int read_reply(int fd, std::string *error, std::string *last_line = nullptr)
{
    std::string first;
    for (int lines = 0; lines < 32; ++lines)
    {
        std::string line;
        for (;;)
        {
            char c = 0;
            const ssize_t count = recv(fd, &c, 1, 0);
            if (count <= 0)
            {
                *error = "The metadata service stopped responding";
                return -1;
            }
            if (c == '\n')
                break;
            if (c != '\r')
                line.push_back(c);
            if (line.size() > 512)
            {
                *error = "The metadata response is too long";
                return -1;
            }
        }
        if (line.size() < 4 || line[0] < '0' || line[0] > '9' || line[1] < '0' || line[1] > '9' ||
            line[2] < '0' || line[2] > '9')
        {
            *error = "Invalid metadata service response";
            return -1;
        }
        if (first.empty())
            first = line;
        if (last_line != nullptr)
            *last_line = line;
        if (line[3] == ' ')
            return std::atoi(line.c_str());
        if (line[3] != '-')
        {
            *error = "Invalid multiline metadata response";
            return -1;
        }
    }
    *error = "Too many metadata response lines";
    return -1;
}

int command(int fd, const std::string &text, std::string *error, std::string *response = nullptr)
{
    return send_text(fd, text + "\r\n", error) ? read_reply(fd, error, response) : -1;
}

bool receive_image(int fd, std::string *data, std::string *error)
{
    char buffer[16384];
    data->clear();
    for (;;)
    {
        const ssize_t count = recv(fd, buffer, sizeof(buffer), 0);
        if (count == 0)
            return true;
        if (count < 0)
        {
            *error = "The artwork download stopped before completion";
            return false;
        }
        if (static_cast<std::size_t>(count) > kMaxImageBytes - data->size())
        {
            *error = "The title artwork is too large";
            return false;
        }
        data->append(buffer, static_cast<std::size_t>(count));
    }
}

bool download_png(const std::string &path, Image *out, std::string *error)
{
    if (path.empty())
    {
        *error = "Invalid title ID for metadata artwork";
        return false;
    }
    Socket control;
    if (!connect_socket(2121, &control, error))
        return false;
    if (read_reply(control.fd, error) != 220 ||
        command(control.fd, "USER anonymous", error) != 331 ||
        command(control.fd, "PASS anonymous", error) != 230 ||
        command(control.fd, "TYPE I", error) != 200)
    {
        *error = "The metadata service did not accept the read-only session";
        return false;
    }
    std::string passive;
    if (command(control.fd, "PASV", error, &passive) != 227)
    {
        *error = "The metadata service refused passive download";
        return false;
    }
    int data_port = 0;
    if (!parse_pasv_port(passive, &data_port))
    {
        *error = "The metadata service returned an invalid passive address";
        return false;
    }
    Socket data;
    if (!connect_socket(data_port, &data, error))
        return false;
    const int transfer = command(control.fd, "RETR " + path, error);
    if (transfer != 150 && transfer != 125)
    {
        *error = "No title artwork is available in the installed metadata";
        return false;
    }
    std::string bytes;
    if (!receive_image(data.fd, &bytes, error) || read_reply(control.fd, error) != 226)
        return false;
    (void)command(control.fd, "QUIT", error);
    return decode_png(bytes, out, error);
}

#endif

} // namespace

bool parse_pasv_port(std::string_view response, int *port)
{
    if (port == nullptr || response.substr(0, 4) != "227 ")
        return false;
    while (!response.empty() && (response.back() == ' ' || response.back() == '\t' ||
                                 response.back() == '\r' || response.back() == '\n'))
        response.remove_suffix(1);
    if (!response.empty() && response.back() == '.')
        response.remove_suffix(1);
    const std::size_t open = response.find('(');
    const std::size_t close = response.find(')', open);
    if (open == std::string_view::npos || close == std::string_view::npos ||
        close + 1 != response.size())
        return false;
    unsigned a = 0;
    unsigned b = 0;
    unsigned c = 0;
    unsigned d = 0;
    unsigned high = 0;
    unsigned low = 0;
    char extra = 0;
    const std::string values(response.substr(open + 1, close - open - 1));
    if (std::sscanf(values.c_str(), "%u,%u,%u,%u,%u,%u%c", &a, &b, &c, &d, &high, &low, &extra) !=
            6 ||
        a > 255 || b > 255 || c > 255 || d > 255 || high > 255 || low > 255)
        return false;
    const unsigned combined = high * 256 + low;
    if (combined == 0 || combined > 65535)
        return false;
    *port = static_cast<int>(combined);
    return true;
}

namespace
{

std::string metadata_path(std::string_view title_id, std::string_view filename)
{
    if (title_id.size() != 9)
        return {};
    const bool valid = std::all_of(title_id.begin(), title_id.end(), [](unsigned char c)
                                   { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); });
    return valid ? "/user/appmeta/" + std::string(title_id) + "/" + std::string(filename)
                 : std::string();
}

} // namespace

std::string title_icon_path(std::string_view title_id)
{
    return metadata_path(title_id, "icon0.png");
}

std::string title_photo_path(std::string_view title_id)
{
    return metadata_path(title_id, "pic0.png");
}

bool fetch_title_icon(std::string_view title_id, Image *out, std::string *error)
{
    if (out == nullptr)
        return false;
#if defined(__linux__)
    (void)title_id;
    *error = "Metadata download is unavailable in host previews";
    return false;
#else
    *out = {};
    if (download_png(title_photo_path(title_id), out, error))
        return true;
    return download_png(title_icon_path(title_id), out, error);
#endif
}

} // namespace garlic
