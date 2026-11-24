#define VK_USE_PLATFORM_ANDROID_KHR

#include "vulkan_probe.h"

#include <android/log.h>

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <vector>

namespace {

constexpr char LogTag[] = "ZeldaVulkanProbe";

void log_result(const char* operation, VkResult result) {
    __android_log_print(
        result == VK_SUCCESS ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
        LogTag,
        "%s result=%d",
        operation,
        static_cast<int>(result));
}

bool supports_swapchain(VkPhysicalDevice physical_device) {
    std::uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, nullptr) != VK_SUCCESS) {
        return false;
    }

    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, extensions.data()) != VK_SUCCESS) {
        return false;
    }

    return std::any_of(extensions.begin(), extensions.end(), [](const VkExtensionProperties& extension) {
        return std::string_view(extension.extensionName) == VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    });
}

bool find_graphics_present_queue(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    std::uint32_t& selected_queue) {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queues.data());

    for (std::uint32_t index = 0; index < count; ++index) {
        VkBool32 present_supported = VK_FALSE;
        const VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device,
            index,
            surface,
            &present_supported);

        __android_log_print(
            ANDROID_LOG_INFO,
            LogTag,
            "queue=%u flags=0x%X count=%u present=%u result=%d",
            index,
            queues[index].queueFlags,
            queues[index].queueCount,
            present_supported,
            static_cast<int>(result));

        if (result == VK_SUCCESS &&
            queues[index].queueCount > 0 &&
            (queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
            present_supported == VK_TRUE) {
            selected_queue = index;
            return true;
        }
    }

    return false;
}

VkCompositeAlphaFlagBitsKHR choose_composite_alpha(VkCompositeAlphaFlagsKHR supported) {
    constexpr std::array Candidates{
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };

    for (const VkCompositeAlphaFlagBitsKHR candidate : Candidates) {
        if ((supported & candidate) != 0) {
            return candidate;
        }
    }

    return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities, ANativeWindow* window) {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    const auto width = static_cast<std::uint32_t>(std::max(ANativeWindow_getWidth(window), 1));
    const auto height = static_cast<std::uint32_t>(std::max(ANativeWindow_getHeight(window), 1));
    return {
        std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
}

} // namespace

VulkanProbe::~VulkanProbe() {
    shutdown();
}

bool VulkanProbe::initialize(ANativeWindow* window) {
    shutdown();
    if (window == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "stage=1 failed: native window is null");
        return false;
    }

    __android_log_print(ANDROID_LOG_INFO, LogTag, "stage=1 Vulkan probe started");

    const VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Zelda64 Android Probe",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "none",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };
    constexpr std::array InstanceExtensions{
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    };
    const VkInstanceCreateInfo instance_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &application_info,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<std::uint32_t>(InstanceExtensions.size()),
        .ppEnabledExtensionNames = InstanceExtensions.data(),
    };

    VkResult result = vkCreateInstance(&instance_info, nullptr, &instance_);
    log_result("vkCreateInstance", result);
    if (result != VK_SUCCESS) {
        shutdown();
        return false;
    }

    const VkAndroidSurfaceCreateInfoKHR surface_info{
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .window = window,
    };
    result = vkCreateAndroidSurfaceKHR(instance_, &surface_info, nullptr, &surface_);
    log_result("vkCreateAndroidSurfaceKHR", result);
    if (result != VK_SUCCESS) {
        shutdown();
        return false;
    }

    std::uint32_t physical_device_count = 0;
    result = vkEnumeratePhysicalDevices(instance_, &physical_device_count, nullptr);
    log_result("vkEnumeratePhysicalDevices", result);
    if (result != VK_SUCCESS || physical_device_count == 0) {
        shutdown();
        return false;
    }

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    result = vkEnumeratePhysicalDevices(instance_, &physical_device_count, physical_devices.data());
    if (result != VK_SUCCESS) {
        log_result("vkEnumeratePhysicalDevices list", result);
        shutdown();
        return false;
    }

    for (VkPhysicalDevice candidate : physical_devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(candidate, &properties);
        __android_log_print(
            ANDROID_LOG_INFO,
            LogTag,
            "device name=%s vendor=0x%X device=0x%X api=%u.%u.%u driver=0x%X",
            properties.deviceName,
            properties.vendorID,
            properties.deviceID,
            VK_VERSION_MAJOR(properties.apiVersion),
            VK_VERSION_MINOR(properties.apiVersion),
            VK_VERSION_PATCH(properties.apiVersion),
            properties.driverVersion);

        std::uint32_t queue_family = 0;
        if (supports_swapchain(candidate) && find_graphics_present_queue(candidate, surface_, queue_family)) {
            physical_device_ = candidate;
            queue_family_ = queue_family;
            break;
        }
    }

    if (physical_device_ == VK_NULL_HANDLE) {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "stage=1 failed: no graphics+present Vulkan device");
        shutdown();
        return false;
    }

    constexpr float QueuePriority = 1.0F;
    const VkDeviceQueueCreateInfo queue_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = queue_family_,
        .queueCount = 1,
        .pQueuePriorities = &QueuePriority,
    };
    constexpr std::array DeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const VkDeviceCreateInfo device_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<std::uint32_t>(DeviceExtensions.size()),
        .ppEnabledExtensionNames = DeviceExtensions.data(),
        .pEnabledFeatures = nullptr,
    };
    result = vkCreateDevice(physical_device_, &device_info, nullptr, &device_);
    log_result("vkCreateDevice", result);
    if (result != VK_SUCCESS) {
        shutdown();
        return false;
    }

    VkSurfaceCapabilitiesKHR capabilities{};
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities);
    log_result("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", result);
    if (result != VK_SUCCESS || (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
        shutdown();
        return false;
    }

    std::uint32_t format_count = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr);
    if (result != VK_SUCCESS || format_count == 0) {
        log_result("vkGetPhysicalDeviceSurfaceFormatsKHR", result);
        shutdown();
        return false;
    }
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, formats.data());
    if (result != VK_SUCCESS) {
        log_result("vkGetPhysicalDeviceSurfaceFormatsKHR list", result);
        shutdown();
        return false;
    }

    const auto preferred_format = std::find_if(formats.begin(), formats.end(), [](const VkSurfaceFormatKHR& format) {
        return format.format == VK_FORMAT_R8G8B8A8_UNORM &&
               format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });
    const VkSurfaceFormatKHR surface_format =
        preferred_format != formats.end() ? *preferred_format : formats.front();
    const VkExtent2D extent = choose_extent(capabilities, window);

    std::uint32_t image_count = std::max(capabilities.minImageCount + 1, 2U);
    if (capabilities.maxImageCount > 0) {
        image_count = std::min(image_count, capabilities.maxImageCount);
    }

    const VkSwapchainCreateInfoKHR swapchain_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface_,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = choose_composite_alpha(capabilities.supportedCompositeAlpha),
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    result = vkCreateSwapchainKHR(device_, &swapchain_info, nullptr, &swapchain_);
    log_result("vkCreateSwapchainKHR", result);
    if (result != VK_SUCCESS) {
        shutdown();
        return false;
    }

    std::uint32_t swapchain_image_count = 0;
    result = vkGetSwapchainImagesKHR(device_, swapchain_, &swapchain_image_count, nullptr);
    log_result("vkGetSwapchainImagesKHR", result);
    if (result != VK_SUCCESS || swapchain_image_count == 0) {
        shutdown();
        return false;
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        LogTag,
        "stage=1 success queue=%u extent=%ux%u format=%d images=%u",
        queue_family_,
        extent.width,
        extent.height,
        surface_format.format,
        swapchain_image_count);
    return true;
}

void VulkanProbe::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        }
        vkDestroyDevice(device_, nullptr);
    }
    if (instance_ != VK_NULL_HANDLE && surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }

    swapchain_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
    physical_device_ = VK_NULL_HANDLE;
    surface_ = VK_NULL_HANDLE;
    instance_ = VK_NULL_HANDLE;
    queue_family_ = 0;
}

bool VulkanProbe::ready() const {
    return swapchain_ != VK_NULL_HANDLE;
}
