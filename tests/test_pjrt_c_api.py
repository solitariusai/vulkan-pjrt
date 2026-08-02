import unittest
import numpy as np
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from vulkan_pjrt import VulkanPJRTClient

class TestPjrtCApiCompute(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.client = VulkanPJRTClient()

    def test_vector_add(self):
        a = np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0], dtype=np.float32)
        b = np.array([10.0, 20.0, 30.0, 40.0, 50.0, 60.0], dtype=np.float32)

        buf_a = self.client.buffer_from_numpy(a)
        buf_b = self.client.buffer_from_numpy(b)

        executable = self.client.compile("op:add", format="payload")
        outputs = self.client.execute(executable, [buf_a, buf_b])

        result = self.client.buffer_to_numpy(outputs[0], shape=a.shape, dtype=np.float32)
        expected = a + b
        np.testing.assert_allclose(result, expected, rtol=1e-5)

    def test_elementwise_mul(self):
        a = np.array([2.0, 3.0, 4.0], dtype=np.float32)
        b = np.array([5.0, 6.0, 7.0], dtype=np.float32)

        buf_a = self.client.buffer_from_numpy(a)
        buf_b = self.client.buffer_from_numpy(b)

        executable = self.client.compile("op:mul", format="payload")
        outputs = self.client.execute(executable, [buf_a, buf_b])

        result = self.client.buffer_to_numpy(outputs[0], shape=a.shape, dtype=np.float32)
        expected = a * b
        np.testing.assert_allclose(result, expected, rtol=1e-5)

    def test_relu_activation(self):
        a = np.array([-2.5, -1.0, 0.0, 1.5, 3.0], dtype=np.float32)
        buf_a = self.client.buffer_from_numpy(a)

        executable = self.client.compile("op:relu", format="payload")
        outputs = self.client.execute(executable, [buf_a])

        result = self.client.buffer_to_numpy(outputs[0], shape=a.shape, dtype=np.float32)
        expected = np.maximum(a, 0)
        np.testing.assert_allclose(result, expected, rtol=1e-5)

    def test_scalar_scale(self):
        a = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float32)
        buf_a = self.client.buffer_from_numpy(a)

        executable = self.client.compile("op:scale,val:3.5", format="payload")
        outputs = self.client.execute(executable, [buf_a])

        result = self.client.buffer_to_numpy(outputs[0], shape=a.shape, dtype=np.float32)
        expected = a * 3.5
        np.testing.assert_allclose(result, expected, rtol=1e-5)

    def test_matrix_multiplication(self):
        M, K, N = 4, 3, 2
        a = np.random.randn(M, K).astype(np.float32)
        b = np.random.randn(K, N).astype(np.float32)

        buf_a = self.client.buffer_from_numpy(a)
        buf_b = self.client.buffer_from_numpy(b)

        executable = self.client.compile(f"op:matmul,M:{M},N:{N},K:{K}", format="payload")
        outputs = self.client.execute(executable, [buf_a, buf_b])

        result = self.client.buffer_to_numpy(outputs[0], shape=(M, N), dtype=np.float32)
        expected = np.matmul(a, b)
        np.testing.assert_allclose(result, expected, rtol=1e-4, atol=1e-4)

    def test_glsl_direct_compilation(self):
        glsl_code = """#version 450
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer InA { float a[]; };
layout(std430, binding = 1) buffer InB { float b[]; };
layout(std430, binding = 2) buffer OutC { float c[]; };
void main() {
    uint id = gl_GlobalInvocationID.x;
    c[id] = a[id] * 2.0 + b[id];
}"""
        a = np.array([1.0, 2.0, 3.0], dtype=np.float32)
        b = np.array([10.0, 20.0, 30.0], dtype=np.float32)

        buf_a = self.client.buffer_from_numpy(a)
        buf_b = self.client.buffer_from_numpy(b)

        executable = self.client.compile(glsl_code, format="glsl")
        outputs = self.client.execute(executable, [buf_a, buf_b])

        result = self.client.buffer_to_numpy(outputs[0], shape=a.shape, dtype=np.float32)
        expected = a * 2.0 + b
        np.testing.assert_allclose(result, expected, rtol=1e-5)

if __name__ == "__main__":
    unittest.main()
