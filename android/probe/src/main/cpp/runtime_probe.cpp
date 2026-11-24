#include "runtime_probe.h"

#include <android/log.h>

#include <array>
#include <cstdint>
#include <vector>

#include "recomp.h"
#include "ultramodern/ultramodern.hpp"

RECOMP_FUNC void runtime_probe_generated_abi(std::uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = ADD32(ctx->r4, ctx->r5);
    SD(ctx->r7, 0, ctx->r6);
    ctx->r8 = LD(0, ctx->r6);
}

namespace {

constexpr char LogTag[] = "ZeldaRuntimeProbe";
constexpr std::size_t RdramSize = 8 * 1024 * 1024;
constexpr std::uint64_t Kseg0Base = 0xFFFFFFFF80000000ULL;

std::int32_t virtual_address(std::uint32_t offset) {
    return static_cast<std::int32_t>(0x80000000U + offset);
}

bool check(bool condition, const char* name) {
    __android_log_print(
        condition ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
        LogTag,
        "check=%s result=%s",
        name,
        condition ? "pass" : "fail");
    return condition;
}

bool test_generated_abi(std::uint8_t* rdram) {
    recomp_context context{};
    context.r4 = 0x7FFFFFFFULL;
    context.r5 = 1;
    context.r6 = Kseg0Base + 0x100;
    context.r7 = 0x1122334455667788ULL;
    context.f_odd = &context.f0.u32h;

    runtime_probe_generated_abi(rdram, &context);

    bool passed = true;
    passed &= check(context.r2 == 0xFFFFFFFF80000000ULL, "generated ADD32 sign extension");
    passed &= check(context.r8 == context.r7, "generated SD/LD round trip");
    passed &= check(MEM_BU(0, context.r6) == 0x11, "N64 byte order byte 0");
    passed &= check(MEM_BU(7, context.r6) == 0x88, "N64 byte order byte 7");

    std::int64_t low = 0;
    std::int64_t high = 0;
    DMULT(-0x12345678LL, 0x10203040LL, &low, &high);
    const __int128 expected = static_cast<__int128>(-0x12345678LL) * 0x10203040LL;
    passed &= check(
        low == static_cast<std::int64_t>(expected) &&
            high == static_cast<std::int64_t>(expected >> 64),
        "128-bit multiply helpers");
    return passed;
}

bool test_scheduler_queue(std::uint8_t* rdram) {
    constexpr std::uint32_t QueueOffset = 0x200;
    constexpr std::array<std::uint32_t, 3> ThreadOffsets{0x300, 0x380, 0x400};
    constexpr std::array<OSPri, 3> Priorities{10, 30, 20};

    const PTR(PTR(OSThread)) queue = virtual_address(QueueOffset);
    *TO_PTR(PTR(OSThread), queue) = NULLPTR;

    for (std::size_t index = 0; index < ThreadOffsets.size(); ++index) {
        const PTR(OSThread) thread_address = virtual_address(ThreadOffsets[index]);
        OSThread* thread = TO_PTR(OSThread, thread_address);
        *thread = {};
        thread->id = static_cast<OSId>(index + 1);
        thread->priority = Priorities[index];
        ultramodern::thread_queue_insert(rdram, queue, thread_address);
    }

    bool passed = true;
    constexpr std::array<OSPri, 3> ExpectedOrder{30, 20, 10};
    for (OSPri expected_priority : ExpectedOrder) {
        const PTR(OSThread) thread_address = ultramodern::thread_queue_pop(rdram, queue);
        passed &= check(
            TO_PTR(OSThread, thread_address)->priority == expected_priority,
            "scheduler priority order");
    }
    passed &= check(ultramodern::thread_queue_empty(rdram, queue), "scheduler queue empty");
    return passed;
}

} // namespace

bool RuntimeProbe::run() {
    __android_log_print(ANDROID_LOG_INFO, LogTag, "stage=3 N64 runtime ABI probe started");
    std::vector<std::uint8_t> rdram(RdramSize);

    const bool generated_abi_passed = test_generated_abi(rdram.data());
    const bool scheduler_queue_passed = test_scheduler_queue(rdram.data());
    const bool passed = generated_abi_passed && scheduler_queue_passed;

    __android_log_print(
        passed ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
        LogTag,
        "stage=3 %s generated_abi=%d scheduler_queue=%d",
        passed ? "success" : "failed",
        generated_abi_passed ? 1 : 0,
        scheduler_queue_passed ? 1 : 0);
    return passed;
}
