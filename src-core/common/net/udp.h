#pragma once

#include <string.h>
#include <stdexcept>
#include <cstdint>
#include <cerrno>
#include <chrono>
#include <thread>
#include <string>

#if defined(_WIN32)
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace net
{
    class UDPClient
    {
    private:
        sockaddr_in sock_addr{};
#if defined(_WIN32)
        WSADATA wsa;
        SOCKET sock = INVALID_SOCKET;
#else
        int sock = -1;
#endif

        void close_socket()
        {
#if defined(_WIN32)
            if (sock != INVALID_SOCKET)
                closesocket(sock);
            WSACleanup();
#else
            if (sock >= 0)
                close(sock);
#endif
        }

    public:
        UDPClient(const char *address, int port)
        {
            if (port < 1 || port > 65535)
                throw std::invalid_argument("UDP port must be between 1 and 65535");
#if defined(_WIN32)
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
                throw std::runtime_error("Couldn't startup WSA socket!");
#endif

            sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#if defined(_WIN32)
            if (sock == INVALID_SOCKET)
#else
            if (sock < 0)
#endif
            {
                close_socket();
                throw std::runtime_error("Couldn't open UDP socket!");
            }

            memset((char *)&sock_addr, 0, sizeof(sock_addr));
            sock_addr.sin_family = AF_INET;
            sock_addr.sin_port = htons(port);

            if (inet_pton(AF_INET, address, &sock_addr.sin_addr) != 1)
            {
                close_socket();
                throw std::invalid_argument("Invalid UDP IPv4 address");
            }
            int send_buffer = 1024 * 1024;
            setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char *)&send_buffer, sizeof(send_buffer));
#if defined(_WIN32)
            // A player closing its receive port must not poison the sender with
            // asynchronous ICMP port-unreachable errors on the next operation.
            BOOL connreset = FALSE;
            DWORD returned = 0;
            WSAIoctl(sock, SIO_UDP_CONNRESET, &connreset, sizeof(connreset),
                     nullptr, 0, &returned, nullptr, nullptr);
#endif
        }

        UDPClient(const UDPClient &) = delete;
        UDPClient &operator=(const UDPClient &) = delete;

        ~UDPClient()
        {
            close_socket();
        }

        int send(const uint8_t *data, int len)
        {
            int error = 0;
            for (int attempt = 0; attempt < 3; ++attempt)
            {
                int r = sendto(sock, (const char *)data, len, 0, (sockaddr *)&sock_addr, sizeof(sock_addr));
                if (r >= 0)
                    return r;
#if defined(_WIN32)
                error = WSAGetLastError();
                const bool retry = error == WSAEINTR || error == WSAENOBUFS ||
                                   error == WSAEWOULDBLOCK || error == WSAECONNRESET || error == WSAECONNREFUSED;
#else
                error = errno;
                const bool retry = error == EINTR || error == ENOBUFS ||
                                   error == EAGAIN || error == EWOULDBLOCK || error == ECONNREFUSED;
#endif
                if (!retry)
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            throw std::runtime_error("UDP send failed (socket error " + std::to_string(error) + ")");
        }

        int recv(uint8_t *data, int len)
        {
#if defined(_WIN32)
            int slen = sizeof(sockaddr);
#else
            socklen_t slen = sizeof(sockaddr);
#endif
            int r = recvfrom(sock, (char *)data, len, 0, (struct sockaddr *)&sock_addr, &slen);
            if (r == -1)
                throw std::runtime_error("Error receiving from UDP socket!");
            return r;
        }
    };

    class UDPServer
    {
    private:
        sockaddr_in sock_addr;
#if defined(_WIN32)
        WSADATA wsa;
#endif
        int sock = 0;

    public:
        UDPServer(int port)
        {
#if defined(_WIN32)
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
                throw std::runtime_error("Couldn't startup WSA socket!");
#endif

            if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                throw std::runtime_error("Couldn't open UDP socket!");

            memset((char *)&sock_addr, 0, sizeof(sock_addr));
            sock_addr.sin_family = AF_INET;
            sock_addr.sin_addr.s_addr = INADDR_ANY;
            sock_addr.sin_port = htons(port);

#if defined(_WIN32)
            sock_addr.sin_addr.S_un.S_addr = INADDR_ANY;
#endif
            if (bind(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0)
                throw std::runtime_error("Couldn't connect to UDP socket!");
            int ttrue = 1;
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&ttrue, sizeof(int));
        }

        ~UDPServer()
        {
#if defined(_WIN32)
            closesocket(sock);
            WSACleanup();
#else
            shutdown(sock, SHUT_RDWR);
            close(sock);
#endif
        }

        int send(uint8_t *data, int len)
        {
            int slen = sizeof(sockaddr);
            int r = sendto(sock, (char *)data, len, 0, (sockaddr *)&sock_addr, slen);
            if (r == -1)
                throw std::runtime_error("Error sending to UDP socket!");
            return r;
        }

        int recv(uint8_t *data, int len)
        {
#if defined(_WIN32)
            int slen = sizeof(sockaddr);
#else
            socklen_t slen = sizeof(sockaddr);
#endif
            int r = recvfrom(sock, (char *)data, len, 0, (struct sockaddr *)&sock_addr, &slen);
            if (r == -1)
            {
#if defined(_WIN32)
                if(WSAGetLastError() != WSAEINTR)
#endif
                throw std::runtime_error("Error receiving from UDP socket!");
            }
            return r;
        }
    };
}
