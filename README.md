# Vulkan PJRT Plugin

Native **Vulkan PJRT Plugin** implementing the [OpenXLA PJRT C API](https://openxla.org/) specification for acceleration of compute operations (JAX / XLA) on Vulkan-compatible GPUs.

---

## 🌟 Features

- **OpenXLA PJRT C API Specification**: Implements standard `pjrt_c_api.h` and exports `GetPjrtApi()` ABI entrypoint for dynamic loading by machine learning frameworks (`.so` / `.dylib`).
- **Native Vulkan Compute Engine**: Directly manages `VkInstance`, `VkPhysicalDevice`, `VkDevice`, `VkQueue` compute family, `VkCommandPool`, `VkBuffer`, and GPU device memory allocations.
- **On-The-Fly SPIR-V Compiler**: Integrates shader compilation engine (`glslc`) to build optimized SPIR-V compute kernels dynamically for math operations and custom GLSL compute shaders.
- **High-Performance Execution**: Synchronously and asynchronously dispatches dispatches to Vulkan GPU compute queues with descriptor set binding and staging buffer memory management.
- **JAX Integration**: Python bindings for registering the Vulkan PJRT plugin via `PJRT_NAMES_AND_LIBRARY_PATHS` and JAX plugin hooks.

---

## 🏛️ Architecture Overview

```mermaid
graph TD
    A[JAX / OpenXLA Framework] -->|dlsym GetPjrtApi| B[PJRT C API Interface]
    B -->|PJRT_Client| C[Vulkan Client & Device Manager]
    B -->|PJRT_Buffer| D[Vulkan Buffer Allocator & Staging]
    B -->|PJRT_Client_Compile| E[Shader Compiler Engine]
    E -->|GLSL -> SPIR-V via glslc| F[VkShaderModule & VkComputePipeline]
    B -->|PJRT_LoadedExecutable_Execute| G[VkQueue Compute Dispatcher]
    G -->|vkCmdDispatch| H[Vulkan GPU Hardware]
```

### Core C++ Components

1. **`pjrt_c_api.h` / `src/pjrt_c_api.cc`**:
   - Defines the standard OpenXLA C struct dispatch table `PJRT_Api`.
   - Exports `const PJRT_Api* GetPjrtApi()` symbol.
   - Handles client, device, buffer, executable, and event lifecycles.

2. **`vulkan_backend.h` / `src/vulkan_backend.cc`**:
   - Vulkan device initialization (`vkCreateInstance`, physical device selection, compute queue discovery, `vkCreateDevice`).
   - Vulkan memory allocation (Device Local memory for fast GPU compute, Host Visible/Coherent staging buffers for transfers).

3. **`vulkan_buffer.h` / `src/vulkan_buffer.cc`**:
   - `PJRT_Buffer` implementation holding Vulkan `VkBuffer` handle, element type (`float32`, `int32`, etc.), tensor shape/dimensions, and row-major byte strides.

4. **`vulkan_compiler.h` / `src/vulkan_compiler.cc` & `vulkan_executable.h` / `src/vulkan_executable.cc`**:
   - SPIR-V lowering engine using `glslc` compiler.
   - Computes descriptors, `VkPipelineLayout`, and `VkComputePipeline`.
   - Dispatches grid dispatches to Vulkan compute queue and reads results back.

---

## 🛠️ Requirements & Building

### Prerequisites

- GCC / Clang (C++17 compiler support)
- CMake >= 3.16 & Ninja
- Vulkan SDK / `libvulkan.so`
- `glslc` (Vulkan shader compiler tool)
- Python >= 3.12 (with `numpy` and `jax`)

### 1. Build the Dynamic Shared Library (`libvulkan_pjrt.so`)

```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
```

### 2. Run the Unit Tests

```bash
# Run backend and memory transfer tests
python3 -m unittest tests/test_vulkan_backend.py

# Run GPU compute & SPIR-V shader tests
python3 -m unittest tests/test_pjrt_c_api.py

# Run JAX plugin registration test
python3 -m unittest tests/test_jax_integration.py
```

### 3. Run the Demonstration Script

```bash
python3 main.py
```

---

## 💻 Python Usage Example

```python
import numpy as np
from vulkan_pjrt import VulkanPJRTClient, register_jax_plugin

# 1. Register Vulkan PJRT Plugin with JAX
register_jax_plugin()

# 2. Initialize Client via GetPjrtApi()
client = VulkanPJRTClient()

# 3. Create GPU buffers
a = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float32)
b = np.array([10.0, 20.0, 30.0, 40.0], dtype=np.float32)

buf_a = client.buffer_from_numpy(a)
buf_b = client.buffer_from_numpy(b)

# 4. Compile & Execute Vector Add on Vulkan GPU
exec_add = client.compile("op:add", format="payload")
outputs = client.execute(exec_add, [buf_a, buf_b])

# 5. Read back result from Vulkan GPU memory
result = client.buffer_to_numpy(outputs[0], shape=a.shape, dtype=np.float32)
print("Vulkan GPU Result:", result)
# Output: [11. 22. 33. 44.]
```

---

## 📄 License

Apache License 2.0 - see [`LICENSE`](file:///home/shinapri/Documents/proj/vulkan-pjrt/LICENSE.md) for details.
