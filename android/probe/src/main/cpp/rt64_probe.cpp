#include "rt64_probe.h"

#include <android/log.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "rhi/rt64_render_interface.h"

namespace RT64 {
std::unique_ptr<RenderInterface> CreateVulkanInterface();
}

namespace {

constexpr char LogTag[] = "ZeldaRt64Probe";
constexpr std::uint32_t BufferCount = 3;
constexpr RT64::RenderFormat SwapchainFormat = RT64::RenderFormat::R8G8B8A8_UNORM;

template <typename T>
bool require(const std::unique_ptr<T>& value, const char* operation) {
    if (value != nullptr) {
        return true;
    }

    __android_log_print(ANDROID_LOG_ERROR, LogTag, "stage=2 failed operation=%s", operation);
    return false;
}

} // namespace

struct Rt64Probe::State {
    std::unique_ptr<RT64::RenderInterface> render_interface;
    std::unique_ptr<RT64::RenderDevice> device;
    std::unique_ptr<RT64::RenderCommandQueue> command_queue;
    std::unique_ptr<RT64::RenderCommandList> command_list;
    std::unique_ptr<RT64::RenderCommandSemaphore> acquire_semaphore;
    std::unique_ptr<RT64::RenderCommandSemaphore> render_semaphore;
    std::unique_ptr<RT64::RenderCommandFence> render_fence;
    std::unique_ptr<RT64::RenderSwapChain> swapchain;
    std::vector<std::unique_ptr<RT64::RenderFramebuffer>> framebuffers;
};

Rt64Probe::Rt64Probe() = default;

Rt64Probe::~Rt64Probe() {
    shutdown();
}

bool Rt64Probe::initialize(ANativeWindow* window) {
    shutdown();
    if (window == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "stage=2 failed: native window is null");
        return false;
    }

    __android_log_print(ANDROID_LOG_INFO, LogTag, "stage=2 upstream RT64 RHI probe started");
    auto state = std::make_unique<State>();

    state->render_interface = RT64::CreateVulkanInterface();
    if (!require(state->render_interface, "CreateVulkanInterface")) {
        return false;
    }

    state->device = state->render_interface->createDevice();
    if (!require(state->device, "RenderInterface::createDevice")) {
        return false;
    }

    const RT64::RenderDeviceDescription& description = state->device->getDescription();
    const RT64::RenderDeviceCapabilities& capabilities = state->device->getCapabilities();
    __android_log_print(
        ANDROID_LOG_INFO,
        LogTag,
        "device name=%s vendor=0x%X driver=0x%llX descriptor_indexing=%d scalar_layout=%d",
        description.name.c_str(),
        static_cast<unsigned int>(description.vendor),
        static_cast<unsigned long long>(description.driverVersion),
        capabilities.descriptorIndexing ? 1 : 0,
        capabilities.scalarBlockLayout ? 1 : 0);

    state->command_queue = state->device->createCommandQueue(RT64::RenderCommandListType::DIRECT);
    state->command_list = state->command_queue != nullptr
        ? state->command_queue->createCommandList(RT64::RenderCommandListType::DIRECT)
        : nullptr;
    state->acquire_semaphore = state->device->createCommandSemaphore();
    state->render_semaphore = state->device->createCommandSemaphore();
    state->render_fence = state->device->createCommandFence();
    if (!require(state->command_queue, "RenderDevice::createCommandQueue") ||
        !require(state->command_list, "RenderCommandQueue::createCommandList") ||
        !require(state->acquire_semaphore, "RenderDevice::createCommandSemaphore(acquire)") ||
        !require(state->render_semaphore, "RenderDevice::createCommandSemaphore(render)") ||
        !require(state->render_fence, "RenderDevice::createCommandFence")) {
        return false;
    }

    state->swapchain = state->command_queue->createSwapChain(window, BufferCount, SwapchainFormat);
    if (!require(state->swapchain, "RenderCommandQueue::createSwapChain") ||
        !state->swapchain->resize() || state->swapchain->isEmpty()) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "stage=2 failed operation=RenderSwapChain::resize");
        return false;
    }

    state->framebuffers.reserve(state->swapchain->getTextureCount());
    for (std::uint32_t index = 0; index < state->swapchain->getTextureCount(); ++index) {
        const RT64::RenderTexture* texture = state->swapchain->getTexture(index);
        auto framebuffer = state->device->createFramebuffer(RT64::RenderFramebufferDesc(&texture, 1));
        if (!require(framebuffer, "RenderDevice::createFramebuffer")) {
            return false;
        }
        state->framebuffers.emplace_back(std::move(framebuffer));
    }

    std::uint32_t texture_index = 0;
    if (!state->swapchain->acquireTexture(state->acquire_semaphore.get(), &texture_index)) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "stage=2 failed operation=RenderSwapChain::acquireTexture");
        return false;
    }

    RT64::RenderTexture* texture = state->swapchain->getTexture(texture_index);
    state->command_list->begin();
    state->command_list->barriers(
        RT64::RenderBarrierStage::GRAPHICS,
        RT64::RenderTextureBarrier(texture, RT64::RenderTextureLayout::COLOR_WRITE));
    state->command_list->setFramebuffer(state->framebuffers[texture_index].get());
    state->command_list->clearColor(0, RT64::RenderColor(0.03F, 0.04F, 0.08F, 1.0F));
    state->command_list->barriers(
        RT64::RenderBarrierStage::NONE,
        RT64::RenderTextureBarrier(texture, RT64::RenderTextureLayout::PRESENT));
    state->command_list->end();

    RT64::RenderCommandSemaphore* wait_semaphore = state->acquire_semaphore.get();
    RT64::RenderCommandSemaphore* signal_semaphore = state->render_semaphore.get();
    const RT64::RenderCommandList* command_list = state->command_list.get();
    state->command_queue->executeCommandLists(
        &command_list,
        1,
        &wait_semaphore,
        1,
        &signal_semaphore,
        1,
        state->render_fence.get());
    state->command_queue->waitForCommandFence(state->render_fence.get());

    if (!state->swapchain->present(texture_index, &signal_semaphore, 1)) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "stage=2 failed operation=RenderSwapChain::present");
        return false;
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        LogTag,
        "stage=2 success extent=%ux%u images=%u texture=%u",
        state->swapchain->getWidth(),
        state->swapchain->getHeight(),
        state->swapchain->getTextureCount(),
        texture_index);
    state_ = std::move(state);
    return true;
}

void Rt64Probe::shutdown() {
    state_.reset();
}

bool Rt64Probe::ready() const {
    return state_ != nullptr;
}
