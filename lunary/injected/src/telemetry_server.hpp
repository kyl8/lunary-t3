#pragma once

#include <lunary/protocol.hpp>

#include <cstdint>
#include <string>

namespace lunary::injected::telemetry {

void start(std::uint64_t moduleBase);
void publish(lunary::Snapshot snapshot);

} // namespace lunary::injected::telemetry
