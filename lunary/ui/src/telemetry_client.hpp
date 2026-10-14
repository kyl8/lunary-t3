#pragma once

#include <lunary/protocol.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace lunary::ui {

enum class ConnectionState {
    Stopped,
    Connecting,
    Connected,
    Error,
};

struct ClientView {
    ConnectionState state{ConnectionState::Stopped};
    std::string message{"Desconectado"};
    Hello hello{};
    Snapshot snapshot{};
    std::chrono::steady_clock::time_point lastFrame{};
};

class TelemetryClient {
public:
    TelemetryClient();
    ~TelemetryClient();

    TelemetryClient(const TelemetryClient&) = delete;
    TelemetryClient& operator=(const TelemetryClient&) = delete;

    void connect(std::string host, std::uint16_t port);
    void disconnect();
    ClientView view() const;
    ClientView waitForUpdate(std::uint64_t afterSequence,
                             std::chrono::milliseconds timeout) const;

private:
    void run();
    void setState(ConnectionState state, std::string message);

    mutable std::mutex mutex_;
    mutable std::condition_variable updateCondition_;
    ClientView view_;
    std::string host_{"127.0.0.1"};
    std::uint16_t port_{27183};
    std::atomic<bool> running_{true};
    std::atomic<bool> desired_{false};
    std::atomic<std::uint64_t> generation_{0};
    std::thread worker_;
};

} // namespace lunary::ui
