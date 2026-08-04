#include "vulkan_backend.h"
#include <cstring>
#include <iostream>
#include <algorithm>

namespace vulkan_pjrt {

VulkanDevice::VulkanDevice() {}

VulkanDevice::~VulkanDevice() {
  if (device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device);
    
    for (auto& block : pool_blocks) {
      if (block.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, block.memory, nullptr);
      }
    }
    pool_blocks.clear();

    if (command_pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device, command_pool, nullptr);
    }
    vkDestroyDevice(device, nullptr);
  }
  if (instance != VK_NULL_HANDLE) {
    vkDestroyInstance(instance, nullptr);
  }
}

void VulkanDevice::Initialize(int device_index) {
  id = device_index;

  // 1. Create Vulkan Instance
  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Vulkan PJRT Backend";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "VulkanPJRT";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_1;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;

  VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
  if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Vulkan instance! Code: " + std::to_string(result));
  }

  // 2. Enumerate Physical Devices
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
  if (device_count == 0) {
    throw std::runtime_error("Failed to find GPUs with Vulkan support!");
  }

  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

  if (device_index < 0 || device_index >= static_cast<int>(device_count)) {
    device_index = 0;
  }
  physical_device = devices[device_index];

  VkPhysicalDeviceProperties device_properties;
  vkGetPhysicalDeviceProperties(physical_device, &device_properties);
  device_name = device_properties.deviceName;
  vendor_id = device_properties.vendorID;
  device_id = device_properties.deviceID;

  // 3. Find Compute Queue Family
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

  int compute_idx = -1;
  for (uint32_t i = 0; i < queue_family_count; ++i) {
    if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
      compute_idx = i;
      break;
    }
  }

  if (compute_idx == -1) {
    throw std::runtime_error("Vulkan device does not support compute queues!");
  }
  compute_queue_family_index = static_cast<uint32_t>(compute_idx);

  // 4. Create Logical Device
  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_create_info{};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = compute_queue_family_index;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;

  VkPhysicalDeviceFeatures device_features{};

  VkDeviceCreateInfo device_info{};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pQueueCreateInfos = &queue_create_info;
  device_info.queueCreateInfoCount = 1;
  device_info.pEnabledFeatures = &device_features;

  result = vkCreateDevice(physical_device, &device_info, nullptr, &device);
  if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Vulkan logical device!");
  }

  vkGetDeviceQueue(device, compute_queue_family_index, 0, &compute_queue);

  // 5. Create Command Pool
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = compute_queue_family_index;


  if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Vulkan command pool!");
  }
}

uint32_t VulkanDevice::FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
  VkPhysicalDeviceMemoryProperties mem_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
    if ((type_filter & (1 << i)) &&
        (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
    if (type_filter & (1 << i)) {
      return i;
    }
  }
  throw std::runtime_error("Failed to find suitable Vulkan memory type!");
}

void VulkanDevice::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                VkMemoryPropertyFlags properties, VkBuffer& buffer,
                                VkDeviceMemory& buffer_memory, VkDeviceSize& memory_offset, VkDeviceSize& allocated_size) const {
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size > 0 ? size : 4; // Ensure non-zero size
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create VkBuffer!");
  }

  VkMemoryRequirements mem_reqs;
  vkGetBufferMemoryRequirements(device, buffer, &mem_reqs);
  
  VkDeviceSize alignment = std::max(mem_reqs.alignment, (VkDeviceSize)256);
  allocated_size = (mem_reqs.size + alignment - 1) / alignment * alignment;
  
  uint32_t mem_type = FindMemoryType(mem_reqs.memoryTypeBits, properties);

  std::lock_guard<std::mutex> lock(pool_mutex);
  for (auto& block : pool_blocks) {
    if (block.memory_type_index == mem_type) {
      for (size_t i = 0; i < block.chunks.size(); ++i) {
        if (block.chunks[i].is_free && block.chunks[i].size >= allocated_size) {
          if (block.chunks[i].size > allocated_size) {
            block.chunks.push_back({block.chunks[i].offset + allocated_size, block.chunks[i].size - allocated_size, true});
            block.chunks[i].size = allocated_size;
          }
          block.chunks[i].is_free = false;
          buffer_memory = block.memory;
          memory_offset = block.chunks[i].offset;
          vkBindBufferMemory(device, buffer, buffer_memory, memory_offset);
          return;
        }
      }
    }
  }

  VkDeviceSize block_size = std::max((VkDeviceSize)64 * 1024 * 1024, allocated_size);
  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = block_size;
  alloc_info.memoryTypeIndex = mem_type;

  std::cout << "[Vulkan Memory Pool] Allocating new block of size: " << block_size << "\n";
  VkDeviceMemory new_memory;
  if (vkAllocateMemory(device, &alloc_info, nullptr, &new_memory) != VK_SUCCESS) {
    vkDestroyBuffer(device, buffer, nullptr);
    throw std::runtime_error("Failed to allocate pool VkDeviceMemory!");
  }

  PoolBlock new_block;
  new_block.memory = new_memory;
  new_block.total_size = block_size;
  new_block.memory_type_index = mem_type;
  
  new_block.chunks.push_back({0, allocated_size, false});
  if (block_size > allocated_size) {
    new_block.chunks.push_back({allocated_size, block_size - allocated_size, true});
  }
  
  pool_blocks.push_back(new_block);
  buffer_memory = new_memory;
  memory_offset = 0;
  vkBindBufferMemory(device, buffer, buffer_memory, memory_offset);
}

void VulkanDevice::FreeMemory(VkDeviceMemory memory, VkDeviceSize memory_offset, VkDeviceSize allocated_size) const {
  if (memory == VK_NULL_HANDLE) return;
  std::lock_guard<std::mutex> lock(pool_mutex);
  for (auto& block : pool_blocks) {
    if (block.memory == memory) {
      for (auto& chunk : block.chunks) {
        if (chunk.offset == memory_offset) {
          chunk.is_free = true;
          return;
        }
      }
    }
  }
}

VkCommandBuffer VulkanDevice::BeginSingleTimeCommands() const {
  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandPool = command_pool;
  alloc_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer;
  vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(command_buffer, &begin_info);
  return command_buffer;
}

void VulkanDevice::EndSingleTimeCommands(VkCommandBuffer command_buffer) const {
  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence;
  vkCreateFence(device, &fence_info, nullptr, &fence);

  vkQueueSubmit(compute_queue, 1, &submit_info, fence);
  vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

  vkDestroyFence(device, fence, nullptr);
  vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

AsyncExecution VulkanDevice::AsyncEndSingleTimeCommands(VkCommandBuffer command_buffer) const {
  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence;
  vkCreateFence(device, &fence_info, nullptr, &fence);

  vkQueueSubmit(compute_queue, 1, &submit_info, fence);

  AsyncExecution exec;
  exec.fence = fence;
  exec.command_buffer = command_buffer;
  return exec;
}

void VulkanDevice::CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) const {
  if (size == 0) return;
  VkCommandBuffer command_buffer = BeginSingleTimeCommands();
  VkBufferCopy copy_region{};
  copy_region.size = size;
  vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);
  EndSingleTimeCommands(command_buffer);
}

}  // namespace vulkan_pjrt
