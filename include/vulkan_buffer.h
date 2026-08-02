#ifndef VULKAN_BUFFER_H_
#define VULKAN_BUFFER_H_

#include "pjrt_c_api.h"
#include "vulkan_backend.h"
#include "vulkan_event.h"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <memory>

namespace vulkan_pjrt {

size_t GetElementTypeSize(PJRT_Buffer_Type type);

class VulkanBufferImpl {
 public:
  const VulkanDevice* device{nullptr};
  PJRT_Device* device_ptr{nullptr};
  PJRT_Memory* memory_ptr{nullptr};
  VkBuffer vk_buffer{VK_NULL_HANDLE};
  VkDeviceMemory vk_memory{VK_NULL_HANDLE};

  PJRT_Buffer_Type element_type{PJRT_Buffer_Type_F32};
  std::vector<int64_t> dims;
  std::vector<int64_t> byte_strides;
  size_t size_in_bytes{0};

  VulkanBufferImpl(const VulkanDevice* dev, PJRT_Buffer_Type type,
                   const int64_t* dimensions, size_t num_dims);

  ~VulkanBufferImpl();

  void CopyFromHost(const void* host_data, size_t host_size_bytes);
  void CopyToHost(void* host_data, size_t host_size_bytes) const;
};

}  // namespace vulkan_pjrt

struct PJRT_Buffer {
  std::unique_ptr<vulkan_pjrt::VulkanBufferImpl> impl;
};

#endif  // VULKAN_BUFFER_H_
