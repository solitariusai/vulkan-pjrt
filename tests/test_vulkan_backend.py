import unittest
import numpy as np
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from vulkan_pjrt import VulkanPJRTClient

class TestVulkanBackend(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.client = VulkanPJRTClient()

    def test_platform_name(self):
        name = self.client.get_platform_name()
        self.assertEqual(name, "vulkan")

    def test_buffer_transfer(self):
        data = np.array([1.5, 2.5, 3.5, 4.5], dtype=np.float32)
        buf = self.client.buffer_from_numpy(data)
        out = self.client.buffer_to_numpy(buf, shape=data.shape, dtype=np.float32)
        np.testing.assert_allclose(data, out, rtol=1e-5)

    def test_large_buffer_transfer(self):
        data = np.random.randn(1024, 1024).astype(np.float32)
        buf = self.client.buffer_from_numpy(data)
        out = self.client.buffer_to_numpy(buf, shape=data.shape, dtype=np.float32)
        np.testing.assert_allclose(data, out, rtol=1e-5)

if __name__ == "__main__":
    unittest.main()
