#ifndef VULKAN_CLIENT_H_
#define VULKAN_CLIENT_H_

#include "pjrt_c_api.h"
#include "vulkan_backend.h"
#include "vulkan_buffer.h"
#include "vulkan_executable.h"
#include "vulkan_event.h"
#include <vector>
#include <memory>
#include <string>

struct PJRT_Device;

struct PJRT_MemoryImpl : public PJRT_Memory {
  int id = 0;
  std::string kind = "device";
  std::string debug_string = "Vulkan Device Memory";
  PJRT_Device* device = nullptr;
};

struct PJRT_Device {
  const vulkan_pjrt::VulkanDevice* device = nullptr;
  std::string device_kind = "Vulkan Device";
  std::string debug_string = "Vulkan GPU";
  std::unique_ptr<PJRT_MemoryImpl> memory;
  std::vector<PJRT_Memory*> memory_ptrs;
};

struct PJRT_TopologyDescription {
  struct PJRT_Client* client = nullptr;
  std::vector<PJRT_DeviceDescription*> device_descriptions;
};

namespace vulkan_pjrt {

class VulkanClientImpl {
 public:
  std::unique_ptr<VulkanDevice> device;
  std::string platform_name{"vulkan"};
  int process_index{0};

  VulkanClientImpl();
  ~VulkanClientImpl();

  PJRT_Device* GetDeviceHandle() const;
};

}  // namespace vulkan_pjrt

struct PJRT_Client {
  std::unique_ptr<vulkan_pjrt::VulkanClientImpl> impl;
  PJRT_Device device_handle;
  std::vector<PJRT_Device*> device_ptrs;
  std::vector<PJRT_Memory*> memory_ptrs;
  std::unique_ptr<PJRT_TopologyDescription> topology;
  std::string platform_name = "vulkan";
  std::string platform_version = "Vulkan PJRT 0.1";
};

#endif  // VULKAN_CLIENT_H_
