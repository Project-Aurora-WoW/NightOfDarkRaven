#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>
#include "ByteBuffer.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

class WinsockInitializer
{
public:
    WinsockInitializer()
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            throw std::runtime_error("WSAStartup failed");
    }
    ~WinsockInitializer() { WSACleanup(); }
};

// Frame: u16 bodySize, u16 opcode, body (little-endian).
class Session : public std::enable_shared_from_this<Session>
{
public:
    static constexpr size_t kRecvBufferSize = 64 * 1024;
    static constexpr size_t kMaxQueuedBytes = 4 * 1024 * 1024;

    explicit Session(SOCKET sock) : _socket(sock) {}
    virtual ~Session() { Close(); }

    void Start() { _running = true; }

    void Send(uint16_t opcode, const ByteBuffer& body = ByteBuffer())
    {
        const auto& c = body.contents();
        if (c.size() > 0xFFFF)
            return;

        std::vector<uint8_t> frame;
        frame.reserve(c.size() + 4);
        uint16_t size = static_cast<uint16_t>(c.size());
        frame.push_back(static_cast<uint8_t>(size & 0xFF));
        frame.push_back(static_cast<uint8_t>(size >> 8));
        frame.push_back(static_cast<uint8_t>(opcode & 0xFF));
        frame.push_back(static_cast<uint8_t>(opcode >> 8));
        frame.insert(frame.end(), c.begin(), c.end());

        std::lock_guard<std::mutex> lock(_writeMutex);
        if (_queuedBytes + frame.size() > kMaxQueuedBytes)
        {
            // РљР»РёРµРЅС‚ РЅРµ С‡РёС‚Р°РµС‚: РѕС‚РєР»СЋС‡Р°РµРј, С‡С‚РѕР±С‹ РЅРµ СЃСЉРµСЃС‚СЊ РїР°РјСЏС‚СЊ.
            _running = false;
            return;
        }
        _queuedBytes += frame.size();
        _writeQueue.push_back(std::move(frame));
    }

    void Close()
    {
        _running = false;
        _closeAfterFlush = false;
        if (_socket != INVALID_SOCKET)
        {
            shutdown(_socket, SD_BOTH);
            closesocket(_socket);
            _socket = INVALID_SOCKET;
        }
        std::lock_guard<std::mutex> lock(_writeMutex);
        _writeQueue.clear();
        _queuedBytes = 0;
    }

    // Закрыть соединение после отправки уже поставленных в очередь кадров.
    void CloseAfterFlush() { _closeAfterFlush = true; }

    SOCKET GetSocket() const { return _socket; }
    bool IsRunning() const { return _running && _socket != INVALID_SOCKET; }

    bool HasPendingWrites() const
    {
        std::lock_guard<std::mutex> lock(_writeMutex);
        return !_writeQueue.empty();
    }

    // false — соединение мертво, true — всё прочитано (или пока нечего).
    bool TryRecv()
    {
        if (_socket == INVALID_SOCKET) return false;

        while (true)
        {
            int space = static_cast<int>(_recvBuffer.size()) - _recvLen;
            if (space <= 0) return false;

            int n = recv(_socket, reinterpret_cast<char*>(_recvBuffer.data() + _recvLen), space, 0);
            if (n == 0) return false;
            if (n == SOCKET_ERROR)
                return WSAGetLastError() == WSAEWOULDBLOCK;

            _recvLen += n;
            return true;
        }
    }

    bool ProcessFrames()
    {
        while (_recvLen >= 4)
        {
            uint16_t size = static_cast<uint16_t>(_recvBuffer[0] | (_recvBuffer[1] << 8));
            uint16_t opcode = static_cast<uint16_t>(_recvBuffer[2] | (_recvBuffer[3] << 8));
            if (_recvLen < 4 + size) break;

            std::vector<uint8_t> body(_recvBuffer.data() + 4, _recvBuffer.data() + 4 + size);
            ByteBuffer pkt(std::move(body));
            std::memmove(_recvBuffer.data(), _recvBuffer.data() + 4 + size, _recvLen - 4 - size);
            _recvLen -= 4 + size;

            try
            {
                HandlePacket(opcode, pkt);
            }
            catch (std::exception const& e)
            {
                std::cerr << "[packet] opcode 0x" << std::hex << opcode << std::dec
                          << " error: " << e.what() << "\n";
                return false;
            }
        }
        return true;
    }

    bool TrySend()
    {
        if (_socket == INVALID_SOCKET) return false;

        std::lock_guard<std::mutex> lock(_writeMutex);
        while (!_writeQueue.empty())
        {
            auto& frame = _writeQueue.front();
            int sent = send(_socket, reinterpret_cast<const char*>(frame.data()),
                            static_cast<int>(frame.size()), 0);

            if (sent == SOCKET_ERROR)
                return WSAGetLastError() == WSAEWOULDBLOCK;
            if (sent <= 0) return false;

            if (sent < static_cast<int>(frame.size()))
            {
                _queuedBytes -= static_cast<size_t>(sent);
                frame.erase(frame.begin(), frame.begin() + sent);
                return true;
            }

            _queuedBytes -= frame.size();
            _writeQueue.pop_front();
        }

        // Очередь опустела: если просили закрыться после отправки — закрываемся.
        if (_closeAfterFlush)
        {
            _closeAfterFlush = false;
            _running = false;
        }
        return true;
    }

protected:
    virtual void HandlePacket(uint16_t opcode, ByteBuffer& packet) = 0;

public:
    virtual void OnClosed() {}

private:
    SOCKET _socket;
    volatile bool _running = false;
    volatile bool _closeAfterFlush = false;
    std::array<uint8_t, kRecvBufferSize> _recvBuffer{};
    int _recvLen = 0;
    std::deque<std::vector<uint8_t>> _writeQueue;
    size_t _queuedBytes = 0;
    mutable std::mutex _writeMutex;
};

// Generic acceptor (TrinityCore SocketMgr style) вЂ” native Winsock select() loop.
template <typename SessionT>
class Acceptor
{
public:
    explicit Acceptor(uint16_t port)
    {
        _listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_listenSocket == INVALID_SOCKET)
            throw std::runtime_error("socket() failed");

        BOOL reuse = TRUE;
        setsockopt(_listenSocket, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
            throw std::runtime_error("bind() failed");

        if (listen(_listenSocket, SOMAXCONN) == SOCKET_ERROR)
            throw std::runtime_error("listen() failed");

        u_long nonblocking = 1;
        ioctlsocket(_listenSocket, FIONBIO, &nonblocking);

        std::cout << "Acceptor listening on port " << port << "\n";
    }

    ~Acceptor()
    {
        Stop();
        if (_listenSocket != INVALID_SOCKET)
            closesocket(_listenSocket);
    }

    void Stop() { _running = false; }

    // Периодический колбэк игрового цикла (вызывается раз в итерацию select).
    void SetTick(std::function<void()> tick) { _tick = std::move(tick); }

    void Run()
    {
        _running = true;
        while (_running)
        {
            fd_set readSet, writeSet, exceptSet;
            FD_ZERO(&readSet);
            FD_ZERO(&writeSet);
            FD_ZERO(&exceptSet);

            FD_SET(_listenSocket, &readSet);
            FD_SET(_listenSocket, &exceptSet);

            SOCKET maxSock = _listenSocket;

            for (auto& s : _sessions)
            {
                if (!s->IsRunning()) continue;
                SOCKET sk = s->GetSocket();
                FD_SET(sk, &readSet);
                FD_SET(sk, &exceptSet);
                if (s->HasPendingWrites()) FD_SET(sk, &writeSet);
                if (sk > maxSock) maxSock = sk;
            }

            timeval tv{};
            tv.tv_sec = 0;
            tv.tv_usec = 100000; // 100 ms

            int result = select(static_cast<int>(maxSock + 1), &readSet, &writeSet, &exceptSet, &tv);
            if (result == SOCKET_ERROR) continue;

            if (_tick) _tick();

            if (FD_ISSET(_listenSocket, &readSet))
            {
                sockaddr_in clientAddr{};
                int clientLen = static_cast<int>(sizeof(clientAddr));
                SOCKET clientSock = accept(_listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
                if (clientSock != INVALID_SOCKET)
                {
                    u_long nonblocking = 1;
                    ioctlsocket(clientSock, FIONBIO, &nonblocking);
                    auto session = std::make_shared<SessionT>(clientSock);
                    session->Start();
                    _sessions.push_back(session);
                }
            }

            for (auto it = _sessions.begin(); it != _sessions.end();)
            {
                auto s = *it;
                if (!s->IsRunning())
                {
                    s->OnClosed();
                    it = _sessions.erase(it);
                    continue;
                }

                SOCKET sk = s->GetSocket();
                bool dead = false;

                if (FD_ISSET(sk, &readSet) || FD_ISSET(sk, &exceptSet))
                {
                    if (!s->TryRecv()) dead = true;
                    if (!dead && !s->ProcessFrames()) dead = true;
                }

                if (!dead && FD_ISSET(sk, &writeSet))
                {
                    if (!s->TrySend()) dead = true;
                }

                if (dead)
                {
                    s->Close();
                    s->OnClosed();
                    it = _sessions.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

private:
    SOCKET _listenSocket = INVALID_SOCKET;
    volatile bool _running = false;
    std::function<void()> _tick;
    std::vector<std::shared_ptr<SessionT>> _sessions;
};
