#include <utility>

#include "zioop/network/Connection.hpp"

#include <iostream>

namespace network {
    Connection::Connection(
        const SOCKET socket,
        std::string ip,
        const std::uint16_t port,
        FrameHandler frameHandler,
        DisconnectHandler disconnectHandler
    ) : m_socket(socket),
        m_ip(std::move(ip)),
        m_port(port),
        m_frameHandler(std::move(frameHandler)),
        m_disconnectHandler(std::move(disconnectHandler)) {
    }

    void Connection::start() {
        m_running = true;
        m_thread = std::thread([this] {
            run();
        });
    }

    void Connection::send(
        const std::vector<std::byte> &data
    ) const {
        std::size_t totalSent = 0;

        while (totalSent < data.size()) {
            const int sent = ::send(
                m_socket,
                reinterpret_cast<const char *>(data.data() + totalSent),
                static_cast<int>(data.size() - totalSent),
                0
            );

            if (sent == SOCKET_ERROR) {
                std::cerr << "send() failed: " << WSAGetLastError() << std::endl;
                break;
            }

            if (sent == 0) {
                break;
            }

            totalSent += static_cast<std::size_t>(sent);
        }
    }

    void Connection::run() {
        std::byte buffer[1024];

        while (m_running) {
            if (m_socket == INVALID_SOCKET) {
                break;
            }

            const int received = recv(
                m_socket,
                reinterpret_cast<char *>(buffer),
                sizeof(buffer),
                0
            );

            if (received > 0) {
                m_receiveBuffer.insert(
                    m_receiveBuffer.end(),
                    buffer,
                    buffer + received
                );

                while (const auto frame = Protocol::decode(m_receiveBuffer)) {
                    m_frameHandler(*this, *frame);

                    const auto consumed = static_cast<std::ptrdiff_t>(
                        Protocol::HeaderSize + frame->payload.size()
                    );
                    m_receiveBuffer.erase(
                        m_receiveBuffer.begin(),
                        m_receiveBuffer.begin() + consumed
                    );
                    std::cout << frame->payload << '\n';
                }
            } else if (received == 0) {
                std::cout
                        << "Client disconnected: "
                        << m_ip
                        << '\n';
                m_running = false;
                break;
            } else {
                std::cerr
                        << "recv() failed: "
                        << WSAGetLastError()
                        << '\n';
                m_running = false;
                break;
            }
        }

        m_running = false;
        m_disconnectHandler(*this);
    }

    void Connection::requestStop() {
        m_running = false;

        if (m_socket != INVALID_SOCKET) {
            shutdown(m_socket, SD_BOTH);
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

    void Connection::join() {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    Connection::~Connection() {
        requestStop();
        join();
    }
}
