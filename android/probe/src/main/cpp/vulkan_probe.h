#pragma once

#include <android/native_window.h>
#include <vulkan/vulkan.h>

#include <cstdint>

class VulkanProbe {
public:
    VulkanProbe() = default;
    ~VulkanProbe();

    VulkanProbe(const VulkanProbe&) = delete;
    VulkanProbe& operator=(const VulkanProbe&) = delete;

    bool initialize(ANativeWindow* window);
    void shutdown();
    bool ready() const;

private:
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::uint32_t queue_family_ = 0;
};
