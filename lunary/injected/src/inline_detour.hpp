#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <string>

namespace lunary::injected {

class InlineDetour {
public:
    InlineDetour() = default;
    ~InlineDetour();

    InlineDetour(const InlineDetour&) = delete;
    InlineDetour& operator=(const InlineDetour&) = delete;

    bool install(void* target, void* replacement, std::atomic<void*>* original,
                 std::string& error);
    bool uninstall(std::string& error);
    [[nodiscard]] bool installed() const { return target_ != nullptr; }

private:
    void* target_{};
    void* codePage_{};
    std::size_t codePageSize_{};
    std::uint32_t originalInstructions_[4]{};
    std::size_t patchWordCount_{};
};

} // namespace lunary::injected
