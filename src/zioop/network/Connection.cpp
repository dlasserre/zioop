#include <utility>

#include "zioop/network/Connection.hpp"

#include <iostream>

namespace network {
    Connection::Connection(
        const SOCKET socket,
        std::string ip,
        const std::uint16_t port,
        FrameHandler frameHandler,
        DisconnectHandler disconnectHandler,
        const std::optional<std::size_t> clientId
    ) : m_clientId(clientId),
        m_socket(socket),
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
        // A tick notification and a command response must never interleave frames.
        std::lock_guard<std::mutex> lock(m_sendMutex);
        if (!m_running) {
            return;
        }
        std::size_t totalSent = 0;

        while (totalSent < data.size()) {
            const SOCKET socket = m_socket.load();
            if (socket == INVALID_SOCKET) {
                break;
            }
            const int sent = ::send(
                socket,
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
            const SOCKET socket = m_socket.load();
            if (socket == INVALID_SOCKET) {
                break;
            }

            const int received = recv(
                socket,
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

                while (m_running) {
                    const auto frame = Protocol::decode(m_receiveBuffer);
                    if (!frame) {
                        break;
                    }
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

        const SOCKET socket = m_socket.exchange(INVALID_SOCKET);
        if (socket != INVALID_SOCKET) {
            shutdown(socket, SD_BOTH);
            closesocket(socket);
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
