#include "vulkan_executable.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <dialectiny/dialectiny.hpp>

namespace vulkan_pjrt {

static void ParseCodePayload(const std::string& code, OpSpec& spec, size_t& num_inputs, size_t& num_outputs) {
    const char* env_op = std::getenv("VULKAN_PJRT_TEST_OP");
    if (env_op && env_op[0] != '\0') {
        std::string op = env_op;
        if (op == "add" || op == "multiply" || op == "subtract" || op == "divide" || op == "maximum" || op == "minimum" || op == "matmul") {
            spec.op_type = op;
            if (op == "multiply") spec.op_type = "mul";
            if (op == "subtract") spec.op_type = "sub";
            if (op == "divide") spec.op_type = "div";
            if (op == "maximum") spec.op_type = "max";
            if (op == "minimum") spec.op_type = "min";
            num_inputs = 2;
        } else {
            spec.op_type = op;
            num_inputs = 1;
        }
        num_outputs = 1;
        std::cout << "[Vulkan JIT] Injected op_type = " << spec.op_type << " from environment." << std::endl;
        return;
    }

    FILE* f = fopen("bytecode_dump.bin", "wb");
    if (f) {
        fwrite(code.data(), 1, code.size(), f);
        fclose(f);
    }

    dialectiny::BytecodeParser parser;
    dialectiny::Graph graph = parser.Parse(code);
    dialectiny::FusedPattern pattern = parser.MatchFusion(graph);

    if (!pattern.primary_op.empty()) {
        spec.op_type = pattern.primary_op;
        spec.epilogue_ops = pattern.epilogue_ops;
        if (!pattern.scalar_vals.empty()) {
            spec.scalar_val = pattern.scalar_vals[0];
        }
        num_inputs = pattern.total_inputs_required;
    } else {
        // Fallback: no known op found — treat as identity/copy
        spec.op_type = "copy";
        num_inputs = 1;
    }

    num_outputs = 1; // All our ops produce exactly one output tensor
    std::cout << "[Vulkan JIT] Dialectiny parsed op_type=" << spec.op_type
              << " inputs=" << num_inputs << "\n";
}

VulkanExecutableImpl::VulkanExecutableImpl(const VulkanDevice* dev, PJRT_Device* dev_ptr, const std::string& code, const std::string& format)
    : device(dev), device_ptr(dev_ptr) {
  if (code.empty() || !dev || dev->device == VK_NULL_HANDLE) {
    return;
  }

  try {
    ParseCodePayload(code, op_spec, num_inputs, num_outputs); 
    
    // Standard single node fallback
    ComputeNode n0;
    n0.op_spec = op_spec;
    for (size_t i=0; i<num_inputs; ++i) n0.input_indices.push_back(-1 - i);
    for (size_t i=0; i<num_outputs; ++i) n0.output_indices.push_back(1000 + i);
    nodes.push_back(n0);
    
    // Compile all nodes
    for (auto& node : nodes) {
        std::string glsl_source = ShaderCompiler::GenerateComputeShader(node.op_spec, node.input_indices.size(), node.output_indices.size());
        std::vector<uint32_t> spirv = ShaderCompiler::CompileGLSLToSPIRV(glsl_source);
        if (spirv.empty()) continue;
        
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = spirv.size() * sizeof(uint32_t);
        create_info.pCode = spirv.data();
        vkCreateShaderModule(device->device, &create_info, nullptr, &node.shader_module);
        
        size_t total_bindings = node.input_indices.size() + node.output_indices.size();
        std::vector<VkDescriptorSetLayoutBinding> bindings(total_bindings);
        for (size_t i = 0; i < total_bindings; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();
        vkCreateDescriptorSetLayout(device->device, &layout_info, nullptr, &node.descriptor_set_layout);
        
        VkPushConstantRange push_constant;
        push_constant.offset = 0;
        push_constant.size = 12;
        push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        
        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &node.descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &push_constant;
        
        vkCreatePipelineLayout(device->device, &pipeline_layout_info, nullptr, &node.pipeline_layout);
        
        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_info.stage.module = node.shader_module;
        pipeline_info.stage.pName = "main";
        pipeline_info.layout = node.pipeline_layout;
        
        vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &node.compute_pipeline);
    }
  } catch (const std::exception& e) {
    std::cerr << "[Vulkan JIT] Exception during compilation: " << e.what() << std::endl;
  }
}

VulkanExecutableImpl::~VulkanExecutableImpl() {
  if (device) {
    for (auto& node : nodes) {
        if (node.compute_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device->device, node.compute_pipeline, nullptr);
        }
        if (node.pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device->device, node.pipeline_layout, nullptr);
        }
        if (node.descriptor_set_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device->device, node.descriptor_set_layout, nullptr);
        }
        if (node.shader_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device->device, node.shader_module, nullptr);
        }
        if (node.descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device->device, node.descriptor_pool, nullptr);
        }
    }
  }
}

std::pair<std::vector<PJRT_Buffer*>, AsyncExecution> VulkanExecutableImpl::Execute(PJRT_Buffer* const* arguments, size_t num_args) {
  std::vector<PJRT_Buffer*> output_buffers;
  size_t actual_inputs = std::min((size_t)num_inputs, num_args);
  
  for (size_t i = 0; i < num_outputs; ++i) {
    PJRT_Buffer* out_buf = new PJRT_Buffer();
    std::vector<int64_t> out_dims;
    
    if (op_spec.op_type == "matmul") {
        if (actual_inputs > 0) {
            if (arguments[0]->impl->dims.size() >= 2) out_dims.push_back(arguments[0]->impl->dims[0]);
            if (arguments[1]->impl->dims.size() >= 2) out_dims.push_back(arguments[1]->impl->dims[1]);
        }
    } else {
        if (actual_inputs > 0) {
            out_dims = arguments[0]->impl->dims;
        }
    }
    
    if (out_dims.empty()) out_dims = {1};
    
    auto buf_impl = std::make_unique<VulkanBufferImpl>(device, PJRT_Buffer_Type_F32, out_dims.data(), out_dims.size());
    if (num_args > 0 && arguments[0] && arguments[0]->impl) {
        buf_impl->device_ptr = arguments[0]->impl->device_ptr;
        buf_impl->memory_ptr = arguments[0]->impl->memory_ptr;
    } else if (device_ptr) {
        buf_impl->device_ptr = device_ptr;
    }
    out_buf->impl = std::move(buf_impl);
    output_buffers.push_back(out_buf);
  }

  std::vector<std::unique_ptr<VulkanBufferImpl>> int_buffers;
  for (auto& ib : intermediate_buffers) {
      auto b = std::make_unique<VulkanBufferImpl>(device, PJRT_Buffer_Type_F32, ib.second.data(), ib.second.size());
      b->device_ptr = device_ptr;
      int_buffers.push_back(std::move(b));
  }
  
  VkCommandBuffer command_buffer = device->BeginSingleTimeCommands();
  
  for (auto& node : nodes) {
      if (node.compute_pipeline == VK_NULL_HANDLE) continue;
      
      size_t total_bindings = node.input_indices.size() + node.output_indices.size();
      
      VkDescriptorPoolSize pool_size{};
      pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      pool_size.descriptorCount = static_cast<uint32_t>(total_bindings);
      
      VkDescriptorPoolCreateInfo pool_info{};
      pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      pool_info.poolSizeCount = 1;
      pool_info.pPoolSizes = &pool_size;
      pool_info.maxSets = 1;
      
      vkCreateDescriptorPool(device->device, &pool_info, nullptr, &node.descriptor_pool);
      
      VkDescriptorSetAllocateInfo alloc_info{};
      alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      alloc_info.descriptorPool = node.descriptor_pool;
      alloc_info.descriptorSetCount = 1;
      alloc_info.pSetLayouts = &node.descriptor_set_layout;
      
      vkAllocateDescriptorSets(device->device, &alloc_info, &node.descriptor_set);
      
      std::vector<VkDescriptorBufferInfo> buffer_infos(total_bindings);
      std::vector<VkWriteDescriptorSet> descriptor_writes(total_bindings);
      
      for (size_t i = 0; i < node.input_indices.size(); ++i) {
          int arg_idx = -node.input_indices[i] - 1;
          if (node.input_indices[i] < 0) {
              if (arg_idx >= 0 && arg_idx < actual_inputs) {
                  buffer_infos[i].buffer = arguments[arg_idx]->impl->vk_buffer;
              } else {
                  buffer_infos[i].buffer = VK_NULL_HANDLE;
              }
          } else {
              buffer_infos[i].buffer = int_buffers[node.input_indices[i]]->vk_buffer;
          }
          buffer_infos[i].offset = 0;
          buffer_infos[i].range = VK_WHOLE_SIZE;
          
          descriptor_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          descriptor_writes[i].dstSet = node.descriptor_set;
          descriptor_writes[i].dstBinding = i;
          descriptor_writes[i].dstArrayElement = 0;
          descriptor_writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          descriptor_writes[i].descriptorCount = 1;
          descriptor_writes[i].pBufferInfo = &buffer_infos[i];
      }
      
      for (size_t i = 0; i < node.output_indices.size(); ++i) {
          size_t bind_idx = node.input_indices.size() + i;
          int out_idx = node.output_indices[i] - 1000;
          if (node.output_indices[i] >= 1000) {
              if (out_idx >= 0 && out_idx < num_outputs) {
                  buffer_infos[bind_idx].buffer = output_buffers[out_idx]->impl->vk_buffer;
              } else {
                  buffer_infos[bind_idx].buffer = VK_NULL_HANDLE;
              }
          } else {
              buffer_infos[bind_idx].buffer = int_buffers[node.output_indices[i]]->vk_buffer;
          }
          buffer_infos[bind_idx].offset = 0;
          buffer_infos[bind_idx].range = VK_WHOLE_SIZE;
          
          descriptor_writes[bind_idx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          descriptor_writes[bind_idx].dstSet = node.descriptor_set;
          descriptor_writes[bind_idx].dstBinding = bind_idx;
          descriptor_writes[bind_idx].dstArrayElement = 0;
          descriptor_writes[bind_idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          descriptor_writes[bind_idx].descriptorCount = 1;
          descriptor_writes[bind_idx].pBufferInfo = &buffer_infos[bind_idx];
      }
      
      vkUpdateDescriptorSets(device->device, static_cast<uint32_t>(total_bindings), descriptor_writes.data(), 0, nullptr);
      
      vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, node.compute_pipeline);
      vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, node.pipeline_layout, 0, 1, &node.descriptor_set, 0, nullptr);
      
      uint32_t M = 1, N = 1, K = 1;
      if (node.op_spec.op_type.find("matmul") != std::string::npos) {
          if (!output_buffers.empty() && output_buffers[0]->impl->dims.size() >= 2) {
              M = output_buffers[0]->impl->dims[0];
              N = output_buffers[0]->impl->dims[1];
          }
          if (actual_inputs > 0 && arguments[0]->impl->dims.size() >= 2) {
              K = arguments[0]->impl->dims[1];
          }
      } else {
          size_t total = 1;
          if (!output_buffers.empty() && !output_buffers[0]->impl->dims.empty()) {
              for (auto d : output_buffers[0]->impl->dims) total *= d;
          }
          M = total;
      }
      
      uint32_t push_constants[] = {M, N, K};
      vkCmdPushConstants(command_buffer, node.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 12, push_constants);
      
      if (node.op_spec.op_type.find("matmul") != std::string::npos) {
          vkCmdDispatch(command_buffer, (N + 31) / 32, (M + 31) / 32, 1);
      } else if (node.op_spec.op_type == "transpose") {
          vkCmdDispatch(command_buffer, (N + 15) / 16, (M + 15) / 16, 1);
      } else {
          vkCmdDispatch(command_buffer, (M + 63) / 64, 1, 1);
      }
      
      VkMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
  }
  
  AsyncExecution exec = device->AsyncEndSingleTimeCommands(command_buffer);
  
  return {output_buffers, exec};
}

}  // namespace vulkan_pjrt
