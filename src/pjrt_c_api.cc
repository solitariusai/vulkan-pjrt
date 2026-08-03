#include "pjrt_c_api.h"
#include "vulkan_client.h"
#include "vulkan_buffer.h"
#include "vulkan_executable.h"
#include "vulkan_event.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

struct PJRT_ErrorImpl : public PJRT_Error {
  PJRT_Error_Code code;
  std::string message;
};

static void ErrorDestroy(PJRT_Error* error) {
  delete static_cast<PJRT_ErrorImpl*>(error);
}

static void ErrorMessage(const PJRT_Error* error, const char** message, size_t* message_size) {
  if (!error) {
    if (message) *message = "";
    if (message_size) *message_size = 0;
    return;
  }
  auto err = static_cast<const PJRT_ErrorImpl*>(error);
  if (message) *message = err->message.c_str();
  if (message_size) *message_size = err->message.size();
}

static PJRT_Error_Code ErrorGetCode(const PJRT_Error* error) {
  if (!error) return PJRT_Error_Code_OK;
  auto err = static_cast<const PJRT_ErrorImpl*>(error);
  return err->code;
}

static void ErrorForEachPayload(const PJRT_Error* error, PJRT_Error_PayloadVisitor visitor, void* user_arg) {}

static const PJRT_Error_FunctionTable g_error_vtable = {
  /* struct_size */ sizeof(PJRT_Error_FunctionTable),
  /* instance_size */ sizeof(PJRT_ErrorImpl),
  /* extension_start */ nullptr,
  /* destroy */ ErrorDestroy,
  /* message */ ErrorMessage,
  /* get_code */ ErrorGetCode,
  /* for_each_payload */ ErrorForEachPayload,
};

static PJRT_Error* CreateError(PJRT_Error_Code code, const std::string& msg) {
  PJRT_ErrorImpl* err = new PJRT_ErrorImpl();
  err->vtable = &g_error_vtable;
  err->code = code;
  err->message = msg;
  return err;
}

extern "C" {

PJRT_Error* Impl_PJRT_Plugin_Initialize(PJRT_Plugin_Initialize_Args* args) {
  return nullptr;
}

PJRT_Error* Impl_PJRT_Plugin_Attributes(PJRT_Plugin_Attributes_Args* args) {
  args->attributes = nullptr;
  args->num_attributes = 0;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_Create(PJRT_Client_Create_Args* args) {
  try {
    PJRT_Client* client = new PJRT_Client();
    client->impl = std::make_unique<vulkan_pjrt::VulkanClientImpl>();
    client->device_handle.device = client->impl->device.get();
    client->device_handle.debug_string = std::string("Vulkan: ") + client->impl->device->device_name;

    auto mem = std::make_unique<PJRT_MemoryImpl>();
    mem->id = 0;
    mem->kind = "device";
    mem->debug_string = std::string("Vulkan Memory: ") + client->impl->device->device_name;
    mem->device = &client->device_handle;

    client->device_handle.memory_ptrs.push_back(mem.get());
    client->device_handle.memory = std::move(mem);

    client->device_ptrs.push_back(&client->device_handle);
    client->memory_ptrs.push_back(client->device_handle.memory.get());

    if (args) args->client = client;
    return nullptr;
  } catch (const std::exception& e) {
    return CreateError(PJRT_Error_Code_INTERNAL, std::string("Vulkan Client creation failed: ") + e.what());
  }
}

PJRT_Error* Impl_PJRT_Client_Destroy(PJRT_Client_Destroy_Args* args) {
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_PlatformName(PJRT_Client_PlatformName_Args* args) {
  if (args && args->client) {
    args->platform_name = args->client->platform_name.c_str();
    args->platform_name_size = args->client->platform_name.size();
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_PlatformVersion(PJRT_Client_PlatformVersion_Args* args) {
  if (args && args->client) {
    args->platform_version = args->client->platform_version.c_str();
    args->platform_version_size = args->client->platform_version.size();
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_ProcessIndex(PJRT_Client_ProcessIndex_Args* args) {
  if (args && args->client && args->client->impl) {
    args->process_index = args->client->impl->process_index;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_Devices(PJRT_Client_Devices_Args* args) {
  if (args && args->client) {
    args->devices = (PJRT_Device**)args->client->device_ptrs.data();
    args->num_devices = args->client->device_ptrs.size();
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_AddressableDevices(PJRT_Client_AddressableDevices_Args* args) {
  if (args && args->client) {
    args->addressable_devices = (PJRT_Device**)args->client->device_ptrs.data();
    args->num_addressable_devices = args->client->device_ptrs.size();
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_LookupDevice(PJRT_Client_LookupDevice_Args* args) {
  if (args && args->client && args->client->device_handle.device) {
    if (args->id == args->client->device_handle.device->id) {
      args->device = &args->client->device_handle;
      return nullptr;
    }
  }
  return CreateError(PJRT_Error_Code_NOT_FOUND, "Device not found");
}

PJRT_Error* Impl_PJRT_Client_LookupAddressableDevice(PJRT_Client_LookupAddressableDevice_Args* args) {
  if (args && args->client) {
    args->addressable_device = &args->client->device_handle;
    return nullptr;
  }
  return CreateError(PJRT_Error_Code_NOT_FOUND, "Addressable device not found");
}

PJRT_Error* Impl_PJRT_Client_AddressableMemories(PJRT_Client_AddressableMemories_Args* args) {
  if (args && args->client) {
    args->addressable_memories = (PJRT_Memory**)args->client->memory_ptrs.data();
    args->num_addressable_memories = args->client->memory_ptrs.size();
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_TopologyDescription(PJRT_Client_TopologyDescription_Args* args) {
  if (!args || !args->client) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null client");

  if (!args->client->topology) {
    auto topo = new PJRT_TopologyDescription();
    topo->client = args->client;
    for (auto dev : args->client->device_ptrs) {
      topo->device_descriptions.push_back((PJRT_DeviceDescription*)dev);
    }
    args->client->topology.reset(topo);
  }
  args->topology = args->client->topology.get();
  return nullptr;
}

PJRT_Error* Impl_PJRT_TopologyDescription_Destroy(PJRT_TopologyDescription_Destroy_Args* args) {
  return nullptr;
}

PJRT_Error* Impl_PJRT_TopologyDescription_PlatformName(PJRT_TopologyDescription_PlatformName_Args* args) {
  if (!args || !args->topology) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null topology");
  args->platform_name = args->topology->client->platform_name.c_str();
  args->platform_name_size = args->topology->client->platform_name.size();
  return nullptr;
}

PJRT_Error* Impl_PJRT_TopologyDescription_PlatformVersion(PJRT_TopologyDescription_PlatformVersion_Args* args) {
  if (!args || !args->topology) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null topology");
  args->platform_version = args->topology->client->platform_version.c_str();
  args->platform_version_size = args->topology->client->platform_version.size();
  return nullptr;
}

PJRT_Error* Impl_PJRT_TopologyDescription_GetDeviceDescriptions(PJRT_TopologyDescription_GetDeviceDescriptions_Args* args) {
  if (!args || !args->topology) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null topology");
  args->descriptions = args->topology->device_descriptions.data();
  args->num_descriptions = args->topology->device_descriptions.size();
  return nullptr;
}

PJRT_Error* Impl_PJRT_TopologyDescription_Attributes(PJRT_TopologyDescription_Attributes_Args* args) {
  args->attributes = nullptr;
  args->num_attributes = 0;
  return nullptr;
}

PJRT_Error* Impl_PJRT_DeviceDescription_Id(PJRT_DeviceDescription_Id_Args* args) {
  if (!args || !args->device_description) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null device");
  auto dev = (PJRT_Device*)args->device_description;
  args->id = dev->device->id;
  return nullptr;
}

PJRT_Error* Impl_PJRT_DeviceDescription_ProcessIndex(PJRT_DeviceDescription_ProcessIndex_Args* args) {
  args->process_index = 0;
  return nullptr;
}

PJRT_Error* Impl_PJRT_DeviceDescription_Kind(PJRT_DeviceDescription_Kind_Args* args) {
  if (!args || !args->device_description) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null device");
  auto dev = (PJRT_Device*)args->device_description;
  args->device_kind = dev->device_kind.c_str();
  args->device_kind_size = dev->device_kind.size();
  return nullptr;
}

PJRT_Error* Impl_PJRT_DeviceDescription_DebugString(PJRT_DeviceDescription_DebugString_Args* args) {
  if (!args || !args->device_description) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null device");
  auto dev = (PJRT_Device*)args->device_description;
  args->debug_string = dev->debug_string.c_str();
  args->debug_string_size = dev->debug_string.size();
  return nullptr;
}

PJRT_Error* Impl_PJRT_DeviceDescription_ToString(PJRT_DeviceDescription_ToString_Args* args) {
  if (!args || !args->device_description) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null device");
  auto dev = (PJRT_Device*)args->device_description;
  args->to_string = dev->debug_string.c_str();
  args->to_string_size = dev->debug_string.size();
  return nullptr;
}

PJRT_Error* Impl_PJRT_DeviceDescription_Attributes(PJRT_DeviceDescription_Attributes_Args* args) {
  args->attributes = nullptr;
  args->num_attributes = 0;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Device_GetDescription(PJRT_Device_GetDescription_Args* args) {
  if (!args || !args->device) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null device");
  args->device_description = (PJRT_DeviceDescription*)args->device;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Device_IsAddressable(PJRT_Device_IsAddressable_Args* args) {
  args->is_addressable = true;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Device_LocalHardwareId(PJRT_Device_LocalHardwareId_Args* args) {
  if (!args || !args->device) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null device");
  args->local_hardware_id = args->device->device->id;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Device_AddressableMemories(PJRT_Device_AddressableMemories_Args* args) {
  if (!args || !args->device) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null device");
  args->memories = (PJRT_Memory**)args->device->memory_ptrs.data();
  args->num_memories = args->device->memory_ptrs.size();
  return nullptr;
}

PJRT_Error* Impl_PJRT_Device_DefaultMemory(PJRT_Device_DefaultMemory_Args* args) {
  if (!args || !args->device) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null device");
  args->memory = args->device->memory.get();
  return nullptr;
}

PJRT_Error* Impl_PJRT_Device_MemoryStats(PJRT_Device_MemoryStats_Args* args) {
  if (args) {
    args->bytes_in_use = 0;
    args->peak_bytes_in_use = 0;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Memory_Id(PJRT_Memory_Id_Args* args) {
  if (!args || !args->memory) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null memory");
  auto mem = static_cast<const PJRT_MemoryImpl*>(args->memory);
  args->id = mem->id;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Memory_Kind(PJRT_Memory_Kind_Args* args) {
  if (!args || !args->memory) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null memory");
  auto mem = static_cast<const PJRT_MemoryImpl*>(args->memory);
  args->kind = mem->kind.c_str();
  args->kind_size = mem->kind.size();
  return nullptr;
}

PJRT_Error* Impl_PJRT_Memory_DebugString(PJRT_Memory_DebugString_Args* args) {
  if (!args || !args->memory) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null memory");
  auto mem = static_cast<const PJRT_MemoryImpl*>(args->memory);
  args->debug_string = mem->debug_string.c_str();
  args->debug_string_size = mem->debug_string.size();
  return nullptr;
}

PJRT_Error* Impl_PJRT_Memory_ToString(PJRT_Memory_ToString_Args* args) {
  return Impl_PJRT_Memory_DebugString((PJRT_Memory_DebugString_Args*)args);
}

PJRT_Error* Impl_PJRT_Memory_AddressableByDevices(PJRT_Memory_AddressableByDevices_Args* args) {
  if (!args || !args->memory) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null memory");
  auto mem = static_cast<const PJRT_MemoryImpl*>(args->memory);
  args->devices = (PJRT_Device**)&mem->device;
  args->num_devices = 1;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_Compile(PJRT_Client_Compile_Args* args) {
  if (!args || !args->client) {
    return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Invalid Compile args");
  }

  std::string code = "";
  std::string format = "";
  if (args->program && args->program->code && args->program->code_size > 0) {
    code.assign(args->program->code, args->program->code_size);
  }
  if (args->program && args->program->format && args->program->format_size > 0) {
    format.assign(args->program->format, args->program->format_size);
  }

  std::cout << "[PJRT Plugin Compile] Format: " << format << ", Code length: " << code.size() << std::endl;
  if (!code.empty()) {
    std::cout << "[PJRT MLIR Payload Snippet]:\n" << code.substr(0, std::min<size_t>(code.size(), 300)) << std::endl;
  }
  PJRT_LoadedExecutable* exec = new PJRT_LoadedExecutable();
  try {
    exec->impl = std::make_unique<vulkan_pjrt::VulkanExecutableImpl>(
        args->client->impl->device.get(), code, format);
  } catch (const std::exception& e) {
    std::cerr << "[CPP Compile Error] " << e.what() << std::endl;
  }

  if (args) args->executable = exec;
  return nullptr;
}

PJRT_Error* Impl_PJRT_LoadedExecutable_GetExecutable(PJRT_LoadedExecutable_GetExecutable_Args* args) {
  if (!args || !args->loaded_executable) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null executable");
  PJRT_Executable* exec = new PJRT_Executable();
  exec->impl = args->loaded_executable->impl.get();
  args->executable = exec;
  return nullptr;
}

PJRT_Error* Impl_PJRT_LoadedExecutable_AddressableDevices(PJRT_LoadedExecutable_AddressableDevices_Args* args) {
  if (!args || !args->executable) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null executable");
  args->addressable_devices = nullptr;
  args->num_addressable_devices = 0;
  return nullptr;
}

PJRT_Error* Impl_PJRT_LoadedExecutable_Delete(PJRT_LoadedExecutable_Delete_Args* args) {
  return nullptr;
}

PJRT_Error* Impl_PJRT_LoadedExecutable_IsDeleted(PJRT_LoadedExecutable_IsDeleted_Args* args) {
  if (args) args->is_deleted = false;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Executable_Destroy(PJRT_Executable_Destroy_Args* args) {
  if (args && args->executable) delete args->executable;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Executable_Name(PJRT_Executable_Name_Args* args) {
  if (!args) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null args");
  static const char* name = "vulkan_executable";
  args->executable_name = name;
  args->executable_name_size = std::strlen(name);
  return nullptr;
}

PJRT_Error* Impl_PJRT_Executable_NumReplicas(PJRT_Executable_NumReplicas_Args* args) {
  if (args) args->num_replicas = 1;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Executable_NumPartitions(PJRT_Executable_NumPartitions_Args* args) {
  if (args) args->num_partitions = 1;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Executable_NumOutputs(PJRT_Executable_NumOutputs_Args* args) {
  if (!args) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Null args");
  args->num_outputs = (args->executable && args->executable->impl) ? args->executable->impl->num_outputs : 1;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Executable_SizeOfGeneratedCodeInBytes(PJRT_Executable_SizeOfGeneratedCodeInBytes_Args* args) {
  if (args) args->size_in_bytes = 0;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Executable_GetCostAnalysis(PJRT_Executable_GetCostAnalysis_Args* args) {
  return nullptr;
}

PJRT_Error* Impl_PJRT_Executable_OptimizedProgram(PJRT_Executable_OptimizedProgram_Args* args) {
  if (args && args->program) {
    args->program->code_size = 0;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Executable_Serialize(PJRT_Executable_Serialize_Args* args) {
  return nullptr;
}

PJRT_Error* Impl_PJRT_LoadedExecutable_AddressableDeviceLogicalIds(PJRT_LoadedExecutable_AddressableDeviceLogicalIds_Args* args) {
  if (args) {
    args->addressable_device_logical_ids = nullptr;
    args->num_addressable_device_logical_ids = 0;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_LoadedExecutable_Fingerprint(PJRT_LoadedExecutable_Fingerprint_Args* args) {
  if (args) {
    args->executable_fingerprint = nullptr;
    args->executable_fingerprint_size = 0;
  }
  return nullptr;
}

static void DummyDeviceAssignmentDeleter(PJRT_DeviceAssignmentSerialized* da) {}

PJRT_Error* Impl_PJRT_LoadedExecutable_GetDeviceAssignment(PJRT_LoadedExecutable_GetDeviceAssignment_Args* args) {
  if (args) {
    static const char empty_da[] = "";
    args->serialized_bytes = empty_da;
    args->serialized_bytes_size = 0;
    args->serialized_device_assignment = nullptr;
    args->serialized_device_assignment_deleter = DummyDeviceAssignmentDeleter;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Executable_OutputMemoryKinds(PJRT_Executable_OutputMemoryKinds_Args* args) {
  if (args) {
    static const char* default_kind = "device";
    static const size_t default_kind_size = 6;
    static const char* kinds[1] = { default_kind };
    static const size_t kind_sizes[1] = { default_kind_size };
    args->memory_kinds = kinds;
    args->memory_kind_sizes = kind_sizes;
    args->num_outputs = 1;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_BufferFromHostBuffer(PJRT_Client_BufferFromHostBuffer_Args* args) {
  if (!args || !args->data) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Invalid BufferFromHostBuffer args");

  try {
    const vulkan_pjrt::VulkanDevice* dev = nullptr;
    PJRT_Device* device_handle_ptr = nullptr;
    PJRT_Memory* memory_handle_ptr = nullptr;

    if (args->client && args->client->impl) {
      dev = args->client->impl->device.get();
      device_handle_ptr = &args->client->device_handle;
      memory_handle_ptr = args->client->device_handle.memory.get();
    } else if (args->device) {
      dev = args->device->device;
      device_handle_ptr = args->device;
      memory_handle_ptr = args->device->memory.get();
    }

    auto buf_impl = std::make_unique<vulkan_pjrt::VulkanBufferImpl>(
        dev, (PJRT_Buffer_Type)args->type, args->dims, args->num_dims);

    if (device_handle_ptr) buf_impl->device_ptr = device_handle_ptr;
    if (memory_handle_ptr) buf_impl->memory_ptr = memory_handle_ptr;

    buf_impl->CopyFromHost(args->data, buf_impl->size_in_bytes);

    PJRT_Buffer* buf = new PJRT_Buffer();
    buf->impl = std::move(buf_impl);

    if (args) args->buffer = buf;

    PJRT_Event* evt = new PJRT_Event();
    evt->impl = std::make_unique<vulkan_pjrt::VulkanEventImpl>();
    args->done_with_host_buffer = evt;

    return nullptr;
  } catch (const std::exception& e) {
    return CreateError(PJRT_Error_Code_INTERNAL, std::string("Buffer creation failed: ") + e.what());
  }
}

PJRT_Error* Impl_PJRT_LoadedExecutable_Destroy(PJRT_LoadedExecutable_Destroy_Args* args) {
  if (args && args->executable) delete args->executable;
  return nullptr;
}

PJRT_Error* Impl_PJRT_LoadedExecutable_Execute(PJRT_LoadedExecutable_Execute_Args* args) {
  if (!args || !args->executable || !args->executable->impl) {
    return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Invalid Execute args");
  }

  try {
    size_t num_args = args->num_args;
    PJRT_Buffer* const* argument_handles = nullptr;
    if (args->argument_lists && args->argument_lists[0]) {
      argument_handles = args->argument_lists[0];
    }

    auto result_bufs = args->executable->impl->Execute(argument_handles, num_args);

    if (args->output_lists) {
      size_t num_devs = args->num_devices > 0 ? args->num_devices : 1;
      for (size_t d = 0; d < num_devs; ++d) {
        if (args->output_lists[d]) {
          for (size_t i = 0; i < result_bufs.size(); ++i) {
            args->output_lists[d][i] = result_bufs[i];
          }
        }
      }
    }

    return nullptr;
  } catch (const std::exception& e) {
    std::cerr << "[CPP Execute Error] " << e.what() << std::endl;
    return CreateError(PJRT_Error_Code_INTERNAL, std::string("Execution failed: ") + e.what());
  }
}

PJRT_Error* Impl_PJRT_Buffer_Destroy(PJRT_Buffer_Destroy_Args* args) {
  if (args && args->buffer) delete args->buffer;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_ToHostBuffer(PJRT_Buffer_ToHostBuffer_Args* args) {
  if (!args || !args->src || !args->src->impl || !args->dst) {
    return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Invalid ToHostBuffer args");
  }

  try {
    args->src->impl->CopyToHost(args->dst, args->dst_size);

    PJRT_Event* evt = new PJRT_Event();
    evt->impl = std::make_unique<vulkan_pjrt::VulkanEventImpl>();
    args->event = evt;

    return nullptr;
  } catch (const std::exception& e) {
    return CreateError(PJRT_Error_Code_INTERNAL, std::string("CopyToHost failed: ") + e.what());
  }
}

PJRT_Error* Impl_PJRT_Buffer_OnDeviceSizeInBytes(PJRT_Buffer_OnDeviceSizeInBytes_Args* args) {
  if (args && args->buffer && args->buffer->impl) {
    args->on_device_size_in_bytes = args->buffer->impl->size_in_bytes;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_Dimensions(PJRT_Buffer_Dimensions_Args* args) {
  if (args && args->buffer && args->buffer->impl) {
    args->dims = args->buffer->impl->dims.data();
    args->num_dims = args->buffer->impl->dims.size();
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_UnpaddedDimensions(PJRT_Buffer_UnpaddedDimensions_Args* args) {
  if (args && args->buffer && args->buffer->impl) {
    args->unpadded_dims = args->buffer->impl->dims.data();
    args->num_dims = args->buffer->impl->dims.size();
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_DynamicDimensionIndices(PJRT_Buffer_DynamicDimensionIndices_Args* args) {
  if (args) {
    args->dynamic_dim_indices = nullptr;
    args->num_dynamic_dims = 0;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_GetMemoryLayout(PJRT_Buffer_GetMemoryLayout_Args* args) {
  if (args) {
    std::memset(&args->layout, 0, sizeof(args->layout));
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_ElementType(PJRT_Buffer_ElementType_Args* args) {
  if (args && args->buffer && args->buffer->impl) {
    args->type = args->buffer->impl->element_type;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_Device(PJRT_Buffer_Device_Args* args) {
  if (args && args->buffer && args->buffer->impl) {
    args->device = args->buffer->impl->device_ptr;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_Memory(PJRT_Buffer_Memory_Args* args) {
  if (args && args->buffer && args->buffer->impl) {
    args->memory = args->buffer->impl->memory_ptr;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_Delete(PJRT_Buffer_Delete_Args* args) {
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_CopyToDevice(PJRT_Buffer_CopyToDevice_Args* args) {
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_IsDeleted(PJRT_Buffer_IsDeleted_Args* args) {
  if (args) args->is_deleted = false;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_IsOnCpu(PJRT_Buffer_IsOnCpu_Args* args) {
  if (args) args->is_on_cpu = false;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_ReadyEvent(PJRT_Buffer_ReadyEvent_Args* args) {
  if (args) {
    PJRT_Event* evt = new PJRT_Event();
    evt->impl = std::make_unique<vulkan_pjrt::VulkanEventImpl>();
    args->event = evt;
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Buffer_UnsafePointer(PJRT_Buffer_UnsafePointer_Args* args) {
  if (args && args->buffer && args->buffer->impl) {
    args->buffer_pointer = reinterpret_cast<uintptr_t>(args->buffer->impl->vk_buffer);
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Event_Destroy(PJRT_Event_Destroy_Args* args) {
  if (args && args->event) delete args->event;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Event_IsReady(PJRT_Event_IsReady_Args* args) {
  if (args && args->event && args->event->impl) {
    args->is_ready = args->event->impl->IsReady();
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Event_Error(PJRT_Event_Error_Args* args) {
  return nullptr;
}

PJRT_Error* Impl_PJRT_Event_Await(PJRT_Event_Await_Args* args) {
  if (args && args->event && args->event->impl) {
    args->event->impl->Await();
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Event_OnReady(PJRT_Event_OnReady_Args* args) {
  if (args && args->callback) {
    args->callback(nullptr, args->user_arg);
  }
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_DefaultDeviceAssignment(PJRT_Client_DefaultDeviceAssignment_Args* args) {
  if (args && args->default_assignment && args->num_replicas * args->num_partitions > 0) {
    std::memset(args->default_assignment, 0, sizeof(int) * args->num_replicas * args->num_partitions);
  }
  return nullptr;
}

static const PJRT_Api g_pjrt_api = []() {
  PJRT_Api api;
  std::memset(&api, 0, sizeof(api));

  api.struct_size = sizeof(PJRT_Api);
  api.pjrt_api_version.struct_size = sizeof(PJRT_Api_Version);
  api.pjrt_api_version.major_version = PJRT_API_MAJOR;
  api.pjrt_api_version.minor_version = PJRT_API_MINOR;

  api.PJRT_Plugin_Initialize = Impl_PJRT_Plugin_Initialize;
  api.PJRT_Plugin_Attributes = Impl_PJRT_Plugin_Attributes;

  api.PJRT_Client_Create = Impl_PJRT_Client_Create;
  api.PJRT_Client_Destroy = Impl_PJRT_Client_Destroy;
  api.PJRT_Client_PlatformName = Impl_PJRT_Client_PlatformName;
  api.PJRT_Client_PlatformVersion = Impl_PJRT_Client_PlatformVersion;
  api.PJRT_Client_ProcessIndex = Impl_PJRT_Client_ProcessIndex;
  api.PJRT_Client_Devices = Impl_PJRT_Client_Devices;
  api.PJRT_Client_AddressableDevices = Impl_PJRT_Client_AddressableDevices;
  api.PJRT_Client_LookupDevice = Impl_PJRT_Client_LookupDevice;
  api.PJRT_Client_LookupAddressableDevice = Impl_PJRT_Client_LookupAddressableDevice;
  api.PJRT_Client_AddressableMemories = Impl_PJRT_Client_AddressableMemories;
  api.PJRT_Client_TopologyDescription = Impl_PJRT_Client_TopologyDescription;
  api.PJRT_Client_BufferFromHostBuffer = Impl_PJRT_Client_BufferFromHostBuffer;
  api.PJRT_Client_Compile = Impl_PJRT_Client_Compile;

  api.PJRT_TopologyDescription_Destroy = Impl_PJRT_TopologyDescription_Destroy;
  api.PJRT_TopologyDescription_PlatformName = Impl_PJRT_TopologyDescription_PlatformName;
  api.PJRT_TopologyDescription_PlatformVersion = Impl_PJRT_TopologyDescription_PlatformVersion;
  api.PJRT_TopologyDescription_GetDeviceDescriptions = Impl_PJRT_TopologyDescription_GetDeviceDescriptions;
  api.PJRT_TopologyDescription_Attributes = Impl_PJRT_TopologyDescription_Attributes;

  api.PJRT_DeviceDescription_Id = Impl_PJRT_DeviceDescription_Id;
  api.PJRT_DeviceDescription_ProcessIndex = Impl_PJRT_DeviceDescription_ProcessIndex;
  api.PJRT_DeviceDescription_Kind = Impl_PJRT_DeviceDescription_Kind;
  api.PJRT_DeviceDescription_DebugString = Impl_PJRT_DeviceDescription_DebugString;
  api.PJRT_DeviceDescription_ToString = Impl_PJRT_DeviceDescription_ToString;
  api.PJRT_DeviceDescription_Attributes = Impl_PJRT_DeviceDescription_Attributes;

  api.PJRT_Device_GetDescription = Impl_PJRT_Device_GetDescription;
  api.PJRT_Device_IsAddressable = Impl_PJRT_Device_IsAddressable;
  api.PJRT_Device_LocalHardwareId = Impl_PJRT_Device_LocalHardwareId;
  api.PJRT_Device_AddressableMemories = Impl_PJRT_Device_AddressableMemories;
  api.PJRT_Device_DefaultMemory = Impl_PJRT_Device_DefaultMemory;
  api.PJRT_Device_MemoryStats = Impl_PJRT_Device_MemoryStats;

  api.PJRT_Memory_Id = Impl_PJRT_Memory_Id;
  api.PJRT_Memory_Kind = Impl_PJRT_Memory_Kind;
  api.PJRT_Memory_DebugString = Impl_PJRT_Memory_DebugString;
  api.PJRT_Memory_ToString = Impl_PJRT_Memory_ToString;
  api.PJRT_Memory_AddressableByDevices = Impl_PJRT_Memory_AddressableByDevices;

  api.PJRT_Executable_Destroy = Impl_PJRT_Executable_Destroy;
  api.PJRT_Executable_Name = Impl_PJRT_Executable_Name;
  api.PJRT_Executable_NumReplicas = Impl_PJRT_Executable_NumReplicas;
  api.PJRT_Executable_NumPartitions = Impl_PJRT_Executable_NumPartitions;
  api.PJRT_Executable_NumOutputs = Impl_PJRT_Executable_NumOutputs;
  api.PJRT_Executable_SizeOfGeneratedCodeInBytes = Impl_PJRT_Executable_SizeOfGeneratedCodeInBytes;
  api.PJRT_Executable_GetCostAnalysis = Impl_PJRT_Executable_GetCostAnalysis;
  api.PJRT_Executable_OutputMemoryKinds = Impl_PJRT_Executable_OutputMemoryKinds;
  api.PJRT_Executable_OptimizedProgram = Impl_PJRT_Executable_OptimizedProgram;
  api.PJRT_Executable_Serialize = Impl_PJRT_Executable_Serialize;

  api.PJRT_LoadedExecutable_Destroy = Impl_PJRT_LoadedExecutable_Destroy;
  api.PJRT_LoadedExecutable_GetExecutable = Impl_PJRT_LoadedExecutable_GetExecutable;
  api.PJRT_LoadedExecutable_AddressableDevices = Impl_PJRT_LoadedExecutable_AddressableDevices;
  api.PJRT_LoadedExecutable_Delete = Impl_PJRT_LoadedExecutable_Delete;
  api.PJRT_LoadedExecutable_IsDeleted = Impl_PJRT_LoadedExecutable_IsDeleted;
  api.PJRT_LoadedExecutable_Execute = Impl_PJRT_LoadedExecutable_Execute;
  api.PJRT_LoadedExecutable_Fingerprint = Impl_PJRT_LoadedExecutable_Fingerprint;
  api.PJRT_LoadedExecutable_GetDeviceAssignment = Impl_PJRT_LoadedExecutable_GetDeviceAssignment;
  api.PJRT_LoadedExecutable_AddressableDeviceLogicalIds = Impl_PJRT_LoadedExecutable_AddressableDeviceLogicalIds;

  api.PJRT_Buffer_Destroy = Impl_PJRT_Buffer_Destroy;
  api.PJRT_Buffer_ElementType = Impl_PJRT_Buffer_ElementType;
  api.PJRT_Buffer_Dimensions = Impl_PJRT_Buffer_Dimensions;
  api.PJRT_Buffer_UnpaddedDimensions = Impl_PJRT_Buffer_UnpaddedDimensions;
  api.PJRT_Buffer_DynamicDimensionIndices = Impl_PJRT_Buffer_DynamicDimensionIndices;
  api.PJRT_Buffer_GetMemoryLayout = Impl_PJRT_Buffer_GetMemoryLayout;
  api.PJRT_Buffer_OnDeviceSizeInBytes = Impl_PJRT_Buffer_OnDeviceSizeInBytes;
  api.PJRT_Buffer_ToHostBuffer = Impl_PJRT_Buffer_ToHostBuffer;
  api.PJRT_Buffer_Device = Impl_PJRT_Buffer_Device;
  api.PJRT_Buffer_Memory = Impl_PJRT_Buffer_Memory;
  api.PJRT_Buffer_Delete = Impl_PJRT_Buffer_Delete;
  api.PJRT_Buffer_IsDeleted = Impl_PJRT_Buffer_IsDeleted;
  api.PJRT_Buffer_CopyToDevice = Impl_PJRT_Buffer_CopyToDevice;
  api.PJRT_Buffer_IsOnCpu = Impl_PJRT_Buffer_IsOnCpu;
  api.PJRT_Buffer_ReadyEvent = Impl_PJRT_Buffer_ReadyEvent;
  api.PJRT_Buffer_UnsafePointer = Impl_PJRT_Buffer_UnsafePointer;

  api.PJRT_Event_Destroy = Impl_PJRT_Event_Destroy;
  api.PJRT_Event_IsReady = Impl_PJRT_Event_IsReady;
  api.PJRT_Event_Error = Impl_PJRT_Event_Error;
  api.PJRT_Event_Await = Impl_PJRT_Event_Await;
  api.PJRT_Event_OnReady = Impl_PJRT_Event_OnReady;

  return api;
}();

const PJRT_Api* GetPjrtApi() {
  return &g_pjrt_api;
}

}  // extern "C"
