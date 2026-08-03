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
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("code", ctypes.c_char_p),
        ("code_size", ctypes.c_size_t),
        ("format", ctypes.c_char_p),
        ("format_size", ctypes.c_size_t),
    ]

class PJRT_Client_Create_Args(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("create_options", ctypes.c_void_p),
        ("num_options", ctypes.c_size_t),
        ("kv_get_callback", ctypes.c_void_p),
        ("kv_get_user_arg", ctypes.c_void_p),
        ("kv_put_callback", ctypes.c_void_p),
        ("kv_put_user_arg", ctypes.c_void_p),
        ("client", ctypes.c_void_p),
        ("kv_try_get_callback", ctypes.c_void_p),
        ("kv_try_get_user_arg", ctypes.c_void_p),
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
        ("client", ctypes.c_void_p),
        ("data", ctypes.c_void_p),
        ("type", ctypes.c_int),
        ("dims", ctypes.POINTER(ctypes.c_int64)),
        ("num_dims", ctypes.c_size_t),
        ("byte_strides", ctypes.POINTER(ctypes.c_int64)),
        ("num_byte_strides", ctypes.c_size_t),
        ("host_buffer_semantics", ctypes.c_int),
        ("device", ctypes.c_void_p),
        ("memory", ctypes.c_void_p),
        ("device_layout", ctypes.c_void_p),
        ("done_with_host_buffer", ctypes.c_void_p),
        ("buffer", ctypes.c_void_p),
    ]

class PJRT_Buffer_ToHostBuffer_Args(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("src", ctypes.c_void_p),
        ("host_layout", ctypes.c_void_p),
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
        ("compile_options", ctypes.c_char_p),
        ("compile_options_size", ctypes.c_size_t),
        ("executable", ctypes.c_void_p),
    ]

class PJRT_LoadedExecutable_Execute_Args(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("executable", ctypes.c_void_p),
        ("options", ctypes.c_void_p),
        ("argument_lists", ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))),
        ("num_devices", ctypes.c_size_t),
        ("num_args", ctypes.c_size_t),
        ("output_lists", ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))),
        ("device_complete_events", ctypes.c_void_p),
        ("execute_device", ctypes.c_void_p),
    ]

class PJRT_Api(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_size_t),
        ("extension_start", ctypes.c_void_p),
        ("pjrt_api_version", PJRT_Api_Version),

        ("PJRT_Error_Destroy", ctypes.c_void_p),
        ("PJRT_Error_Message", ctypes.c_void_p),
        ("PJRT_Error_GetCode", ctypes.c_void_p),

        ("PJRT_Plugin_Initialize", ctypes.c_void_p),
        ("PJRT_Plugin_Attributes", ctypes.c_void_p),

        ("PJRT_Event_Destroy", ctypes.c_void_p),
        ("PJRT_Event_IsReady", ctypes.c_void_p),
        ("PJRT_Event_Error", ctypes.c_void_p),
        ("PJRT_Event_Await", ctypes.c_void_p),
        ("PJRT_Event_OnReady", ctypes.c_void_p),

        ("PJRT_Client_Create", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_Client_Create_Args))),
        ("PJRT_Client_Destroy", ctypes.c_void_p),
        ("PJRT_Client_PlatformName", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_Client_PlatformName_Args))),
        ("PJRT_Client_ProcessIndex", ctypes.c_void_p),
        ("PJRT_Client_PlatformVersion", ctypes.c_void_p),
        ("PJRT_Client_Devices", ctypes.c_void_p),
        ("PJRT_Client_AddressableDevices", ctypes.c_void_p),
        ("PJRT_Client_LookupDevice", ctypes.c_void_p),
        ("PJRT_Client_LookupAddressableDevice", ctypes.c_void_p),
        ("PJRT_Client_AddressableMemories", ctypes.c_void_p),
        ("PJRT_Client_Compile", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_Client_Compile_Args))),
        ("PJRT_Client_DefaultDeviceAssignment", ctypes.c_void_p),
        ("PJRT_Client_BufferFromHostBuffer", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_Client_BufferFromHostBuffer_Args))),

        ("PJRT_DeviceDescription_Id", ctypes.c_void_p),
        ("PJRT_DeviceDescription_ProcessIndex", ctypes.c_void_p),
        ("PJRT_DeviceDescription_Attributes", ctypes.c_void_p),
        ("PJRT_DeviceDescription_Kind", ctypes.c_void_p),
        ("PJRT_DeviceDescription_DebugString", ctypes.c_void_p),
        ("PJRT_DeviceDescription_ToString", ctypes.c_void_p),

        ("PJRT_Device_GetDescription", ctypes.c_void_p),
        ("PJRT_Device_IsAddressable", ctypes.c_void_p),
        ("PJRT_Device_LocalHardwareId", ctypes.c_void_p),
        ("PJRT_Device_AddressableMemories", ctypes.c_void_p),
        ("PJRT_Device_DefaultMemory", ctypes.c_void_p),
        ("PJRT_Device_MemoryStats", ctypes.c_void_p),

        ("PJRT_Memory_Id", ctypes.c_void_p),
        ("PJRT_Memory_Kind", ctypes.c_void_p),
        ("PJRT_Memory_DebugString", ctypes.c_void_p),
        ("PJRT_Memory_ToString", ctypes.c_void_p),
        ("PJRT_Memory_AddressableByDevices", ctypes.c_void_p),

        ("PJRT_Executable_Destroy", ctypes.c_void_p),
        ("PJRT_Executable_Name", ctypes.c_void_p),
        ("PJRT_Executable_NumReplicas", ctypes.c_void_p),
        ("PJRT_Executable_NumPartitions", ctypes.c_void_p),
        ("PJRT_Executable_NumOutputs", ctypes.c_void_p),
        ("PJRT_Executable_SizeOfGeneratedCodeInBytes", ctypes.c_void_p),
        ("PJRT_Executable_GetCostAnalysis", ctypes.c_void_p),
        ("PJRT_Executable_OutputMemoryKinds", ctypes.c_void_p),
        ("PJRT_Executable_OptimizedProgram", ctypes.c_void_p),
        ("PJRT_Executable_Serialize", ctypes.c_void_p),

        ("PJRT_LoadedExecutable_Destroy", ctypes.c_void_p),
        ("PJRT_LoadedExecutable_GetExecutable", ctypes.c_void_p),
        ("PJRT_LoadedExecutable_AddressableDevices", ctypes.c_void_p),
        ("PJRT_LoadedExecutable_Delete", ctypes.c_void_p),
        ("PJRT_LoadedExecutable_IsDeleted", ctypes.c_void_p),
        ("PJRT_LoadedExecutable_Execute", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_LoadedExecutable_Execute_Args))),
        ("PJRT_Executable_DeserializeAndLoad", ctypes.c_void_p),
        ("PJRT_LoadedExecutable_Fingerprint", ctypes.c_void_p),

        ("PJRT_Buffer_Destroy", ctypes.c_void_p),
        ("PJRT_Buffer_ElementType", ctypes.c_void_p),
        ("PJRT_Buffer_Dimensions", ctypes.c_void_p),
        ("PJRT_Buffer_UnpaddedDimensions", ctypes.c_void_p),
        ("PJRT_Buffer_DynamicDimensionIndices", ctypes.c_void_p),
        ("PJRT_Buffer_GetMemoryLayout", ctypes.c_void_p),
        ("PJRT_Buffer_OnDeviceSizeInBytes", ctypes.c_void_p),
        ("PJRT_Buffer_Device", ctypes.c_void_p),
        ("PJRT_Buffer_Memory", ctypes.c_void_p),
        ("PJRT_Buffer_Delete", ctypes.c_void_p),
        ("PJRT_Buffer_IsDeleted", ctypes.c_void_p),
        ("PJRT_Buffer_CopyToDevice", ctypes.c_void_p),
        ("PJRT_Buffer_ToHostBuffer", ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.POINTER(PJRT_Buffer_ToHostBuffer_Args))),
        ("PJRT_Buffer_IsOnCpu", ctypes.c_void_p),
        ("PJRT_Buffer_ReadyEvent", ctypes.c_void_p),
        ("PJRT_Buffer_UnsafePointer", ctypes.c_void_p),
    ]

# High-Level Python Client Wrapper
class VulkanPJRTClient:
    def __init__(self):
        lib_path = get_library_path()
        self.lib = ctypes.CDLL(lib_path)
        self.lib.GetPjrtApi.restype = ctypes.POINTER(PJRT_Api)
        self.api = self.lib.GetPjrtApi().contents

        args = PJRT_Client_Create_Args()
        args.struct_size = ctypes.sizeof(PJRT_Client_Create_Args)
        args.client = None
        err = self.api.PJRT_Client_Create(ctypes.byref(args))
        if err:
            raise RuntimeError(f"Failed to create Vulkan PJRT Client")
        self.client_ptr = args.client

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

        args = PJRT_Client_BufferFromHostBuffer_Args()
        args.struct_size = ctypes.sizeof(PJRT_Client_BufferFromHostBuffer_Args)
        args.client = self.client_ptr
        args.data = arr_contiguous.ctypes.data_as(ctypes.c_void_p)
        args.type = PJRT_Buffer_Type.from_numpy(arr.dtype)
        args.dims = dims_array
        args.num_dims = arr.ndim
        args.byte_strides = None
        args.num_byte_strides = 0
        args.host_buffer_semantics = 0
        args.device = None
        args.memory = None
        args.device_layout = None
        args.buffer = None

        err = self.api.PJRT_Client_BufferFromHostBuffer(ctypes.byref(args))
        if err:
            raise RuntimeError("Failed to create buffer from host array")
        return args.buffer

    def buffer_to_numpy(self, buffer_ptr: ctypes.c_void_p, shape: Tuple[int, ...], dtype=np.float32) -> np.ndarray:
        result = np.empty(shape, dtype=dtype)
        args = PJRT_Buffer_ToHostBuffer_Args()
        args.struct_size = ctypes.sizeof(PJRT_Buffer_ToHostBuffer_Args)
        args.src = buffer_ptr
        args.host_layout = None
        args.dst = result.ctypes.data_as(ctypes.c_void_p)
        args.dst_size = result.nbytes

        err = self.api.PJRT_Buffer_ToHostBuffer(ctypes.byref(args))
        if err:
            raise RuntimeError("Failed to copy Vulkan buffer to host")
        return result

    def compile(self, code: str, format: str = "payload") -> ctypes.c_void_p:
        prog = PJRT_Program()
        prog.struct_size = ctypes.sizeof(PJRT_Program)
        prog.extension_start = None
        prog.code = code.encode("utf-8")
        prog.code_size = len(prog.code)
        prog.format = format.encode("utf-8")
        prog.format_size = len(prog.format)

        args = PJRT_Client_Compile_Args()
        args.struct_size = ctypes.sizeof(PJRT_Client_Compile_Args)
        args.client = self.client_ptr
        args.program = ctypes.pointer(prog)
        args.executable = None

        err = self.api.PJRT_Client_Compile(ctypes.byref(args))
        if err:
            raise RuntimeError("Failed to compile computation for Vulkan device")
        return args.executable

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

def initialize():
    """Plugin initialization callback for JAX."""
    pass

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
