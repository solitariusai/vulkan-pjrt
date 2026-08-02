#include "vulkan_client.h"

namespace vulkan_pjrt {

VulkanClientImpl::VulkanClientImpl() {
  device = std::make_unique<VulkanDevice>();
  device->Initialize(0);
}

VulkanClientImpl::~VulkanClientImpl() {}

PJRT_Device* VulkanClientImpl::GetDeviceHandle() const {
  return nullptr;
}

}  // namespace vulkan_pjrt
