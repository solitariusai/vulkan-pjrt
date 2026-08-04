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

  if (spec.op_type.find("matmul") != std::string::npos) {
    ss << "layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n\n";
    ss << "#define TILE_M 32\n";
    ss << "#define TILE_N 32\n";
    ss << "#define TILE_K 16\n\n";
    ss << "shared " << glsl_type << " Asub[TILE_M][TILE_K];\n";
    ss << "shared " << glsl_type << " Bsub[TILE_K][TILE_N];\n\n";
  } else if (spec.op_type == "transpose") {
    ss << "layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;\n\n";
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

  if (spec.op_type.find("matmul") != std::string::npos) {
    ss << "  uint M = pc.M;\n";
    ss << "  uint N = pc.N;\n";
    ss << "  uint K = pc.K;\n\n";
    ss << "  uint row_block = gl_WorkGroupID.y * TILE_M;\n";
    ss << "  uint col_block = gl_WorkGroupID.x * TILE_N;\n";
    ss << "  uint local_row = gl_LocalInvocationID.y * 4;\n";
    ss << "  uint local_col = gl_LocalInvocationID.x * 4;\n";
    ss << "  uint flat_id = gl_LocalInvocationIndex;\n\n";
    
    ss << "  " << glsl_type << " acc[4][4];\n";
    ss << "  for (int i = 0; i < 4; ++i)\n";
    ss << "    for (int j = 0; j < 4; ++j)\n";
    ss << "      acc[i][j] = " << glsl_type << "(0.0);\n\n";
    
    ss << "  uint num_tiles = (K + TILE_K - 1) / TILE_K;\n\n";
    ss << "  for (uint t = 0; t < num_tiles; ++t) {\n";
    ss << "    for (uint i = 0; i < 8; ++i) {\n";
    ss << "      uint idx = flat_id * 8 + i;\n";
    ss << "      uint a_row = idx / TILE_K;\n";
    ss << "      uint a_col = idx % TILE_K;\n";
    ss << "      uint global_a_row = row_block + a_row;\n";
    ss << "      uint global_a_col = t * TILE_K + a_col;\n";
    if (spec.op_type == "matmul_trans_a") {
      ss << "      if (global_a_col < M && global_a_row < K)\n";
      ss << "        Asub[a_row][a_col] = in0[global_a_col * K + global_a_row];\n";
    } else {
      ss << "      if (global_a_row < M && global_a_col < K)\n";
      ss << "        Asub[a_row][a_col] = in0[global_a_row * K + global_a_col];\n";
    }
    ss << "      else\n";
    ss << "        Asub[a_row][a_col] = " << glsl_type << "(0.0);\n";
    ss << "    }\n";
    ss << "    for (uint i = 0; i < 8; ++i) {\n";
    ss << "      uint idx = flat_id * 8 + i;\n";
    ss << "      uint b_row = idx / TILE_N;\n";
    ss << "      uint b_col = idx % TILE_N;\n";
    ss << "      uint global_b_row = t * TILE_K + b_row;\n";
    ss << "      uint global_b_col = col_block + b_col;\n";
    if (spec.op_type == "matmul_trans_b") {
      ss << "      if (global_b_col < K && global_b_row < N)\n";
      ss << "        Bsub[b_row][b_col] = in1[global_b_col * N + global_b_row];\n";
    } else {
      ss << "      if (global_b_row < K && global_b_col < N)\n";
      ss << "        Bsub[b_row][b_col] = in1[global_b_row * N + global_b_col];\n";
    }
    ss << "      else\n";
    ss << "        Bsub[b_row][b_col] = " << glsl_type << "(0.0);\n";
    ss << "    }\n";
    ss << "    barrier();\n\n";
    
    ss << "    for (uint k = 0; k < TILE_K; ++k) {\n";
    ss << "      " << glsl_type << " a_reg[4];\n";
    ss << "      " << glsl_type << " b_reg[4];\n";
    ss << "      for (int i = 0; i < 4; ++i) a_reg[i] = Asub[local_row + i][k];\n";
    ss << "      for (int j = 0; j < 4; ++j) b_reg[j] = Bsub[k][local_col + j];\n";
    ss << "      for (int i = 0; i < 4; ++i) {\n";
    ss << "        for (int j = 0; j < 4; ++j) {\n";
    ss << "          acc[i][j] += a_reg[i] * b_reg[j];\n";
    ss << "        }\n";
    ss << "      }\n";
    ss << "    }\n";
    ss << "    barrier();\n";
    ss << "  }\n\n";
    
    ss << "  for (int i = 0; i < 4; ++i) {\n";
    ss << "    for (int j = 0; j < 4; ++j) {\n";
    ss << "      uint global_row = row_block + local_row + i;\n";
    ss << "      uint global_col = col_block + local_col + j;\n";
    ss << "      if (global_row < M && global_col < N) {\n";
    
    if (spec.op_type == "matmul_fused") {
      ss << "        " << glsl_type << " val = acc[i][j];\n";
      size_t extra_in_idx = 2;
      for (const std::string& ep_op : spec.epilogue_ops) {
        if (ep_op == "add") {
          ss << "        val = val + in" << extra_in_idx++ << "[global_col];\n";
        } else if (ep_op == "mul") {
          ss << "        val = val * in" << extra_in_idx++ << "[global_col];\n";
        } else if (ep_op == "relu") {
          ss << "        val = max(val, " << glsl_type << "(0.0));\n";
        }
      }
      ss << "        out0[global_row * N + global_col] = val;\n";
    } else if (spec.op_type == "matmul_add") {
      ss << "        out0[global_row * N + global_col] = acc[i][j] + in2[global_col];\n";
    } else {
      ss << "        out0[global_row * N + global_col] = acc[i][j];\n";
    }
    
    ss << "      }\n";
    ss << "    }\n";
    ss << "  }\n";
  } else if (spec.op_type == "transpose") {
    ss << "  uint row = gl_GlobalInvocationID.y;\n";
    ss << "  uint col = gl_GlobalInvocationID.x;\n";
    ss << "  uint M = pc.M;\n";
    ss << "  uint N = pc.N;\n";
    ss << "  if (row < M && col < N) {\n";
    ss << "    out0[col * M + row] = in0[row * N + col];\n";
    ss << "  }\n";
  } else if (spec.op_type == "reduce_sum") {
    ss << "  uint col = gl_GlobalInvocationID.x;\n";
    ss << "  uint M = pc.M;\n";
    ss << "  uint N = pc.N;\n";
    ss << "  if (col < N) {\n";
    ss << "    " << glsl_type << " sum = " << glsl_type << "(0.0);\n";
    ss << "    for (uint i = 0; i < M; ++i) {\n";
    ss << "      sum += in0[i * N + col];\n";
    ss << "    }\n";
    ss << "    out0[col] = sum;\n";
    ss << "  }\n";
  } else {
    ss << "  uint g_id = gl_GlobalInvocationID.x;\n";
    ss << "  if (g_id >= pc.M) return;\n";
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
    } else if (spec.op_type == "elementwise_fused") {
      ss << "  " << glsl_type << " acc = in0[g_id];\n";
      size_t in_idx = 1;
      for (size_t i = 1; i < spec.epilogue_ops.size(); ++i) {
        const std::string& ep = spec.epilogue_ops[i];
        if (ep == "add") {
          ss << "  acc = acc + in" << in_idx++ << "[g_id];\n";
        } else if (ep == "sub") {
          ss << "  acc = acc - in" << in_idx++ << "[g_id];\n";
        } else if (ep == "mul") {
          ss << "  acc = acc * in" << in_idx++ << "[g_id];\n";
        } else if (ep == "div") {
          ss << "  acc = acc / in" << in_idx++ << "[g_id];\n";
        } else if (ep == "relu") {
          ss << "  acc = max(acc, " << glsl_type << "(0.0));\n";
        } else if (ep == "scale") {
          ss << "  acc = acc * " << glsl_type << "(" << spec.scalar_val << ");\n";
        }
      }
      ss << "  out0[g_id] = acc;\n";
    } else {
      ss << "  out0[g_id] = in0[g_id] + in1[g_id];\n";
    }
  }

  ss << "}\n";
  return ss.str();
}

}  // namespace vulkan_pjrt
