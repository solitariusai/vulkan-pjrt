#ifndef VULKAN_EXECUTABLE_H_
#define VULKAN_EXECUTABLE_H_

#include "pjrt_c_api.h"
#include "vulkan_backend.h"
#include "vulkan_buffer.h"
#include "vulkan_compiler.h"
#include <vector>
#include <memory>
#include <string>

namespace vulkan_pjrt {

class VulkanExecutableImpl {
 public:
  const VulkanDevice* device{nullptr};

  VkShaderModule shader_module{VK_NULL_HANDLE};
  VkDescriptorSetLayout descriptor_set_layout{VK_NULL_HANDLE};
  VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
  VkPipeline compute_pipeline{VK_NULL_HANDLE};
  VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};

  size_t num_inputs{0};
  size_t num_outputs{0};

  OpSpec op_spec;
  std::vector<int64_t> output_dims;
  PJRT_Buffer_Type output_type{PJRT_Buffer_Type_F32};

  VulkanExecutableImpl(const VulkanDevice* dev, const std::string& code, const std::string& format);
  ~VulkanExecutableImpl();

  std::vector<PJRT_Buffer*> Execute(PJRT_Buffer* const* arguments, size_t num_args);
};

}  // namespace vulkan_pjrt

struct PJRT_LoadedExecutable {
  std::unique_ptr<vulkan_pjrt::VulkanExecutableImpl> impl;
};

#endif  // VULKAN_EXECUTABLE_H_
