// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUReconstructionOCL.cxx
/// \author David Rohr

#define GPUCA_GPUTYPE_OPENCL
#define __OPENCL_HOST__

#include "GPUReconstructionOCL.h"
#include "GPUReconstructionOCLInternals.h"
#include "GPUReconstructionIncludes.h"

using namespace GPUCA_NAMESPACE::gpu;

#include <cstring>
#include <unistd.h>
#include <typeinfo>
#include <cstdlib>

#define quit(...)          \
  {                        \
    GPUError(__VA_ARGS__); \
    return (1);            \
  }

#define GPUCA_KRNL(x_class, x_attributes, ...) GPUCA_KRNL_PROP(x_class, x_attributes)
#define GPUCA_KRNL_BACKEND_CLASS GPUReconstructionOCL
#include "GPUReconstructionKernelList.h"
#undef GPUCA_KRNL

GPUReconstructionOCL::GPUReconstructionOCL(const GPUSettingsDeviceBackend& cfg) : GPUReconstructionDeviceBase(cfg, sizeof(GPUReconstructionDeviceBase))
{
  if (mMaster == nullptr) {
    mInternals = new GPUReconstructionOCLInternals;
  }
  mDeviceBackendSettings.deviceType = DeviceType::OCL;
}

GPUReconstructionOCL::~GPUReconstructionOCL()
{
  Exit(); // Make sure we destroy everything (in particular the ITS tracker) before we exit
  if (mMaster == nullptr) {
    delete mInternals;
  }
}

int32_t GPUReconstructionOCL::GPUFailedMsgAI(const int64_t error, const char* file, int32_t line)
{
  // Check for OPENCL Error and in the case of an error display the corresponding error string
  if (error == CL_SUCCESS) {
    return (0);
  }
  GPUError("OCL Error: %ld / %s (%s:%d)", error, opencl_error_string(error), file, line);
  return 1;
}

void GPUReconstructionOCL::GPUFailedMsgA(const int64_t error, const char* file, int32_t line)
{
  if (GPUFailedMsgAI(error, file, line)) {
    static bool runningCallbacks = false;
    if (IsInitialized() && runningCallbacks == false) {
      runningCallbacks = true;
      CheckErrorCodes(false, true);
    }
    throw std::runtime_error("OpenCL Failure");
  }
}

void GPUReconstructionOCL::UpdateAutomaticProcessingSettings()
{
  GPUCA_GPUReconstructionUpdateDefaults();
}

int32_t GPUReconstructionOCL::InitDevice_Runtime()
{
  if (mMaster == nullptr) {
    cl_int ocl_error;
    cl_uint num_platforms;
    if (GPUFailedMsgI(clGetPlatformIDs(0, nullptr, &num_platforms))) {
      quit("Error getting OpenCL Platform Count");
    }
    if (num_platforms == 0) {
      quit("No OpenCL Platform found");
    }
    if (mProcessingSettings.debugLevel >= 2) {
      GPUInfo("%d OpenCL Platforms found", num_platforms);
    }

    // Query platforms
    mInternals->platforms.reset(new cl_platform_id[num_platforms]);
    if (GPUFailedMsgI(clGetPlatformIDs(num_platforms, mInternals->platforms.get(), nullptr))) {
      quit("Error getting OpenCL Platforms");
    }

    bool found = false;
    if (mProcessingSettings.platformNum >= 0) {
      if (mProcessingSettings.platformNum >= (int32_t)num_platforms) {
        quit("Invalid platform specified");
      }
      mInternals->platform = mInternals->platforms[mProcessingSettings.platformNum];
      found = true;
      if (mProcessingSettings.debugLevel >= 2) {
        char platform_profile[256] = {}, platform_version[256] = {}, platform_name[256] = {}, platform_vendor[256] = {};
        clGetPlatformInfo(mInternals->platform, CL_PLATFORM_PROFILE, sizeof(platform_profile), platform_profile, nullptr);
        clGetPlatformInfo(mInternals->platform, CL_PLATFORM_VERSION, sizeof(platform_version), platform_version, nullptr);
        clGetPlatformInfo(mInternals->platform, CL_PLATFORM_NAME, sizeof(platform_name), platform_name, nullptr);
        clGetPlatformInfo(mInternals->platform, CL_PLATFORM_VENDOR, sizeof(platform_vendor), platform_vendor, nullptr);
        GPUInfo("Selected Platform %d: (%s %s) %s %s", mProcessingSettings.platformNum, platform_profile, platform_version, platform_vendor, platform_name);
      }
    } else {
      for (uint32_t i_platform = 0; i_platform < num_platforms; i_platform++) {
        char platform_profile[256] = {}, platform_version[256] = {}, platform_name[256] = {}, platform_vendor[256] = {};
        clGetPlatformInfo(mInternals->platforms[i_platform], CL_PLATFORM_PROFILE, sizeof(platform_profile), platform_profile, nullptr);
        clGetPlatformInfo(mInternals->platforms[i_platform], CL_PLATFORM_VERSION, sizeof(platform_version), platform_version, nullptr);
        clGetPlatformInfo(mInternals->platforms[i_platform], CL_PLATFORM_NAME, sizeof(platform_name), platform_name, nullptr);
        clGetPlatformInfo(mInternals->platforms[i_platform], CL_PLATFORM_VENDOR, sizeof(platform_vendor), platform_vendor, nullptr);
        const char* platformUsageInfo = "";
        if (!found && CheckPlatform(i_platform)) {
          found = true;
          mInternals->platform = mInternals->platforms[i_platform];
          if (mProcessingSettings.debugLevel >= 2) {
            platformUsageInfo = "    !!! Using this platform !!!";
          }
        }
        if (mProcessingSettings.debugLevel >= 2) {
          GPUInfo("Available Platform %d: (%s %s) %s %s%s", i_platform, platform_profile, platform_version, platform_vendor, platform_name, platformUsageInfo);
        }
      }
    }

    if (found == false) {
      quit("Did not find compatible OpenCL Platform");
    }

    cl_uint count, bestDevice = (cl_uint)-1;
    if (GPUFailedMsgI(clGetDeviceIDs(mInternals->platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &count))) {
      quit("Error getting OPENCL Device Count");
    }

    // Query devices
    mInternals->devices.reset(new cl_device_id[count]);
    if (GPUFailedMsgI(clGetDeviceIDs(mInternals->platform, CL_DEVICE_TYPE_ALL, count, mInternals->devices.get(), nullptr))) {
      quit("Error getting OpenCL devices");
    }

    char device_vendor[64], device_name[64];
    cl_device_type device_type;
    cl_uint freq, shaders;

    if (mProcessingSettings.debugLevel >= 2) {
      GPUInfo("Available OPENCL devices:");
    }
    std::vector<bool> devicesOK(count, false);
    for (uint32_t i = 0; i < count; i++) {
      if (mProcessingSettings.debugLevel >= 3) {
        GPUInfo("Examining device %d", i);
      }
      cl_uint nbits;
      cl_bool endian;

      clGetDeviceInfo(mInternals->devices[i], CL_DEVICE_NAME, 64, device_name, nullptr);
      clGetDeviceInfo(mInternals->devices[i], CL_DEVICE_VENDOR, 64, device_vendor, nullptr);
      clGetDeviceInfo(mInternals->devices[i], CL_DEVICE_TYPE, sizeof(cl_device_type), &device_type, nullptr);
      clGetDeviceInfo(mInternals->devices[i], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(freq), &freq, nullptr);
      clGetDeviceInfo(mInternals->devices[i], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(shaders), &shaders, nullptr);
      clGetDeviceInfo(mInternals->devices[i], CL_DEVICE_ADDRESS_BITS, sizeof(nbits), &nbits, nullptr);
      clGetDeviceInfo(mInternals->devices[i], CL_DEVICE_ENDIAN_LITTLE, sizeof(endian), &endian, nullptr);
      int32_t deviceOK = true;
      const char* deviceFailure = "";
      if (mProcessingSettings.gpuDeviceOnly && ((device_type & CL_DEVICE_TYPE_CPU) || !(device_type & CL_DEVICE_TYPE_GPU))) {
        deviceOK = false;
        deviceFailure = "No GPU device";
      }
      if (nbits / 8 != sizeof(void*)) {
        deviceOK = false;
        deviceFailure = "No 64 bit device";
      }
      if (!endian) {
        deviceOK = false;
        deviceFailure = "No Little Endian Mode";
      }

      double bestDeviceSpeed = -1, deviceSpeed = (double)freq * (double)shaders;
      if (mProcessingSettings.debugLevel >= 2) {
        GPUImportant("Device %s%2d: %s %s (Frequency %d, Shaders %d, %d bit) (Speed Value: %ld)%s %s", deviceOK ? " " : "[", i, device_vendor, device_name, (int32_t)freq, (int32_t)shaders, (int32_t)nbits, (int64_t)deviceSpeed, deviceOK ? " " : " ]", deviceOK ? "" : deviceFailure);
      }
      if (!deviceOK) {
        continue;
      }
      devicesOK[i] = true;
      if (deviceSpeed > bestDeviceSpeed) {
        bestDevice = i;
        bestDeviceSpeed = deviceSpeed;
      } else {
        if (mProcessingSettings.debugLevel >= 2) {
          GPUInfo("Skipping: Speed %f < %f", deviceSpeed, bestDeviceSpeed);
        }
      }
    }
    if (bestDevice == (cl_uint)-1) {
      quit("No %sOPENCL Device available, aborting OPENCL Initialisation", count ? "appropriate " : "");
    }

    if (mProcessingSettings.deviceNum > -1) {
      if (mProcessingSettings.deviceNum >= (signed)count) {
        quit("Requested device ID %d does not exist", mProcessingSettings.deviceNum);
      } else if (!devicesOK[mProcessingSettings.deviceNum]) {
        quit("Unsupported device requested (%d)", mProcessingSettings.deviceNum);
      } else {
        bestDevice = mProcessingSettings.deviceNum;
      }
    }
    mInternals->device = mInternals->devices[bestDevice];

    cl_ulong constantBuffer, globalMem, localMem;
    char deviceVersion[64];
    size_t maxWorkGroup, maxWorkItems[3];
    clGetDeviceInfo(mInternals->device, CL_DEVICE_NAME, 64, device_name, nullptr);
    clGetDeviceInfo(mInternals->device, CL_DEVICE_VENDOR, 64, device_vendor, nullptr);
    clGetDeviceInfo(mInternals->device, CL_DEVICE_TYPE, sizeof(cl_device_type), &device_type, nullptr);
    clGetDeviceInfo(mInternals->device, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(freq), &freq, nullptr);
    clGetDeviceInfo(mInternals->device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(shaders), &shaders, nullptr);
    clGetDeviceInfo(mInternals->device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(globalMem), &globalMem, nullptr);
    clGetDeviceInfo(mInternals->device, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(constantBuffer), &constantBuffer, nullptr);
    clGetDeviceInfo(mInternals->device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(localMem), &localMem, nullptr);
    clGetDeviceInfo(mInternals->device, CL_DEVICE_VERSION, sizeof(deviceVersion) - 1, deviceVersion, nullptr);
    clGetDeviceInfo(mInternals->device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroup), &maxWorkGroup, nullptr);
    clGetDeviceInfo(mInternals->device, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(maxWorkItems), maxWorkItems, nullptr);
    int versionMajor, versionMinor;
    sscanf(deviceVersion, "OpenCL %d.%d", &versionMajor, &versionMinor);
    if (mProcessingSettings.debugLevel >= 2) {
      GPUInfo("Using OpenCL device %d: %s %s with properties:", bestDevice, device_vendor, device_name);
      GPUInfo("\tVersion = %s", deviceVersion);
      GPUInfo("\tFrequency = %d", (int32_t)freq);
      GPUInfo("\tShaders = %d", (int32_t)shaders);
      GPUInfo("\tGLobalMemory = %ld", (int64_t)globalMem);
      GPUInfo("\tContantMemoryBuffer = %ld", (int64_t)constantBuffer);
      GPUInfo("\tLocalMemory = %ld", (int64_t)localMem);
      GPUInfo("\tmaxThreadsPerBlock = %ld", (int64_t)maxWorkGroup);
      GPUInfo("\tmaxThreadsDim = %ld %ld %ld", (int64_t)maxWorkItems[0], (int64_t)maxWorkItems[1], (int64_t)maxWorkItems[2]);
      GPUInfo(" ");
    }
#ifndef GPUCA_NO_CONSTANT_MEMORY
    if (gGPUConstantMemBufferSize > constantBuffer) {
      quit("Insufficient constant memory available on GPU %d < %d!", (int32_t)constantBuffer, (int32_t)gGPUConstantMemBufferSize);
    }
#endif

    mDeviceName = device_name;
    mDeviceName += " (OpenCL)";
    mBlockCount = shaders;
    mWarpSize = 32;
    mMaxThreads = std::max<int32_t>(mMaxThreads, maxWorkGroup * mBlockCount);

    mInternals->context = clCreateContext(nullptr, ContextForAllPlatforms() ? count : 1, ContextForAllPlatforms() ? mInternals->devices.get() : &mInternals->device, nullptr, nullptr, &ocl_error);
    if (GPUFailedMsgI(ocl_error)) {
      quit("Could not create OPENCL Device Context!");
    }

    if (GetOCLPrograms()) {
      return 1;
    }

    if (mProcessingSettings.debugLevel >= 2) {
      GPUInfo("OpenCL program and kernels loaded successfully");
    }

    mInternals->mem_gpu = clCreateBuffer(mInternals->context, CL_MEM_READ_WRITE, mDeviceMemorySize, nullptr, &ocl_error);
    if (GPUFailedMsgI(ocl_error)) {
      clReleaseContext(mInternals->context);
      quit("OPENCL Memory Allocation Error");
    }

    mInternals->mem_constant = clCreateBuffer(mInternals->context, CL_MEM_READ_ONLY, gGPUConstantMemBufferSize, nullptr, &ocl_error);
    if (GPUFailedMsgI(ocl_error)) {
      clReleaseMemObject(mInternals->mem_gpu);
      clReleaseContext(mInternals->context);
      quit("OPENCL Constant Memory Allocation Error");
    }

    if (device_type & CL_DEVICE_TYPE_CPU) {
      if (mProcessingSettings.deviceTimers && mProcessingSettings.debugLevel >= 2) {
        GPUInfo("Disabling device timers for CPU device");
      }
      mProcessingSettings.deviceTimers = 0;
    }
    for (int32_t i = 0; i < mNStreams; i++) {
#ifdef CL_VERSION_2_0
      cl_queue_properties prop = 0;
      if (versionMajor >= 2 && IsGPU() && mProcessingSettings.deviceTimers) {
        prop |= CL_QUEUE_PROFILING_ENABLE;
      }
      mInternals->command_queue[i] = clCreateCommandQueueWithProperties(mInternals->context, mInternals->device, &prop, &ocl_error);
      if (mProcessingSettings.deviceTimers && ocl_error == CL_INVALID_QUEUE_PROPERTIES) {
        GPUError("GPU device timers not supported by OpenCL platform, disabling");
        mProcessingSettings.deviceTimers = 0;
        prop = 0;
        mInternals->command_queue[i] = clCreateCommandQueueWithProperties(mInternals->context, mInternals->device, &prop, &ocl_error);
      }
#else
      mInternals->command_queue[i] = clCreateCommandQueue(mInternals->context, mInternals->device, 0, &ocl_error);
#endif
      if (GPUFailedMsgI(ocl_error)) {
        quit("Error creating OpenCL command queue");
      }
    }
    if (GPUFailedMsgI(clEnqueueMigrateMemObjects(mInternals->command_queue[0], 1, &mInternals->mem_gpu, 0, 0, nullptr, nullptr))) {
      quit("Error migrating buffer");
    }
    if (GPUFailedMsgI(clEnqueueMigrateMemObjects(mInternals->command_queue[0], 1, &mInternals->mem_constant, 0, 0, nullptr, nullptr))) {
      quit("Error migrating buffer");
    }

    mInternals->mem_host = clCreateBuffer(mInternals->context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, mHostMemorySize, nullptr, &ocl_error);
    if (GPUFailedMsgI(ocl_error)) {
      quit("Error allocating pinned host memory");
    }

    const char* krnlGetPtr = "__kernel void krnlGetPtr(__global char* gpu_mem, __global char* constant_mem, __global size_t* host_mem) {if (get_global_id(0) == 0) {host_mem[0] = (size_t) gpu_mem; host_mem[1] = (size_t) constant_mem;}}";
    cl_program program = clCreateProgramWithSource(mInternals->context, 1, (const char**)&krnlGetPtr, nullptr, &ocl_error);
    if (GPUFailedMsgI(ocl_error)) {
      quit("Error creating program object");
    }
    ocl_error = clBuildProgram(program, 1, &mInternals->device, "", nullptr, nullptr);
    if (GPUFailedMsgI(ocl_error)) {
      char build_log[16384];
      clGetProgramBuildInfo(program, mInternals->device, CL_PROGRAM_BUILD_LOG, 16384, build_log, nullptr);
      GPUImportant("Build Log:\n\n%s\n\n", build_log);
      quit("Error compiling program");
    }
    cl_kernel kernel = clCreateKernel(program, "krnlGetPtr", &ocl_error);
    if (GPUFailedMsgI(ocl_error)) {
      quit("Error creating kernel");
    }

    if (GPUFailedMsgI(OCLsetKernelParameters(kernel, mInternals->mem_gpu, mInternals->mem_constant, mInternals->mem_host)) ||
        GPUFailedMsgI(clExecuteKernelA(mInternals->command_queue[0], kernel, 16, 16, nullptr)) ||
        GPUFailedMsgI(clFinish(mInternals->command_queue[0])) ||
        GPUFailedMsgI(clReleaseKernel(kernel)) ||
        GPUFailedMsgI(clReleaseProgram(program))) {
      quit("Error obtaining device memory ptr");
    }

    if (mProcessingSettings.debugLevel >= 2) {
      GPUInfo("Mapping hostmemory");
    }
    mHostMemoryBase = clEnqueueMapBuffer(mInternals->command_queue[0], mInternals->mem_host, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0, mHostMemorySize, 0, nullptr, nullptr, &ocl_error);
    if (GPUFailedMsgI(ocl_error)) {
      quit("Error allocating Page Locked Host Memory");
    }

    mDeviceMemoryBase = ((void**)mHostMemoryBase)[0];
    mDeviceConstantMem = (GPUConstantMem*)((void**)mHostMemoryBase)[1];

    if (mProcessingSettings.debugLevel >= 1) {
      GPUInfo("Memory ptrs: GPU (%ld bytes): %p - Host (%ld bytes): %p", (int64_t)mDeviceMemorySize, mDeviceMemoryBase, (int64_t)mHostMemorySize, mHostMemoryBase);
      memset(mHostMemoryBase, 0xDD, mHostMemorySize);
    }

    GPUInfo("OPENCL Initialisation successfull (%d: %s %s (Frequency %d, Shaders %d), %ld / %ld bytes host / global memory, Stack frame %d, Constant memory %ld)", bestDevice, device_vendor, device_name, (int32_t)freq, (int32_t)shaders, (int64_t)mDeviceMemorySize,
            (int64_t)mHostMemorySize, -1, (int64_t)gGPUConstantMemBufferSize);
  } else {
    GPUReconstructionOCL* master = dynamic_cast<GPUReconstructionOCL*>(mMaster);
    mBlockCount = master->mBlockCount;
    mWarpSize = master->mWarpSize;
    mMaxThreads = master->mMaxThreads;
    mDeviceName = master->mDeviceName;
    mDeviceConstantMem = master->mDeviceConstantMem;
    mInternals = master->mInternals;
  }

  for (uint32_t i = 0; i < mEvents.size(); i++) {
    cl_event* events = (cl_event*)mEvents[i].data();
    new (events) cl_event[mEvents[i].size()];
  }

  return (0);
}

int32_t GPUReconstructionOCL::ExitDevice_Runtime()
{
  // Uninitialize OPENCL
  SynchronizeGPU();

  if (mMaster == nullptr) {
    if (mDeviceMemoryBase) {
      clReleaseMemObject(mInternals->mem_gpu);
      clReleaseMemObject(mInternals->mem_constant);
      for (uint32_t i = 0; i < mInternals->kernels.size(); i++) {
        clReleaseKernel(mInternals->kernels[i].first);
      }
      mInternals->kernels.clear();
    }
    if (mHostMemoryBase) {
      clEnqueueUnmapMemObject(mInternals->command_queue[0], mInternals->mem_host, mHostMemoryBase, 0, nullptr, nullptr);
      for (int32_t i = 0; i < mNStreams; i++) {
        clReleaseCommandQueue(mInternals->command_queue[i]);
      }
      clReleaseMemObject(mInternals->mem_host);
    }

    clReleaseProgram(mInternals->program);
    clReleaseContext(mInternals->context);
    GPUInfo("OPENCL Uninitialized");
  }
  mDeviceMemoryBase = nullptr;
  mHostMemoryBase = nullptr;

  return (0);
}

size_t GPUReconstructionOCL::GPUMemCpy(void* dst, const void* src, size_t size, int32_t stream, int32_t toGPU, deviceEvent* ev, deviceEvent* evList, int32_t nEvents)
{
  if (evList == nullptr) {
    nEvents = 0;
  }
  if (mProcessingSettings.debugLevel >= 3) {
    stream = -1;
  }
  if (stream == -1) {
    SynchronizeGPU();
  }
  if (toGPU == -2) {
    GPUFailedMsg(clEnqueueCopyBuffer(mInternals->command_queue[stream == -1 ? 0 : stream], mInternals->mem_gpu, mInternals->mem_gpu, (char*)src - (char*)mDeviceMemoryBase, (char*)dst - (char*)mDeviceMemoryBase, size, nEvents, evList->getEventList<cl_event>(), ev->getEventList<cl_event>()));
  } else if (toGPU) {
    GPUFailedMsg(clEnqueueWriteBuffer(mInternals->command_queue[stream == -1 ? 0 : stream], mInternals->mem_gpu, stream == -1, (char*)dst - (char*)mDeviceMemoryBase, size, src, nEvents, evList->getEventList<cl_event>(), ev->getEventList<cl_event>()));
  } else {
    GPUFailedMsg(clEnqueueReadBuffer(mInternals->command_queue[stream == -1 ? 0 : stream], mInternals->mem_gpu, stream == -1, (char*)src - (char*)mDeviceMemoryBase, size, dst, nEvents, evList->getEventList<cl_event>(), ev->getEventList<cl_event>()));
  }
  if (mProcessingSettings.serializeGPU & 2) {
    GPUDebug(("GPUMemCpy " + std::to_string(toGPU)).c_str(), stream, true);
  }
  return size;
}

size_t GPUReconstructionOCL::WriteToConstantMemory(size_t offset, const void* src, size_t size, int32_t stream, deviceEvent* ev)
{
  if (stream == -1) {
    SynchronizeGPU();
  }
  GPUFailedMsg(clEnqueueWriteBuffer(mInternals->command_queue[stream == -1 ? 0 : stream], mInternals->mem_constant, stream == -1, offset, size, src, 0, nullptr, ev->getEventList<cl_event>()));
  if (mProcessingSettings.serializeGPU & 2) {
    GPUDebug("WriteToConstantMemory", stream, true);
  }
  return size;
}

void GPUReconstructionOCL::ReleaseEvent(deviceEvent ev) { GPUFailedMsg(clReleaseEvent(ev.get<cl_event>())); }

void GPUReconstructionOCL::RecordMarker(deviceEvent* ev, int32_t stream) { GPUFailedMsg(clEnqueueMarkerWithWaitList(mInternals->command_queue[stream], 0, nullptr, ev->getEventList<cl_event>())); }

int32_t GPUReconstructionOCL::DoStuckProtection(int32_t stream, deviceEvent event)
{
  if (mProcessingSettings.stuckProtection) {
    cl_int tmp = 0;
    for (int32_t i = 0; i <= mProcessingSettings.stuckProtection / 50; i++) {
      usleep(50);
      clGetEventInfo(event.get<cl_event>(), CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(tmp), &tmp, nullptr);
      if (tmp == CL_COMPLETE) {
        break;
      }
    }
    if (tmp != CL_COMPLETE) {
      mGPUStuck = 1;
      quit("GPU Stuck, future processing in this component is disabled, skipping event (GPU Event State %d)", (int32_t)tmp);
    }
  } else {
    clFinish(mInternals->command_queue[stream]);
  }
  return 0;
}

void GPUReconstructionOCL::SynchronizeGPU()
{
  for (int32_t i = 0; i < mNStreams; i++) {
    GPUFailedMsg(clFinish(mInternals->command_queue[i]));
  }
}

void GPUReconstructionOCL::SynchronizeStream(int32_t stream) { GPUFailedMsg(clFinish(mInternals->command_queue[stream])); }

void GPUReconstructionOCL::SynchronizeEvents(deviceEvent* evList, int32_t nEvents) { GPUFailedMsg(clWaitForEvents(nEvents, evList->getEventList<cl_event>())); }

void GPUReconstructionOCL::StreamWaitForEvents(int32_t stream, deviceEvent* evList, int32_t nEvents)
{
  if (nEvents) {
    GPUFailedMsg(clEnqueueMarkerWithWaitList(mInternals->command_queue[stream], nEvents, evList->getEventList<cl_event>(), nullptr));
  }
}

bool GPUReconstructionOCL::IsEventDone(deviceEvent* evList, int32_t nEvents)
{
  cl_int eventdone;
  for (int32_t i = 0; i < nEvents; i++) {
    GPUFailedMsg(clGetEventInfo(evList[i].get<cl_event>(), CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(eventdone), &eventdone, nullptr));
    if (eventdone != CL_COMPLETE) {
      return false;
    }
  }
  return true;
}

int32_t GPUReconstructionOCL::GPUDebug(const char* state, int32_t stream, bool force)
{
  // Wait for OPENCL-Kernel to finish and check for OPENCL errors afterwards, in case of debugmode
  if (!force && mProcessingSettings.debugLevel <= 0) {
    return (0);
  }
  for (int32_t i = 0; i < mNStreams; i++) {
    if (GPUFailedMsgI(clFinish(mInternals->command_queue[i]))) {
      GPUError("OpenCL Error while synchronizing (%s) (Stream %d/%d)", state, stream, i);
    }
  }
  if (mProcessingSettings.debugLevel >= 3) {
    GPUInfo("GPU Sync Done");
  }
  return (0);
}
