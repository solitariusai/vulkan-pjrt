#include "vulkan_executable.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace vulkan_pjrt {

static void ParseCodePayload(const std::string& code, OpSpec& spec, size_t& num_inputs, size_t& num_outputs) {
  spec.op_type = "add";
  spec.dtype = "float";
  num_inputs = 2;
  num_outputs = 1;

  if (code.find("op:sub") != std::string::npos) spec.op_type = "sub";
  else if (code.find("op:mul") != std::string::npos) spec.op_type = "mul";
  else if (code.find("op:div") != std::string::npos) spec.op_type = "div";
  else if (code.find("op:relu") != std::string::npos) { spec.op_type = "relu"; num_inputs = 1; }
  else if (code.find("op:copy") != std::string::npos) { spec.op_type = "copy"; num_inputs = 1; }
  else if (code.find("op:scale") != std::string::npos) {
    spec.op_type = "scale";
    num_inputs = 1;
    size_t pos = code.find("val:");
    if (pos != std::string::npos) {
      spec.scalar_val = std::stof(code.substr(pos + 4));
    }
  } else if (code.find("op:matmul") != std::string::npos) {
    spec.op_type = "matmul";
    num_inputs = 2;
    size_t pos_m = code.find("M:");
    if (pos_m != std::string::npos) spec.M = std::stoi(code.substr(pos_m + 2));
    size_t pos_n = code.find("N:");
    if (pos_n != std::string::npos) spec.N = std::stoi(code.substr(pos_n + 2));
    size_t pos_k = code.find("K:");
    if (pos_k != std::string::npos) spec.K = std::stoi(code.substr(pos_k + 2));
  }
}

VulkanExecutableImpl::VulkanExecutableImpl(const VulkanDevice* dev, const std::string& code, const std::string& format)
    : device(dev) {
  if (code.empty() || !dev || dev->device == VK_NULL_HANDLE) {
    return;
  }
  try {
    std::string glsl_source;
    if (format == "glsl" || code.find("#version") != std::string::npos) {
      glsl_source = code;
      num_inputs = 2;
      num_outputs = 1;
    } else {
      ParseCodePayload(code, op_spec, num_inputs, num_outputs);
      glsl_source = ShaderCompiler::GenerateComputeShader(op_spec, num_inputs, num_outputs);
    }

    std::vector<uint32_t> spirv = ShaderCompiler::CompileGLSLToSPIRV(glsl_source);
    if (spirv.empty()) return;

    // 1. Create Shader Module
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = spirv.size() * sizeof(uint32_t);
    create_info.pCode = spirv.data();

    if (vkCreateShaderModule(dev->device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
      return;
    }

  // 2. Create Descriptor Set Layout
  size_t total_bindings = num_inputs + num_outputs;
  std::vector<VkDescriptorSetLayoutBinding> bindings(total_bindings);
  for (size_t i = 0; i < total_bindings; ++i) {
    bindings[i].binding = static_cast<uint32_t>(i);
    bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[i].pImmutableSamplers = nullptr;
  }

  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(total_bindings);
  layout_info.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(device->device, &layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create descriptor set layout!");
  }

  // 3. Create Pipeline Layout
  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

  if (vkCreatePipelineLayout(device->device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create pipeline layout!");
  }

  // 4. Create Compute Pipeline
  VkComputePipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipeline_info.stage.module = shader_module;
  pipeline_info.stage.pName = "main";
  pipeline_info.layout = pipeline_layout;

  if (vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &compute_pipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Vulkan compute pipeline!");
  }

  // 5. Create Descriptor Pool
  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_size.descriptorCount = static_cast<uint32_t>(total_bindings * 16);

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 16;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;

  if (vkCreateDescriptorPool(device->device, &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create descriptor pool!");
  }
  } catch (const std::exception& e) {
    std::cerr << "[Vulkan PJRT Warning] Shader compilation skipped: " << e.what() << std::endl;
  }
}

VulkanExecutableImpl::~VulkanExecutableImpl() {
  if (device && device->device != VK_NULL_HANDLE) {
    if (descriptor_pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device->device, descriptor_pool, nullptr);
    if (compute_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device->device, compute_pipeline, nullptr);
    if (pipeline_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device->device, pipeline_layout, nullptr);
    if (descriptor_set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device->device, descriptor_set_layout, nullptr);
    if (shader_module != VK_NULL_HANDLE) vkDestroyShaderModule(device->device, shader_module, nullptr);
  }
}

std::vector<PJRT_Buffer*> VulkanExecutableImpl::Execute(PJRT_Buffer* const* arguments, size_t num_args) {
  if (num_args < num_inputs) {
    throw std::runtime_error("Insufficient arguments provided to Vulkan executable!");
  }

  // Determine output shape from input arguments
  std::vector<int64_t> out_dims;
  PJRT_Buffer_Type out_type = arguments[0]->impl->element_type;

  if (op_spec.op_type == "matmul") {
    int64_t M = op_spec.M > 0 ? op_spec.M : (arguments[0]->impl->dims.empty() ? 1 : arguments[0]->impl->dims[0]);
    int64_t N = op_spec.N > 0 ? op_spec.N : (arguments[1]->impl->dims.size() > 1 ? arguments[1]->impl->dims[1] : 1);
    out_dims = {M, N};
  } else {
    out_dims = arguments[0]->impl->dims;
  }

  size_t total_elements = 1;
  for (auto d : out_dims) total_elements *= d;

  // Create Output Buffer(s)
  std::vector<PJRT_Buffer*> output_buffers;
  for (size_t i = 0; i < num_outputs; ++i) {
    PJRT_Buffer* out_buf = new PJRT_Buffer();
    auto buf_impl = std::make_unique<VulkanBufferImpl>(device, out_type, out_dims.data(), out_dims.size());
    if (num_args > 0 && arguments[0] && arguments[0]->impl) {
      buf_impl->device_ptr = arguments[0]->impl->device_ptr;
      buf_impl->memory_ptr = arguments[0]->impl->memory_ptr;
    }
    out_buf->impl = std::move(buf_impl);
    output_buffers.push_back(out_buf);
  }

  // Allocate Descriptor Set
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &descriptor_set_layout;

  VkDescriptorSet descriptor_set;
  if (vkAllocateDescriptorSets(device->device, &alloc_info, &descriptor_set) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate descriptor set!");
  }

  // Bind Storage Buffers
  size_t total_bindings = num_inputs + num_outputs;
  std::vector<VkDescriptorBufferInfo> buffer_infos(total_bindings);
  std::vector<VkWriteDescriptorSet> descriptor_writes(total_bindings);

  for (size_t i = 0; i < num_inputs; ++i) {
    buffer_infos[i].buffer = arguments[i]->impl->vk_buffer;
    buffer_infos[i].offset = 0;
    buffer_infos[i].range = arguments[i]->impl->size_in_bytes;

    descriptor_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[i].dstSet = descriptor_set;
    descriptor_writes[i].dstBinding = static_cast<uint32_t>(i);
    descriptor_writes[i].descriptorCount = 1;
    descriptor_writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_writes[i].pBufferInfo = &buffer_infos[i];
  }

  for (size_t i = 0; i < num_outputs; ++i) {
    size_t idx = num_inputs + i;
    buffer_infos[idx].buffer = output_buffers[i]->impl->vk_buffer;
    buffer_infos[idx].offset = 0;
    buffer_infos[idx].range = output_buffers[i]->impl->size_in_bytes;

    descriptor_writes[idx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[idx].dstSet = descriptor_set;
    descriptor_writes[idx].dstBinding = static_cast<uint32_t>(idx);
    descriptor_writes[idx].descriptorCount = 1;
    descriptor_writes[idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_writes[idx].pBufferInfo = &buffer_infos[idx];
  }

  vkUpdateDescriptorSets(device->device, static_cast<uint32_t>(total_bindings), descriptor_writes.data(), 0, nullptr);

  // Dispatch Compute Work
  VkCommandBuffer command_buffer = device->BeginSingleTimeCommands();

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

  uint32_t group_count_x = static_cast<uint32_t>((total_elements + 63) / 64);
  vkCmdDispatch(command_buffer, std::max(1u, group_count_x), 1, 1);

  device->EndSingleTimeCommands(command_buffer);

  vkFreeDescriptorSets(device->device, descriptor_pool, 1, &descriptor_set);

  return output_buffers;
}

}  // namespace vulkan_pjrt
