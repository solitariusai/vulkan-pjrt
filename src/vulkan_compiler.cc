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
  std::string glsl_type = (spec.dtype == "int" || spec.dtype == "int32") ? "int" : "float";
  
  ss << "layout(push_constant) uniform PushConstants {\n";
  ss << "  uint M;\n";
  ss << "  uint N;\n";
  ss << "  uint K;\n";
  ss << "} pc;\n\n";

  if (spec.op_type == "matmul" || spec.op_type == "matmul_add" || spec.op_type == "matmul_fused") {
    ss << "layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;\n\n";
    ss << "shared " << glsl_type << " Asub[16][16];\n";
    ss << "shared " << glsl_type << " Bsub[16][16];\n\n";
  } else {
    ss << "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n\n";
  }

  size_t binding_idx = 0;
  for (size_t i = 0; i < num_inputs; ++i) {
    ss << "layout(std430, binding = " << binding_idx++ << ") buffer Input" << i << " { " << glsl_type << " in" << i << "[]; };\n";
  }
  for (size_t i = 0; i < num_outputs; ++i) {
    ss << "layout(std430, binding = " << binding_idx++ << ") buffer Output" << i << " { " << glsl_type << " out" << i << "[]; };\n";
  }

  ss << "\nvoid main() {\n";

  if (spec.op_type == "matmul" || spec.op_type == "matmul_add" || spec.op_type == "matmul_fused") {
    ss << "  uint M = pc.M;\n";
    ss << "  uint N = pc.N;\n";
    ss << "  uint K = pc.K;\n\n";
    ss << "  uint row = gl_GlobalInvocationID.y;\n";
    ss << "  uint col = gl_GlobalInvocationID.x;\n";
    ss << "  uint local_row = gl_LocalInvocationID.y;\n";
    ss << "  uint local_col = gl_LocalInvocationID.x;\n\n";
    ss << "  " << glsl_type << " sum = " << glsl_type << "(0.0);\n";
    ss << "  uint num_tiles = (K + 15) / 16;\n\n";
    ss << "  for (uint t = 0; t < num_tiles; ++t) {\n";
    ss << "    uint tiled_col = t * 16 + local_col;\n";
    ss << "    if (row < M && tiled_col < K) {\n";
    ss << "      Asub[local_row][local_col] = in0[row * K + tiled_col];\n";
    ss << "    } else {\n";
    ss << "      Asub[local_row][local_col] = " << glsl_type << "(0.0);\n";
    ss << "    }\n\n";
    ss << "    uint tiled_row = t * 16 + local_row;\n";
    ss << "    if (tiled_row < K && col < N) {\n";
    ss << "      Bsub[local_row][local_col] = in1[tiled_row * N + col];\n";
    ss << "    } else {\n";
    ss << "      Bsub[local_row][local_col] = " << glsl_type << "(0.0);\n";
    ss << "    }\n\n";
    ss << "    barrier();\n\n";
    ss << "    for (uint k = 0; k < 16; ++k) {\n";
    ss << "      sum += Asub[local_row][k] * Bsub[k][local_col];\n";
    ss << "    }\n";
    ss << "    barrier();\n";
    ss << "  }\n\n";
    ss << "  if (row < M && col < N) {\n";
    if (spec.op_type == "matmul_fused") {
      ss << "    " << glsl_type << " acc = sum;\n";
      size_t extra_in_idx = 2;
      for (const std::string& ep_op : spec.epilogue_ops) {
        if (ep_op == "add") {
          ss << "    acc = acc + in" << extra_in_idx++ << "[col];\n";
        } else if (ep_op == "mul") {
          ss << "    acc = acc * in" << extra_in_idx++ << "[col];\n";
        } else if (ep_op == "relu") {
          ss << "    acc = max(acc, " << glsl_type << "(0.0));\n";
        }
      }
      ss << "    out0[row * N + col] = acc;\n";
    } else if (spec.op_type == "matmul_add") {
      ss << "    out0[row * N + col] = sum + in2[col];\n";
    } else {
      ss << "    out0[row * N + col] = sum;\n";
    }
    ss << "  }\n";
  } else {
    ss << "  uint g_id = gl_GlobalInvocationID.x;\n";
    if (spec.op_type == "add") {
      ss << "  out0[g_id] = in0[g_id] + in1[g_id];\n";
    } else if (spec.op_type == "sub") {
      ss << "  out0[g_id] = in0[g_id] - in1[g_id];\n";
    } else if (spec.op_type == "mul") {
      ss << "  out0[g_id] = in0[g_id] * in1[g_id];\n";
    } else if (spec.op_type == "div") {
      ss << "  out0[g_id] = in0[g_id] / in1[g_id];\n";
    } else if (spec.op_type == "max") {
      ss << "  out0[g_id] = max(in0[g_id], in1[g_id]);\n";
    } else if (spec.op_type == "min") {
      ss << "  out0[g_id] = min(in0[g_id], in1[g_id]);\n";
    } else if (spec.op_type == "exp") {
      ss << "  out0[g_id] = exp(in0[g_id]);\n";
    } else if (spec.op_type == "log") {
      ss << "  out0[g_id] = log(in0[g_id]);\n";
    } else if (spec.op_type == "sqrt") {
      ss << "  out0[g_id] = sqrt(in0[g_id]);\n";
    } else if (spec.op_type == "sin") {
      ss << "  out0[g_id] = sin(in0[g_id]);\n";
    } else if (spec.op_type == "cos") {
      ss << "  out0[g_id] = cos(in0[g_id]);\n";
    } else if (spec.op_type == "relu") {
      ss << "  out0[g_id] = max(in0[g_id], " << glsl_type << "(0.0));\n";
    } else if (spec.op_type == "scale") {
      ss << "  out0[g_id] = in0[g_id] * " << glsl_type << "(" << spec.scalar_val << ");\n";
    } else if (spec.op_type == "copy") {
      ss << "  out0[g_id] = in0[g_id];\n";
    } else {
      ss << "  out0[g_id] = in0[g_id] + in1[g_id];\n";
    }
  }

  ss << "}\n";
  return ss.str();
}

}  // namespace vulkan_pjrt
