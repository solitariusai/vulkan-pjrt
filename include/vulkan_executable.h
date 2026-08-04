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

struct ComputeNode {
    OpSpec op_spec;
    std::vector<int> input_indices; // <0 means global input, >=0 means intermediate buffer
    std::vector<int> output_indices; // >=0 means intermediate/global output buffer
    std::vector<int64_t> out_dims;
    
    VkShaderModule shader_module{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptor_set_layout{VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
    VkPipeline compute_pipeline{VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};
    VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
};

class VulkanExecutableImpl {
 public:
  const VulkanDevice* device{nullptr};
  PJRT_Device* device_ptr{nullptr};

  std::vector<ComputeNode> nodes;
  std::vector<std::pair<int64_t, std::vector<int64_t>>> intermediate_buffers; // size, dims

  size_t num_inputs{0};
  size_t num_outputs{0};

  OpSpec op_spec;
  std::vector<int64_t> output_dims;
  PJRT_Buffer_Type output_type{PJRT_Buffer_Type_F32};

  VulkanExecutableImpl(const VulkanDevice* dev, PJRT_Device* dev_ptr, const std::string& code, const std::string& format);
  ~VulkanExecutableImpl();

  std::pair<std::vector<PJRT_Buffer*>, AsyncExecution> Execute(PJRT_Buffer* const* arguments, size_t num_args);
};

}  // namespace vulkan_pjrt

struct PJRT_LoadedExecutable {
  std::unique_ptr<vulkan_pjrt::VulkanExecutableImpl> impl;
};

struct PJRT_Executable {
  vulkan_pjrt::VulkanExecutableImpl* impl{nullptr};
};

#endif  // VULKAN_EXECUTABLE_H_
