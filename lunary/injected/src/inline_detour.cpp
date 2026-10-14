#include "inline_detour.hpp"

#if !defined(__aarch64__)
#error "The in-process detour currently supports ARM64 only."
#endif

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace lunary::injected {
namespace {

constexpr std::uint32_t kNop = 0xD503201FU;
constexpr std::uint32_t kBranchRegister17 = 0xD61F0220U;
constexpr std::uint32_t kBranchLinkRegister17 = 0xD63F0220U;
constexpr std::size_t kPatchWordCount = 4;
constexpr std::size_t kMaximumTrampolineWords = 32;
constexpr const char* kDiagnosticPath = "/data/local/tmp/lunary-hook.log";

void detourDiagnostic(const char* stage, const void* address = nullptr) {
    char line[192]{};
    if (address)
        std::snprintf(line, sizeof(line), "InlineDetour: %s address=%p", stage, address);
    else
        std::snprintf(line, sizeof(line), "InlineDetour: %s", stage);
    const int descriptor = open(kDiagnosticPath, O_WRONLY | O_APPEND | O_CLOEXEC);
    if (descriptor < 0) return;
    (void)write(descriptor, line, std::strlen(line));
    (void)write(descriptor, "\n", 1U);
    close(descriptor);
}

std::uintptr_t alignDown(std::uintptr_t value, std::size_t alignment) {
    return value & ~(static_cast<std::uintptr_t>(alignment) - 1U);
}

std::uint32_t loadLiteral(std::uint32_t registerIndex, std::uint32_t byteOffset) {
    return 0x58000000U | ((byteOffset / 4U) << 5U) | registerIndex;
}

bool isAdr(std::uint32_t instruction) {
    return (instruction & 0x9F000000U) == 0x10000000U;
}

bool isAdrp(std::uint32_t instruction) {
    return (instruction & 0x9F000000U) == 0x90000000U;
}

bool isUnconditionalBranch(std::uint32_t instruction) {
    return (instruction & 0x7C000000U) == 0x14000000U; // B/BL
}

bool isConditionalBranch(std::uint32_t instruction) {
    return (instruction & 0xFF000010U) == 0x54000000U; // B.cond
}

bool isCompareBranch(std::uint32_t instruction) {
    return (instruction & 0x7E000000U) == 0x34000000U; // CBZ/CBNZ
}

bool isTestBranch(std::uint32_t instruction) {
    return (instruction & 0x7E000000U) == 0x36000000U; // TBZ/TBNZ
}

bool isLiteralLoad(std::uint32_t instruction) {
    return (instruction & 0x3B000000U) == 0x18000000U;
}

std::int64_t signExtend(std::uint64_t value, unsigned bits) {
    const std::uint64_t sign = std::uint64_t{1} << (bits - 1U);
    return static_cast<std::int64_t>((value ^ sign) - sign);
}

std::uintptr_t decodeAdr(std::uint32_t instruction,
                         std::uintptr_t instructionAddress) {
    const std::uint64_t immediate =
        ((instruction >> 5U) & 0x7FFFFU) << 2U |
        ((instruction >> 29U) & 0x3U);
    return static_cast<std::uintptr_t>(
        static_cast<std::int64_t>(instructionAddress) + signExtend(immediate, 21));
}

std::uintptr_t decodeAdrp(std::uint32_t instruction,
                          std::uintptr_t instructionAddress) {
    const std::uint64_t encoded =
        ((instruction >> 5U) & 0x7FFFFU) << 2U |
        ((instruction >> 29U) & 0x3U);
    const std::int64_t immediate = signExtend(encoded, 21);
    return static_cast<std::uintptr_t>(
        static_cast<std::int64_t>(instructionAddress & ~std::uintptr_t{0xFFF}) +
        immediate * 4096LL);
}

std::uintptr_t decodeBranchTarget(std::uint32_t instruction,
                                  std::uintptr_t instructionAddress) {
    std::int64_t words{};
    if (isUnconditionalBranch(instruction))
        words = signExtend(instruction & 0x03FFFFFFU, 26);
    else if (isConditionalBranch(instruction) || isCompareBranch(instruction))
        words = signExtend((instruction >> 5U) & 0x7FFFFU, 19);
    else
        words = signExtend((instruction >> 5U) & 0x3FFFU, 14);
    return static_cast<std::uintptr_t>(
        static_cast<std::int64_t>(instructionAddress) + words * 4LL);
}

bool protectTarget(void* target, std::size_t length, int protection,
                   std::string& error) {
    const auto pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    const auto page = alignDown(reinterpret_cast<std::uintptr_t>(target), pageSize);
    const auto end = reinterpret_cast<std::uintptr_t>(target) + length;
    const auto protectedLength = alignDown(end + pageSize - 1U, pageSize) - page;
    if (mprotect(reinterpret_cast<void*>(page), protectedLength, protection) == 0)
        return true;
    error = "mprotect failed: " + std::string(std::strerror(errno));
    return false;
}

} // namespace

InlineDetour::~InlineDetour() {
    // Unloading injected code while another thread is inside the hook is unsafe.
    // The owner must explicitly uninstall after coordinating its lifecycle.
}

bool InlineDetour::install(void* target, void* replacement, std::atomic<void*>* original,
                           std::string& error) {
    detourDiagnostic("install entry", target);
    if (installed()) {
        error = "detour already installed";
        return false;
    }
    if (!target || !replacement || !original ||
        (reinterpret_cast<std::uintptr_t>(target) & 3U) != 0U) {
        error = "invalid or unaligned detour address";
        return false;
    }

    std::uint32_t displaced[kPatchWordCount]{};
    detourDiagnostic("reading target prologue", target);
    std::memcpy(displaced, target, sizeof(displaced));
    detourDiagnostic("target prologue read");

    const auto pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* code = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code == MAP_FAILED) {
        error = "could not allocate executable memory for the trampoline: " +
                std::string(std::strerror(errno));
        return false;
    }
    detourDiagnostic("trampoline mapped", code);

    const auto targetAddress = reinterpret_cast<std::uintptr_t>(target);
    auto* bytes = static_cast<std::byte*>(code);
    auto* words = reinterpret_cast<std::uint32_t*>(bytes);

    struct LiteralReference {
        std::size_t wordIndex{};
        std::uint32_t registerIndex{};
        std::uintptr_t value{};
    };
    std::array<LiteralReference, 12> literals{};
    std::size_t literalCount{};
    std::size_t wordCount{};

    auto emit = [&](std::uint32_t instruction) -> bool {
        if (wordCount >= kMaximumTrampolineWords) return false;
        words[wordCount++] = instruction;
        return true;
    };
    auto emitLiteralLoad = [&](std::uint32_t registerIndex,
                               std::uintptr_t value) -> bool {
        if (literalCount >= literals.size() || wordCount >= kMaximumTrampolineWords)
            return false;
        literals[literalCount++] = {wordCount, registerIndex, value};
        return emit(0U);
    };
    auto emitAbsoluteBranch = [&](std::uintptr_t destination, bool link) -> bool {
        return emitLiteralLoad(17U, destination) &&
               emit(link ? kBranchLinkRegister17 : kBranchRegister17);
    };

    bool relocationOk = true;
    for (std::size_t index = 0; index < kPatchWordCount && relocationOk; ++index) {
        const auto instruction = displaced[index];
        const auto instructionAddress = targetAddress + index * sizeof(std::uint32_t);
        if (isAdr(instruction)) {
            relocationOk = emitLiteralLoad(instruction & 0x1FU,
                                           decodeAdr(instruction, instructionAddress));
        } else if (isAdrp(instruction)) {
            relocationOk = emitLiteralLoad(instruction & 0x1FU,
                                           decodeAdrp(instruction, instructionAddress));
        } else if (isUnconditionalBranch(instruction)) {
            const auto destination = decodeBranchTarget(instruction, instructionAddress);
            const bool link = (instruction & 0x80000000U) != 0;
            relocationOk = emitAbsoluteBranch(destination, link);
        } else if (isConditionalBranch(instruction) ||
                   isCompareBranch(instruction) || isTestBranch(instruction)) {
            const auto destination = decodeBranchTarget(instruction, instructionAddress);
            std::uint32_t inverse = instruction ^ (isConditionalBranch(instruction)
                ? 0x1U : 0x01000000U);
            if (isConditionalBranch(instruction) || isCompareBranch(instruction)) {
                inverse = (inverse & ~0x00FFFFE0U) | (3U << 5U);
            } else {
                inverse = (inverse & ~0x0007FFE0U) | (3U << 5U);
            }
            relocationOk = emit(inverse) && emitAbsoluteBranch(destination, false);
        } else if (isLiteralLoad(instruction)) {
            relocationOk = false;
        } else {
            relocationOk = emit(instruction);
        }
    }
    const auto continuation = targetAddress + sizeof(displaced);
    relocationOk = relocationOk && emitAbsoluteBranch(continuation, false);
    if (!relocationOk) {
        error = "ARM64 prologue contains an unsupported PC-relative instruction";
        munmap(code, pageSize);
        return false;
    }
    detourDiagnostic("prologue relocated");

    if ((wordCount & 1U) != 0) emit(kNop);
    std::size_t literalOffset = wordCount * sizeof(std::uint32_t);
    for (std::size_t index = 0; index < literalCount; ++index) {
        const auto& reference = literals[index];
        const auto instructionOffset = reference.wordIndex * sizeof(std::uint32_t);
        const auto relative = literalOffset - instructionOffset;
        words[reference.wordIndex] = loadLiteral(reference.registerIndex,
                                                 static_cast<std::uint32_t>(relative));
        std::memcpy(bytes + literalOffset, &reference.value, sizeof(reference.value));
        literalOffset += sizeof(reference.value);
    }
    __builtin___clear_cache(reinterpret_cast<char*>(code),
                            reinterpret_cast<char*>(code) + literalOffset);
    if (mprotect(code, pageSize, PROT_READ | PROT_EXEC) != 0) {
        error = "could not make the trampoline executable: " +
                std::string(std::strerror(errno));
        munmap(code, pageSize);
        return false;
    }
    detourDiagnostic("trampoline executable", code);

    // Publish the trampoline before the entry can branch to the replacement.
    original->store(code, std::memory_order_release);
    if (!protectTarget(target, sizeof(displaced),
                       PROT_READ | PROT_WRITE | PROT_EXEC, error)) {
        original->store(nullptr, std::memory_order_release);
        munmap(code, pageSize);
        return false;
    }
    detourDiagnostic("target writable", target);

    const auto replacementAddress = reinterpret_cast<std::uintptr_t>(replacement);
    std::uint32_t entryStub[4] = {
        loadLiteral(17, 8), kBranchRegister17, 0, 0,
    };
    std::memcpy(&entryStub[2], &replacementAddress, sizeof(replacementAddress));

    // Prepare the tail first. The atomic word-zero store activates the detour.
    std::memcpy(static_cast<std::byte*>(target) + 4, &entryStub[1], 12);
    __atomic_store_n(static_cast<std::uint32_t*>(target), entryStub[0],
                     __ATOMIC_RELEASE);
    __builtin___clear_cache(static_cast<char*>(target),
                            static_cast<char*>(target) + sizeof(entryStub));
    detourDiagnostic("target patched", target);

    std::string restoreError;
    if (!protectTarget(target, sizeof(displaced), PROT_READ | PROT_EXEC,
                       restoreError)) {
        std::memcpy(static_cast<std::byte*>(target) + 4, &displaced[1], 12);
        __atomic_store_n(static_cast<std::uint32_t*>(target), displaced[0],
                         __ATOMIC_RELEASE);
        __builtin___clear_cache(static_cast<char*>(target),
                                static_cast<char*>(target) + sizeof(displaced));
        error = restoreError;
        original->store(nullptr, std::memory_order_release);
        munmap(code, pageSize);
        return false;
    }
    detourDiagnostic("target protection restored", target);

    target_ = target;
    codePage_ = code;
    codePageSize_ = pageSize;
    std::memcpy(originalInstructions_, displaced, sizeof(originalInstructions_));
    patchWordCount_ = kPatchWordCount;
    return true;
}

bool InlineDetour::uninstall(std::string& error) {
    if (!installed()) return true;
    if (!protectTarget(target_, patchWordCount_ * sizeof(std::uint32_t),
                       PROT_READ | PROT_WRITE | PROT_EXEC, error)) return false;
    std::memcpy(static_cast<std::byte*>(target_) + 4,
                &originalInstructions_[1], 12);
    __atomic_store_n(static_cast<std::uint32_t*>(target_), originalInstructions_[0],
                     __ATOMIC_RELEASE);
    __builtin___clear_cache(static_cast<char*>(target_),
                            static_cast<char*>(target_) + patchWordCount_ * 4);
    std::string restoreError;
    const bool restored = protectTarget(target_, patchWordCount_ * sizeof(std::uint32_t),
                                        PROT_READ | PROT_EXEC, restoreError);
    if (restored) {
        munmap(codePage_, codePageSize_);
        target_ = nullptr;
        codePage_ = nullptr;
        codePageSize_ = 0;
        std::memset(originalInstructions_, 0, sizeof(originalInstructions_));
        patchWordCount_ = 0;
    } else {
        error = restoreError;
    }
    return restored;
}

} // namespace lunary::injected
