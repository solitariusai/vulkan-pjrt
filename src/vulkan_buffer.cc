#include "vulkan_buffer.h"
#include <cstring>
#include <numeric>
#include <iostream>

namespace vulkan_pjrt {

size_t GetElementTypeSize(PJRT_Buffer_Type type) {
  switch (type) {
    case PJRT_Buffer_Type_PRED:
    case PJRT_Buffer_Type_S8:
    case PJRT_Buffer_Type_U8:
      return 1;
    case PJRT_Buffer_Type_S16:
    case PJRT_Buffer_Type_U16:
    case PJRT_Buffer_Type_F16:
    case PJRT_Buffer_Type_BF16:
      return 2;
    case PJRT_Buffer_Type_S32:
    case PJRT_Buffer_Type_U32:
    case PJRT_Buffer_Type_F32:
      return 4;
    case PJRT_Buffer_Type_S64:
    case PJRT_Buffer_Type_U64:
    case PJRT_Buffer_Type_F64:
    case PJRT_Buffer_Type_C64:
      return 8;
    case PJRT_Buffer_Type_C128:
      return 16;
    default:
      return 4;
  }
}

VulkanBufferImpl::VulkanBufferImpl(const VulkanDevice* dev, PJRT_Buffer_Type type,
                                   const int64_t* dimensions, size_t num_dims)
    : device(dev), element_type(type) {
  size_t elem_size = GetElementTypeSize(type);
  size_t num_elements = 1;
  dims.assign(dimensions, dimensions + num_dims);

  if (num_dims == 0) {
    num_elements = 1;
  } else {
    for (size_t i = 0; i < num_dims; ++i) {
      num_elements *= dimensions[i];
    }
  }

  // Calculate row-major byte strides
  byte_strides.resize(num_dims);
  if (num_dims > 0) {
    byte_strides[num_dims - 1] = elem_size;
    for (int i = static_cast<int>(num_dims) - 2; i >= 0; --i) {
      byte_strides[i] = byte_strides[i + 1] * dimensions[i + 1];
    }
  }

  size_in_bytes = num_elements * elem_size;

  // Allocate GPU buffer (Device Local + Transfer Src/Dst + Storage Buffer)
  device->CreateBuffer(
      size_in_bytes,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      vk_buffer, vk_memory);
}

VulkanBufferImpl::~VulkanBufferImpl() {
  if (device && device->device != VK_NULL_HANDLE) {
    if (vk_buffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device->device, vk_buffer, nullptr);
    }
    if (vk_memory != VK_NULL_HANDLE) {
      vkFreeMemory(device->device, vk_memory, nullptr);
    }
  }
}

void VulkanBufferImpl::CopyFromHost(const void* host_data, size_t host_size_bytes) {
  if (host_size_bytes == 0 || !host_data) return;

  // Create staging buffer (Host Visible + Host Coherent)
  VkBuffer staging_buffer;
  VkDeviceMemory staging_memory;
  device->CreateBuffer(
      host_size_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      staging_buffer, staging_memory);

  void* mapped = nullptr;
  vkMapMemory(device->device, staging_memory, 0, host_size_bytes, 0, &mapped);
  std::memcpy(mapped, host_data, host_size_bytes);
  vkUnmapMemory(device->device, staging_memory);

  // Copy staging buffer -> GPU device buffer
  device->CopyBuffer(staging_buffer, vk_buffer, std::min(size_in_bytes, host_size_bytes));

  vkDestroyBuffer(device->device, staging_buffer, nullptr);
  vkFreeMemory(device->device, staging_memory, nullptr);
}

void VulkanBufferImpl::CopyToHost(void* host_data, size_t host_size_bytes) const {
  if (host_size_bytes == 0 || !host_data) return;

  size_t copy_size = std::min(size_in_bytes, host_size_bytes);

  // Create staging buffer (Host Visible + Host Coherent)
  VkBuffer staging_buffer;
  VkDeviceMemory staging_memory;
  device->CreateBuffer(
      copy_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      staging_buffer, staging_memory);

  // Copy GPU device buffer -> staging buffer
  device->CopyBuffer(vk_buffer, staging_buffer, copy_size);

  void* mapped = nullptr;
  vkMapMemory(device->device, staging_memory, 0, copy_size, 0, &mapped);
  std::memcpy(host_data, mapped, copy_size);
  vkUnmapMemory(device->device, staging_memory);

  vkDestroyBuffer(device->device, staging_buffer, nullptr);
  vkFreeMemory(device->device, staging_memory, nullptr);
}

}  // namespace vulkan_pjrt
