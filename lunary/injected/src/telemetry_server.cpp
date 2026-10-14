#include "telemetry_server.hpp"

#include <android/log.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#define LUNARY_LOG(prio, ...) \
    __android_log_print(prio, "LunaryHook", __VA_ARGS__)

namespace lunary::injected::telemetry {
namespace {

std::mutex gMutex;
lunary::Snapshot gSnapshot;
std::atomic<std::uint64_t> gModuleBase{};
std::once_flag gStart;

bool sendAll(int socket, const std::string& text) {
    std::size_t sent{};
    while (sent < text.size()) {
        const ssize_t result = ::send(socket, text.data() + sent,
                                      text.size() - sent, MSG_NOSIGNAL);
        if (result > 0) {
            sent += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

bool sendSnapshot(int socket, const lunary::Snapshot& snapshot) {
    std::string frame = lunary::makeFrameHeader(snapshot);
    for (const auto& entity : snapshot.entities)
        frame += lunary::makeEntityLine(entity);
    frame += ".\n";
    return sendAll(socket, frame);
}

void serveClient(int client) {
    const int enabled = 1;
    setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
    timeval timeout{1, 0};
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    lunary::Hello hello;
    hello.protocolVersion = lunary::kProtocolVersion;
    hello.pid = static_cast<int>(getpid());
    hello.moduleBase = gModuleBase.load(std::memory_order_acquire);
    hello.packageName = "com.xd.t3.global";
    hello.status = "hook in-process; ESP ligado ao mesmo snapshot da mira";
    if (!sendAll(client, lunary::makeHelloLine(hello))) return;

    std::uint64_t lastSequence = static_cast<std::uint64_t>(-1);
    for (;;) {
        lunary::Snapshot snapshot;
        {
            std::scoped_lock lock(gMutex);
            snapshot = gSnapshot;
        }
        if (snapshot.sequence != lastSequence) {
            if (!sendSnapshot(client, snapshot)) return;
            lastSequence = snapshot.sequence;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

void run() {
    LUNARY_LOG(ANDROID_LOG_INFO, "telemetry: server thread started");
    for (;;) {
        const int server = socket(AF_INET, SOCK_STREAM, 0);
        if (server < 0) {
            LUNARY_LOG(ANDROID_LOG_WARN, "telemetry: socket() failed: %d (%s)",
                       errno, strerror(errno));
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        const int enabled = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(27183);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            LUNARY_LOG(ANDROID_LOG_WARN, "telemetry: bind(27183) failed: %d (%s)",
                       errno, strerror(errno));
            close(server);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        if (listen(server, 1) != 0) {
            LUNARY_LOG(ANDROID_LOG_WARN, "telemetry: listen() failed: %d (%s)",
                       errno, strerror(errno));
            close(server);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        LUNARY_LOG(ANDROID_LOG_INFO, "telemetry: listening on 127.0.0.1:27183");
        for (;;) {
            const int client = accept(server, nullptr, nullptr);
            if (client < 0) {
                if (errno == EINTR) continue;
                LUNARY_LOG(ANDROID_LOG_WARN, "telemetry: accept() failed: %d (%s)",
                           errno, strerror(errno));
                break;
            }
            LUNARY_LOG(ANDROID_LOG_INFO, "telemetry: panel connected");
            serveClient(client);
            close(client);
            LUNARY_LOG(ANDROID_LOG_INFO, "telemetry: panel disconnected");
        }
        close(server);
    }
}

} // namespace

void start(std::uint64_t moduleBase) {
    gModuleBase.store(moduleBase, std::memory_order_release);
    LUNARY_LOG(ANDROID_LOG_INFO, "telemetry: start() called, moduleBase=0x%llx",
               static_cast<unsigned long long>(moduleBase));
    std::call_once(gStart, [] { std::thread(run).detach(); });
}

void publish(lunary::Snapshot snapshot) {
    std::scoped_lock lock(gMutex);
    gSnapshot = std::move(snapshot);
}

} // namespace lunary::injected::telemetry
