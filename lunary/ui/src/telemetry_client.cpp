#include "telemetry_client.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <string_view>
#include <thread>

namespace lunary::ui {

namespace {

class SocketHandle {
public:
    explicit SocketHandle(SOCKET socket = INVALID_SOCKET) : socket_(socket) {}
    ~SocketHandle() { reset(); }
    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    SOCKET get() const { return socket_; }
    explicit operator bool() const { return socket_ != INVALID_SOCKET; }
    void reset(SOCKET socket = INVALID_SOCKET) {
        if (socket_ != INVALID_SOCKET) {
            ::shutdown(socket_, SD_BOTH);
            ::closesocket(socket_);
        }
        socket_ = socket;
    }
private:
    SOCKET socket_ = INVALID_SOCKET;
};

std::string socketError(const char* action) {
    return std::string(action) + " (Winsock " + std::to_string(::WSAGetLastError()) + ")";
}

} // namespace

TelemetryClient::TelemetryClient() {
    WSADATA data{};
    if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        setState(ConnectionState::Error, "WSAStartup falhou");
        running_ = false;
        return;
    }
    worker_ = std::thread(&TelemetryClient::run, this);
}

TelemetryClient::~TelemetryClient() {
    running_ = false;
    desired_ = false;
    ++generation_;
    if (worker_.joinable()) worker_.join();
    ::WSACleanup();
}

void TelemetryClient::connect(std::string host, std::uint16_t port) {
    {
        std::scoped_lock lock(mutex_);
        host_ = std::move(host);
        port_ = port;
    }
    desired_ = true;
    ++generation_;
    setState(ConnectionState::Connecting, "Conectando ao agente...");
}

void TelemetryClient::disconnect() {
    desired_ = false;
    ++generation_;
    setState(ConnectionState::Stopped, "Desconectado");
}

ClientView TelemetryClient::view() const {
    std::scoped_lock lock(mutex_);
    return view_;
}

ClientView TelemetryClient::waitForUpdate(std::uint64_t afterSequence,
                                          std::chrono::milliseconds timeout) const {
    std::unique_lock lock(mutex_);
    updateCondition_.wait_for(lock, timeout, [&] {
        return !running_.load() || view_.snapshot.sequence != afterSequence;
    });
    return view_;
}

void TelemetryClient::setState(ConnectionState state, std::string message) {
    {
        std::scoped_lock lock(mutex_);
        view_.state = state;
        view_.message = std::move(message);
    }
    updateCondition_.notify_all();
}

void TelemetryClient::run() {
    std::uint64_t handledGeneration = 0;
    while (running_) {
        if (!desired_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            continue;
        }
        handledGeneration = generation_.load();
        std::string host;
        std::uint16_t port{};
        {
            std::scoped_lock lock(mutex_);
            host = host_;
            port = port_;
        }
        setState(ConnectionState::Connecting, "Conectando a " + host + ":" + std::to_string(port));

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        addrinfo* rawAddresses = nullptr;
        const auto portText = std::to_string(port);
        if (::getaddrinfo(host.c_str(), portText.c_str(), &hints, &rawAddresses) != 0) {
            setState(ConnectionState::Error, socketError("Endereco invalido"));
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            continue;
        }
        SocketHandle socket(::socket(rawAddresses->ai_family, rawAddresses->ai_socktype, rawAddresses->ai_protocol));
        const bool connected = socket &&
            ::connect(socket.get(), rawAddresses->ai_addr, static_cast<int>(rawAddresses->ai_addrlen)) == 0;
        ::freeaddrinfo(rawAddresses);
        if (!connected) {
            setState(ConnectionState::Error, socketError("Agente indisponivel"));
            std::this_thread::sleep_for(std::chrono::milliseconds(900));
            continue;
        }
        BOOL noDelay = TRUE;
        ::setsockopt(socket.get(), IPPROTO_TCP, TCP_NODELAY,
                     reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
        DWORD timeout = 500;
        ::setsockopt(socket.get(), SOL_SOCKET, SO_RCVTIMEO,
                     reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setState(ConnectionState::Connected, "Conectado; aguardando telemetria");

        std::string pending;
        pending.reserve(64 * 1024);
        Snapshot building;
        std::size_t expectedEntities = 0;
        bool haveHeader = false;
        bool connectionAlive = true;
        std::array<char, 16 * 1024> buffer{};
        while (running_ && desired_ && generation_.load() == handledGeneration && connectionAlive) {
            const int received = ::recv(socket.get(), buffer.data(), static_cast<int>(buffer.size()), 0);
            if (received == 0) break;
            if (received < 0) {
                const int error = ::WSAGetLastError();
                if (error == WSAETIMEDOUT || error == WSAEWOULDBLOCK) continue;
                break;
            }
            pending.append(buffer.data(), static_cast<std::size_t>(received));
            if (pending.size() > 4 * 1024 * 1024) {
                setState(ConnectionState::Error, "Fluxo do agente excedeu o limite");
                break;
            }
            for (;;) {
                const auto newline = pending.find('\n');
                if (newline == std::string::npos) break;
                std::string line = pending.substr(0, newline);
                pending.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;
                if (line[0] == 'H') {
                    Hello hello;
                    if (parseHelloLine(line, hello)) {
                        {
                            std::scoped_lock lock(mutex_);
                            view_.hello = std::move(hello);
                            view_.state = ConnectionState::Connected;
                            view_.message = view_.hello.status;
                        }
                        updateCondition_.notify_all();
                    }
                } else if (line[0] == 'F') {
                    building = {};
                    haveHeader = parseFrameHeader(line, building, expectedEntities) && expectedEntities <= 8192;
                } else if (line[0] == 'E' && haveHeader) {
                    EntityTelemetry entity;
                    if (parseEntityLine(line, entity)) building.entities.push_back(std::move(entity));
                } else if (line == "." && haveHeader) {
                    if (building.entities.size() == expectedEntities) {
                        {
                            std::scoped_lock lock(mutex_);
                            view_.snapshot = std::move(building);
                            view_.lastFrame = std::chrono::steady_clock::now();
                            view_.state = ConnectionState::Connected;
                        }
                        updateCondition_.notify_all();
                    }
                    haveHeader = false;
                }
            }
        }
        if (desired_ && generation_.load() == handledGeneration) {
            setState(ConnectionState::Error, "Conexao perdida; tentando novamente");
            std::this_thread::sleep_for(std::chrono::milliseconds(700));
        }
    }
}

} // namespace lunary::ui
