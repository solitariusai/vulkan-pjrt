#ifndef VULKAN_BACKEND_H_
#define VULKAN_BACKEND_H_

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <stdexcept>
#include <iostream>

namespace vulkan_pjrt {

struct AsyncExecution {
  VkFence fence{VK_NULL_HANDLE};
  VkCommandBuffer command_buffer{VK_NULL_HANDLE};
};

class VulkanDevice {
 public:
  VkInstance instance{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device{VK_NULL_HANDLE};
  VkDevice device{VK_NULL_HANDLE};
  VkQueue compute_queue{VK_NULL_HANDLE};
  uint32_t compute_queue_family_index{0};
  VkCommandPool command_pool{VK_NULL_HANDLE};

  std::string device_name;
  uint32_t vendor_id{0};
  uint32_t device_id{0};
  int id{0};

  mutable std::mutex pool_mutex;
  
  struct Chunk {
      VkDeviceSize offset;
      VkDeviceSize size;
      bool is_free;
  };
  struct PoolBlock {
      VkDeviceMemory memory;
      VkDeviceSize total_size;
      uint32_t memory_type_index;
      std::vector<Chunk> chunks;
  };
  mutable std::vector<PoolBlock> pool_blocks;

  VulkanDevice();
  ~VulkanDevice();

  void Initialize(int device_index = 0);
  uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const;

  void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkBuffer& buffer,
                    VkDeviceMemory& buffer_memory, VkDeviceSize& memory_offset, VkDeviceSize& allocated_size) const;

  void FreeMemory(VkDeviceMemory memory, VkDeviceSize memory_offset, VkDeviceSize allocated_size) const;

  void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) const;

  VkCommandBuffer BeginSingleTimeCommands() const;
  void EndSingleTimeCommands(VkCommandBuffer command_buffer) const;
  AsyncExecution AsyncEndSingleTimeCommands(VkCommandBuffer command_buffer) const;
};

}  // namespace vulkan_pjrt

#endif  // VULKAN_BACKEND_H_
