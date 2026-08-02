#ifndef VULKAN_EVENT_H_
#define VULKAN_EVENT_H_

#include "pjrt_c_api.h"
#include "vulkan_backend.h"
#include <memory>
#include <mutex>
#include <atomic>

namespace vulkan_pjrt {

class VulkanEventImpl {
 public:
  const VulkanDevice* device{nullptr};
  VkFence fence{VK_NULL_HANDLE};
  std::atomic<bool> is_completed{false};
  std::string error_message;

  VulkanEventImpl(const VulkanDevice* dev = nullptr, VkFence f = VK_NULL_HANDLE)
      : device(dev), fence(f) {
    if (!dev || f == VK_NULL_HANDLE) {
      is_completed = true;
    }
  }

  ~VulkanEventImpl() {
    if (device && fence != VK_NULL_HANDLE) {
      vkDestroyFence(device->device, fence, nullptr);
    }
  }

  void Await() {
    if (is_completed) return;
    if (device && fence != VK_NULL_HANDLE) {
      vkWaitForFences(device->device, 1, &fence, VK_TRUE, UINT64_MAX);
      is_completed = true;
    }
  }

  bool IsReady() {
    if (is_completed) return true;
    if (device && fence != VK_NULL_HANDLE) {
      VkResult res = vkGetFenceStatus(device->device, fence);
      if (res == VK_SUCCESS) {
        is_completed = true;
        return true;
      }
    }
    return is_completed;
  }
};

}  // namespace vulkan_pjrt

struct PJRT_Event {
  std::unique_ptr<vulkan_pjrt::VulkanEventImpl> impl;
};

#endif  // VULKAN_EVENT_H_
