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
  AsyncExecution exec;
  std::atomic<bool> is_completed{false};
  std::string error_message;

  VulkanEventImpl(const VulkanDevice* dev = nullptr, AsyncExecution e = AsyncExecution{})
      : device(dev), exec(e) {
    if (!dev || e.fence == VK_NULL_HANDLE) {
      is_completed = true;
    }
  }

  ~VulkanEventImpl() {
    if (device && exec.fence != VK_NULL_HANDLE) {
      vkWaitForFences(device->device, 1, &exec.fence, VK_TRUE, UINT64_MAX);
      vkDestroyFence(device->device, exec.fence, nullptr);
      if (exec.command_buffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device->device, device->command_pool, 1, &exec.command_buffer);
      }
    }
  }

  void Await() {
    if (is_completed) return;
    if (device && exec.fence != VK_NULL_HANDLE) {
      vkWaitForFences(device->device, 1, &exec.fence, VK_TRUE, UINT64_MAX);
      is_completed = true;
    }
  }

  bool IsReady() {
    if (is_completed) return true;
    if (device && exec.fence != VK_NULL_HANDLE) {
      VkResult res = vkGetFenceStatus(device->device, exec.fence);
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
