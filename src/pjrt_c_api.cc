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

  PJRT_LoadedExecutable* exec = new PJRT_LoadedExecutable();
  try {
    exec->impl = std::make_unique<vulkan_pjrt::VulkanExecutableImpl>(
        args->client->impl->device.get(), code, format);
  } catch (...) {
  }

  if (args) args->executable = exec;
  return nullptr;
}

PJRT_Error* Impl_PJRT_Client_BufferFromHostBuffer(PJRT_Client_BufferFromHostBuffer_Args* args) {
  if (!args || !args->data) return CreateError(PJRT_Error_Code_INVALID_ARGUMENT, "Invalid BufferFromHostBuffer args");

  try {
    const vulkan_pjrt::VulkanDevice* dev = args->client ? args->client->impl->device.get() : nullptr;
    auto buf_impl = std::make_unique<vulkan_pjrt::VulkanBufferImpl>(
        dev, (PJRT_Buffer_Type)args->type, args->dims, args->num_dims);

    if (args->client) {
      buf_impl->device_ptr = &args->client->device_handle;
      buf_impl->memory_ptr = args->client->device_handle.memory.get();
    }

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

    if (args->output_lists && args->output_lists[0] && !result_bufs.empty()) {
      args->output_lists[0][0] = result_bufs[0];
    }

    return nullptr;
  } catch (const std::exception& e) {
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
  api.PJRT_Buffer_IsDeleted = Impl_PJRT_Buffer_IsDeleted;
  api.PJRT_Buffer_IsOnCpu = Impl_PJRT_Buffer_IsOnCpu;
  api.PJRT_Buffer_ReadyEvent = Impl_PJRT_Buffer_ReadyEvent;
  api.PJRT_Buffer_UnsafePointer = Impl_PJRT_Buffer_UnsafePointer;

  api.PJRT_LoadedExecutable_Destroy = Impl_PJRT_LoadedExecutable_Destroy;
  api.PJRT_LoadedExecutable_Execute = Impl_PJRT_LoadedExecutable_Execute;

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
