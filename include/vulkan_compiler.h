#ifndef VULKAN_COMPILER_H_
#define VULKAN_COMPILER_H_

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace vulkan_pjrt {

struct OpSpec {
  std::string op_type; // "add", "sub", "mul", "div", "max", "min", "exp", "log", "sqrt", "sin", "cos", "relu", "matmul", "copy", "scale"
  std::string dtype;   // "float", "int"
  float scalar_val{0.0f};
  int M{0}, N{0}, K{0};
  std::vector<std::string> epilogue_ops;
};

class ShaderCompiler {
 public:
  static std::vector<uint32_t> CompileGLSLToSPIRV(const std::string& glsl_source);
  static std::string GenerateComputeShader(const OpSpec& spec, size_t num_inputs, size_t num_outputs);
};

}  // namespace vulkan_pjrt

#endif  // VULKAN_COMPILER_H_
