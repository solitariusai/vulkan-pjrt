import os
import ctypes
from typing import List, Tuple, Optional
import numpy as np

# Path to built shared library
LIB_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "build", "libvulkan_pjrt.so")
)

def get_library_path() -> str:
    if not os.path.exists(LIB_PATH):
        raise FileNotFoundError(f"Vulkan PJRT plugin library not found at: {LIB_PATH}")
    return LIB_PATH

# Enums
class PJRT_Buffer_Type:
    INVALID = 0
    PRED = 1
    S8 = 2
    S16 = 3
    S32 = 4
    S64 = 5
    U8 = 6
    U16 = 7
    U32 = 8
    U64 = 9
    F16 = 10
    F32 = 11
    F64 = 12
    BF16 = 13
    C64 = 14
    C128 = 15

    @staticmethod
    def from_numpy(dtype) -> int:
        dt = np.dtype(dtype)
        if dt == np.float32: return PJRT_Buffer_Type.F32
        if dt == np.int32: return PJRT_Buffer_Type.S32
        if dt == np.int64: return PJRT_Buffer_Type.S64
        if dt == np.uint8: return PJRT_Buffer_Type.U8
        if dt == np.float64: return PJRT_Buffer_Type.F64
        if dt == np.float16: return PJRT_Buffer_Type.F16
        if dt == np.bool_: return PJRT_Buffer_Type.PRED
        return PJRT_Buffer_Type.F32

# C API Structures
class PJRT_Api_Version(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("major_version", ctypes.c_int),
        ("minor_version", ctypes.c_int),
    ]

class PJRT_Program(ctypes.Structure):
    _fields_ = [
        ("code", ctypes.c_char_p),
        ("code_size", ctypes.c_size_t),
        ("format", ctypes.c_char_p),
        ("format_size", ctypes.c_size_t),
    ]

class PJRT_Client_Create_Args(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("client", ctypes.POINTER(ctypes.c_void_p)),
    ]

class PJRT_Client_PlatformName_Args(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("client", ctypes.c_void_p),
        ("platform_name", ctypes.c_char_p),
        ("platform_name_size", ctypes.c_size_t),
    ]

class PJRT_Client_BufferFromHostBuffer_Args(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("data", ctypes.c_void_p),
        ("type", ctypes.c_int),
        ("dims", ctypes.POINTER(ctypes.c_int64)),
        ("num_dims", ctypes.c_size_t),
        ("byte_strides", ctypes.POINTER(ctypes.c_int64)),
        ("num_byte_strides", ctypes.c_size_t),
        ("host_buffer_semantics", ctypes.c_int),
        ("device", ctypes.c_void_p),
        ("done_with_host_buffer", ctypes.c_void_p),
        ("buffer", ctypes.POINTER(ctypes.c_void_p)),
        ("client", ctypes.c_void_p),
    ]

class PJRT_Buffer_ToHostBuffer_Args(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("buffer", ctypes.c_void_p),
        ("dst", ctypes.c_void_p),
        ("dst_size", ctypes.c_size_t),
        ("event", ctypes.c_void_p),
    ]

class PJRT_Client_Compile_Args(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("client", ctypes.c_void_p),
        ("program", ctypes.POINTER(PJRT_Program)),
        ("executable", ctypes.POINTER(ctypes.c_void_p)),
        ("compile_options", ctypes.c_char_p),
        ("compile_options_size", ctypes.c_size_t),
    ]

class PJRT_LoadedExecutable_Execute_Args(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("executable", ctypes.c_void_p),
        ("options", ctypes.c_void_p),
        ("argument_handles", ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))),
        ("num_devices", ctypes.c_size_t),
        ("num_args", ctypes.c_size_t),
        ("output_handles", ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))),
        ("execute_device_complete_events", ctypes.c_void_p),
    ]

class PJRT_Api(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("pjrt_api_version", PJRT_Api_Version),

        ("PJRT_Error_Destroy", ctypes.c_void_p),
        ("PJRT_Error_Message", ctypes.c_void_p),
        ("PJRT_Error_GetCode", ctypes.c_void_p),

        ("PJRT_Client_Create", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_Client_Create_Args))),
        ("PJRT_Client_Destroy", ctypes.c_void_p),
        ("PJRT_Client_PlatformName", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_Client_PlatformName_Args))),
        ("PJRT_Client_ProcessIndex", ctypes.c_void_p),
        ("PJRT_Client_Devices", ctypes.c_void_p),
        ("PJRT_Client_AddressableDevices", ctypes.c_void_p),
        ("PJRT_Client_LookupDevice", ctypes.c_void_p),
        ("PJRT_Client_Compile", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_Client_Compile_Args))),
        ("PJRT_Client_BufferFromHostBuffer", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_Client_BufferFromHostBuffer_Args))),

        ("PJRT_LoadedExecutable_Destroy", ctypes.c_void_p),
        ("PJRT_LoadedExecutable_Execute", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_LoadedExecutable_Execute_Args))),

        ("PJRT_Buffer_Destroy", ctypes.c_void_p),
        ("PJRT_Buffer_ToHostBuffer", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_Buffer_ToHostBuffer_Args))),
        ("PJRT_Buffer_OnDeviceSizeInBytes", ctypes.c_void_p),
        ("PJRT_Buffer_Dimensions", ctypes.c_void_p),
        ("PJRT_Buffer_ElementType", ctypes.c_void_p),

        ("PJRT_Event_Destroy", ctypes.c_void_p),
        ("PJRT_Event_IsReady", ctypes.c_void_p),
        ("PJRT_Event_Error", ctypes.c_void_p),
        ("PJRT_Event_Await", ctypes.c_void_p),

        ("PJRT_Device_Id", ctypes.c_void_p),
        ("PJRT_Device_ProcessIndex", ctypes.c_void_p),
        ("PJRT_Device_DebugString", ctypes.c_void_p),
        ("PJRT_Device_ToString", ctypes.c_void_p),
    ]

# High-Level Python Client Wrapper
class VulkanPJRTClient:
    def __init__(self):
        lib_path = get_library_path()
        self.lib = ctypes.CDLL(lib_path)
        self.lib.GetPjrtApi.restype = ctypes.POINTER(PJRT_Api)
        self.api = self.lib.GetPjrtApi().contents

        client_ptr = ctypes.c_void_p()
        args = PJRT_Client_Create_Args()
        args.struct_size = ctypes.sizeof(PJRT_Client_Create_Args)
        args.client = ctypes.cast(ctypes.byref(client_ptr), ctypes.POINTER(ctypes.c_void_p))
        err = self.api.PJRT_Client_Create(ctypes.byref(args))
        if err:
            raise RuntimeError(f"Failed to create Vulkan PJRT Client")
        self.client_ptr = client_ptr.value

    def get_platform_name(self) -> str:
        args = PJRT_Client_PlatformName_Args()
        args.struct_size = ctypes.sizeof(PJRT_Client_PlatformName_Args)
        args.client = self.client_ptr
        self.api.PJRT_Client_PlatformName(ctypes.byref(args))
        return args.platform_name.decode("utf-8") if args.platform_name else "vulkan"

    def buffer_from_numpy(self, arr: np.ndarray) -> ctypes.c_void_p:
        arr_contiguous = np.ascontiguousarray(arr)
        dims_type = ctypes.c_int64 * arr.ndim
        dims_array = dims_type(*arr.shape)

        buf_ptr = ctypes.c_void_p()
        args = PJRT_Client_BufferFromHostBuffer_Args()
        args.struct_size = ctypes.sizeof(PJRT_Client_BufferFromHostBuffer_Args)
        args.client = self.client_ptr
        args.data = arr_contiguous.ctypes.data_as(ctypes.c_void_p)
        args.type = PJRT_Buffer_Type.from_numpy(arr.dtype)
        args.dims = dims_array
        args.num_dims = arr.ndim
        args.buffer = ctypes.cast(ctypes.byref(buf_ptr), ctypes.POINTER(ctypes.c_void_p))

        err = self.api.PJRT_Client_BufferFromHostBuffer(ctypes.byref(args))
        if err:
            raise RuntimeError("Failed to create buffer from host array")
        return buf_ptr.value

    def buffer_to_numpy(self, buffer_ptr: ctypes.c_void_p, shape: Tuple[int, ...], dtype=np.float32) -> np.ndarray:
        result = np.empty(shape, dtype=dtype)
        args = PJRT_Buffer_ToHostBuffer_Args()
        args.struct_size = ctypes.sizeof(PJRT_Buffer_ToHostBuffer_Args)
        args.buffer = buffer_ptr
        args.dst = result.ctypes.data_as(ctypes.c_void_p)
        args.dst_size = result.nbytes

        err = self.api.PJRT_Buffer_ToHostBuffer(ctypes.byref(args))
        if err:
            raise RuntimeError("Failed to copy Vulkan buffer to host")
        return result

    def compile(self, code: str, format: str = "payload") -> ctypes.c_void_p:
        prog = PJRT_Program()
        prog.code = code.encode("utf-8")
        prog.code_size = len(prog.code)
        prog.format = format.encode("utf-8")
        prog.format_size = len(prog.format)

        exec_ptr = ctypes.c_void_p()
        args = PJRT_Client_Compile_Args()
        args.struct_size = ctypes.sizeof(PJRT_Client_Compile_Args)
        args.client = self.client_ptr
        args.program = ctypes.pointer(prog)
        args.executable = ctypes.cast(ctypes.byref(exec_ptr), ctypes.POINTER(ctypes.c_void_p))

        err = self.api.PJRT_Client_Compile(ctypes.byref(args))
        if err:
            raise RuntimeError("Failed to compile computation for Vulkan device")
        return exec_ptr.value

    def execute(self, executable_ptr: ctypes.c_void_p, inputs: List[ctypes.c_void_p]) -> List[ctypes.c_void_p]:
        arg_array_type = ctypes.c_void_p * len(inputs)
        arg_array = arg_array_type(*inputs)
        
        device_args = (ctypes.c_void_p * 1)()
        device_args[0] = ctypes.cast(arg_array, ctypes.c_void_p)

        out_bufs = (ctypes.c_void_p * 1)()
        out_lists = (ctypes.POINTER(ctypes.c_void_p) * 1)()
        out_lists[0] = out_bufs

        args = PJRT_LoadedExecutable_Execute_Args()
        args.struct_size = ctypes.sizeof(PJRT_LoadedExecutable_Execute_Args)
        args.executable = executable_ptr
        args.argument_lists = ctypes.cast(ctypes.pointer(device_args), ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p)))
        args.num_devices = 1
        args.num_args = len(inputs)
        args.output_lists = ctypes.cast(out_lists, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p)))

        err = self.api.PJRT_LoadedExecutable_Execute(ctypes.byref(args))
        if err:
            raise RuntimeError("Failed to execute computation on Vulkan GPU")

        return [out_bufs[0]]

def register_jax_plugin():
    """Register Vulkan PJRT plugin with JAX."""
    so_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "vulkan_pjrt.so")
    )
    if not os.path.exists(so_path):
        so_path = os.path.abspath(
            os.path.join(os.path.dirname(__file__), "..", "build", "libvulkan_pjrt.so")
        )
    
    try:
        import jax._src.xla_bridge as xla_bridge
        xla_bridge.register_plugin(
            c_api_path=so_path,
            c_api_custom_name="vulkan",
        )
    except Exception:
        os.environ["PJRT_NAMES_AND_LIBRARY_PATHS"] = f"vulkan:{so_path}"

try:
    register_jax_plugin()
except Exception:
    pass
