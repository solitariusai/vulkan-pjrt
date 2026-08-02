#include "vulkan_compiler.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>

namespace vulkan_pjrt {

std::vector<uint32_t> ShaderCompiler::CompileGLSLToSPIRV(const std::string& glsl_source) {
  char temp_in[] = "/tmp/vulkan_pjrt_shader_XXXXXX.comp";
  int fd_in = mkstemps(temp_in, 5);
  if (fd_in == -1) {
    throw std::runtime_error("Failed to create temporary input file for glslc!");
  }
  write(fd_in, glsl_source.c_str(), glsl_source.size());
  close(fd_in);

  char temp_out[] = "/tmp/vulkan_pjrt_shader_XXXXXX.spv";
  int fd_out = mkstemps(temp_out, 4);
  if (fd_out == -1) {
    unlink(temp_in);
    throw std::runtime_error("Failed to create temporary output file for glslc!");
  }
  close(fd_out);

  std::string command = "glslc -fshader-stage=compute " + std::string(temp_in) + " -o " + std::string(temp_out);
  int ret = std::system(command.c_str());

  unlink(temp_in);

  if (ret != 0) {
    unlink(temp_out);
    throw std::runtime_error("glslc shader compilation failed for source:\n" + glsl_source);
  }

  std::ifstream spv_file(temp_out, std::ios::binary | std::ios::ate);
  if (!spv_file.is_open()) {
    unlink(temp_out);
    throw std::runtime_error("Failed to open compiled SPIR-V binary!");
  }

  std::streamsize size = spv_file.tellg();
  spv_file.seekg(0, std::ios::beg);

  std::vector<uint32_t> spirv(size / sizeof(uint32_t));
  if (!spv_file.read(reinterpret_cast<char*>(spirv.data()), size)) {
    unlink(temp_out);
    throw std::runtime_error("Failed to read SPIR-V binary content!");
  }

  spv_file.close();
  unlink(temp_out);

  return spirv;
}

std::string ShaderCompiler::GenerateComputeShader(const OpSpec& spec, size_t num_inputs, size_t num_outputs) {
  std::stringstream ss;
  ss << "#version 450\n";
  ss << "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n\n";

  std::string glsl_type = (spec.dtype == "int" || spec.dtype == "int32") ? "int" : "float";

  size_t binding_idx = 0;
  for (size_t i = 0; i < num_inputs; ++i) {
    ss << "layout(std430, binding = " << binding_idx++ << ") buffer Input" << i << " { " << glsl_type << " in" << i << "[]; };\n";
  }
  for (size_t i = 0; i < num_outputs; ++i) {
    ss << "layout(std430, binding = " << binding_idx++ << ") buffer Output" << i << " { " << glsl_type << " out" << i << "[]; };\n";
  }

  ss << "\nvoid main() {\n";
  ss << "  uint g_id = gl_GlobalInvocationID.x;\n";

  if (spec.op_type == "add") {
    ss << "  out0[g_id] = in0[g_id] + in1[g_id];\n";
  } else if (spec.op_type == "sub") {
    ss << "  out0[g_id] = in0[g_id] - in1[g_id];\n";
  } else if (spec.op_type == "mul") {
    ss << "  out0[g_id] = in0[g_id] * in1[g_id];\n";
  } else if (spec.op_type == "div") {
    ss << "  out0[g_id] = in0[g_id] / in1[g_id];\n";
  } else if (spec.op_type == "relu") {
    ss << "  out0[g_id] = max(in0[g_id], " << glsl_type << "(0.0));\n";
  } else if (spec.op_type == "scale") {
    ss << "  out0[g_id] = in0[g_id] * " << glsl_type << "(" << spec.scalar_val << ");\n";
  } else if (spec.op_type == "copy") {
    ss << "  out0[g_id] = in0[g_id];\n";
  } else if (spec.op_type == "matmul") {
    // Basic Matmul compute kernel: C[i, j] = sum(A[i, k] * B[k, j])
    ss << "  uint M = " << (spec.M > 0 ? spec.M : 1) << ";\n";
    ss << "  uint N = " << (spec.N > 0 ? spec.N : 1) << ";\n";
    ss << "  uint K = " << (spec.K > 0 ? spec.K : 1) << ";\n";
    ss << "  uint row = g_id / N;\n";
    ss << "  uint col = g_id % N;\n";
    ss << "  if (row < M && col < N) {\n";
    ss << "    " << glsl_type << " sum = " << glsl_type << "(0.0);\n";
    ss << "    for (uint k = 0; k < K; ++k) {\n";
    ss << "      sum += in0[row * K + k] * in1[k * N + col];\n";
    ss << "    }\n";
    ss << "    out0[row * N + col] = sum;\n";
    ss << "  }\n";
  } else {
    // Default binary add fallback
    ss << "  out0[g_id] = in0[g_id] + in1[g_id];\n";
  }

  ss << "}\n";
  return ss.str();
}

}  // namespace vulkan_pjrt
