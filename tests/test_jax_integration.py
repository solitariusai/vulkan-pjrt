import unittest
import os
import sys
import numpy as np

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from vulkan_pjrt import register_jax_plugin, get_library_path

class TestJAXPluginIntegration(unittest.TestCase):
    def test_jax_plugin_registration(self):
        register_jax_plugin()
        lib_path = get_library_path()
        self.assertTrue(os.path.exists(lib_path))
        self.assertIn("vulkan", os.environ.get("PJRT_NAMES_AND_LIBRARY_PATHS", ""))

        import jax
        print(f"JAX version: {jax.__version__}")
        print(f"PJRT Env: {os.environ.get('PJRT_NAMES_AND_LIBRARY_PATHS')}")

if __name__ == "__main__":
    unittest.main()
