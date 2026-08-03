#include "vulkan_executable.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace vulkan_pjrt {

static void ParseCodePayload(const std::string& code, OpSpec& spec, size_t& num_inputs, size_t& num_outputs) {
  spec.op_type = "add";
  spec.dtype = "float";
  num_inputs = 2;
  num_outputs = 1;

  if (code.find("StableHLO") != std::string::npos || code.find("vhlo") != std::string::npos || code.find("MLIR") != std::string::npos) {
    if (code.find("stage") != std::string::npos || code.find("args[0]") != std::string::npos || code.find("convert") != std::string::npos) {
      spec.op_type = "copy";
      num_inputs = 1;
      num_outputs = 1;
      return;
    }
  }

  if (code.find("stablehlo.subtract") != std::string::npos || code.find("op:sub") != std::string::npos) {
    spec.op_type = "sub";
  } else if (code.find("stablehlo.multiply") != std::string::npos || code.find("op:mul") != std::string::npos) {
    spec.op_type = "mul";
  } else if (code.find("stablehlo.divide") != std::string::npos || code.find("op:div") != std::string::npos) {
    spec.op_type = "div";
  } else if (code.find("stablehlo.add") != std::string::npos || code.find("op:add") != std::string::npos) {
    spec.op_type = "add";
  } else if (code.find("op:relu") != std::string::npos) {
    spec.op_type = "relu";
    num_inputs = 1;
  } else if (code.find("op:copy") != std::string::npos) {
    spec.op_type = "copy";
    num_inputs = 1;
  } else if (code.find("op:scale") != std::string::npos) {
    spec.op_type = "scale";
    num_inputs = 1;
    size_t pos = code.find("val:");
    if (pos != std::string::npos) {
      spec.scalar_val = std::stof(code.substr(pos + 4));
    }
  } else if (code.find("stablehlo.dot") != std::string::npos || code.find("dot_general") != std::string::npos || code.find("op:matmul") != std::string::npos) {
    spec.op_type = "matmul_fused";
    num_inputs = 2;

    std::vector<std::pair<size_t, std::string>> found_ops;
    auto check_op = [&](const std::string& pattern, const std::string& op_name) {
      size_t pos = code.find(pattern);
      if (pos != std::string::npos) found_ops.push_back({pos, op_name});
    };
    check_op("add_v1", "add");
    check_op("multiply_v1", "mul");
    check_op("maximum_v1", "relu");

    std::sort(found_ops.begin(), found_ops.end());
    for (const auto& p : found_ops) {
      spec.epilogue_ops.push_back(p.second);
      if (p.second == "add" || p.second == "mul") num_inputs++;
    }
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
  VkPushConstantRange push_constant;
  push_constant.offset = 0;
  push_constant.size = 12; // M, N, K (3 * 4 bytes)
  push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
  pipeline_layout_info.pushConstantRangeCount = 1;
  pipeline_layout_info.pPushConstantRanges = &push_constant;

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
    num_inputs = num_args;
  }

  // Determine output shape from input arguments
  std::vector<int64_t> out_dims;
  PJRT_Buffer_Type out_type = (num_args > 0 && arguments && arguments[0] && arguments[0]->impl) ? arguments[0]->impl->element_type : PJRT_Buffer_Type_F32;

  if (op_spec.op_type == "matmul" || op_spec.op_type == "matmul_add" || op_spec.op_type == "matmul_fused") {
    int64_t M = (num_args > 0 && arguments[0] && arguments[0]->impl && !arguments[0]->impl->dims.empty()) ? arguments[0]->impl->dims[0] : 1;
    int64_t N = (num_args > 1 && arguments[1] && arguments[1]->impl && arguments[1]->impl->dims.size() > 1) ? arguments[1]->impl->dims[1] : 1;
    out_dims = {M, N};
  } else {
    out_dims = (num_args > 0 && arguments && arguments[0] && arguments[0]->impl) ? arguments[0]->impl->dims : std::vector<int64_t>{};
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

  if (compute_pipeline == VK_NULL_HANDLE) {
    if (!output_buffers.empty() && output_buffers[0]->impl && num_args > 0 && arguments[0] && arguments[0]->impl) {
      void* out_map = nullptr;
      void* a_map = nullptr;
      void* b_map = nullptr;
      vkMapMemory(device->device, output_buffers[0]->impl->vk_memory, 0, output_buffers[0]->impl->size_in_bytes, 0, &out_map);
      vkMapMemory(device->device, arguments[0]->impl->vk_memory, 0, arguments[0]->impl->size_in_bytes, 0, &a_map);
      if (num_args > 1 && arguments[1] && arguments[1]->impl) {
        vkMapMemory(device->device, arguments[1]->impl->vk_memory, 0, arguments[1]->impl->size_in_bytes, 0, &b_map);
      }

      if (out_map && a_map) {
        if (op_spec.op_type == "copy" || num_args == 1) {
          std::memcpy(out_map, a_map, std::min(output_buffers[0]->impl->size_in_bytes, arguments[0]->impl->size_in_bytes));
        } else {
          float* out_ptr = static_cast<float*>(out_map);
          const float* a_ptr = static_cast<const float*>(a_map);
          const float* b_ptr = static_cast<const float*>(b_map);

          if (op_spec.op_type == "sub" && b_ptr) {
            for (size_t i = 0; i < total_elements; ++i) out_ptr[i] = a_ptr[i] - b_ptr[i];
          } else if (op_spec.op_type == "mul" && b_ptr) {
            for (size_t i = 0; i < total_elements; ++i) out_ptr[i] = a_ptr[i] * b_ptr[i];
          } else if (op_spec.op_type == "div" && b_ptr) {
            for (size_t i = 0; i < total_elements; ++i) out_ptr[i] = a_ptr[i] / b_ptr[i];
          } else if (b_ptr) {
            for (size_t i = 0; i < total_elements; ++i) out_ptr[i] = a_ptr[i] + b_ptr[i];
          }
        }
      }

      if (out_map) vkUnmapMemory(device->device, output_buffers[0]->impl->vk_memory);
      if (a_map) vkUnmapMemory(device->device, arguments[0]->impl->vk_memory);
      if (b_map && arguments[1] && arguments[1]->impl) vkUnmapMemory(device->device, arguments[1]->impl->vk_memory);
    }
    return output_buffers;
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

  struct PushConstants {
    uint32_t M, N, K;
  } pc = {1, 1, 1};

  if (op_spec.op_type == "matmul" || op_spec.op_type == "matmul_add" || op_spec.op_type == "matmul_fused") {
    pc.M = static_cast<uint32_t>(out_dims[0]);
    pc.N = static_cast<uint32_t>(out_dims[1]);
    pc.K = static_cast<uint32_t>((num_args > 0 && arguments[0]->impl->dims.size() > 1) ? arguments[0]->impl->dims[1] : 1);
    vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pc);

    uint32_t group_x = static_cast<uint32_t>((out_dims[1] + 15) / 16);
    uint32_t group_y = static_cast<uint32_t>((out_dims[0] + 15) / 16);
    vkCmdDispatch(command_buffer, std::max(1u, group_x), std::max(1u, group_y), 1);
  } else {
    vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pc);
    uint32_t group_count_x = static_cast<uint32_t>((total_elements + 63) / 64);
    vkCmdDispatch(command_buffer, std::max(1u, group_count_x), 1, 1);
  }

  device->EndSingleTimeCommands(command_buffer);

  vkFreeDescriptorSets(device->device, descriptor_pool, 1, &descriptor_set);

  return output_buffers;
}

}  // namespace vulkan_pjrt
