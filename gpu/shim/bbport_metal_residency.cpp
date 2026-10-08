// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_metal_residency.h"

#ifdef __APPLE__
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <objc/message.h>
#include <objc/runtime.h>
#include <vulkan/vulkan_metal.h>
#include "video_core/renderer_vulkan/vk_instance.h"
#endif

namespace BbMetalResidency {

#ifdef __APPLE__
namespace {

id Send(id object, const char* selector) {
    return reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(object, sel_registerName(selector));
}

void SendObject(id object, const char* selector, id argument) {
    reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(object, sel_registerName(selector),
                                                          argument);
}

struct State {
    std::mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkExportMetalObjectsEXT export_objects = nullptr;
    id set = nullptr;
    /// Allocations in the set: memory (MTLBuffer and its count) and images (MTLTexture).
    std::unordered_map<VkDeviceMemory, std::pair<id, u32>> memories;
    std::unordered_map<VkImage, id> images;
};

State& Get() {
    static State state;
    return state;
}

id ExportBuffer(State& s, VkDeviceMemory memory) {
    VkExportMetalBufferInfoEXT info{
        .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_BUFFER_INFO_EXT,
        .memory = memory,
    };
    VkExportMetalObjectsInfoEXT objects{
        .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT,
        .pNext = &info,
    };
    s.export_objects(s.device, &objects);
    return static_cast<id>(info.mtlBuffer);
}

id ExportTexture(State& s, VkImage image) {
    VkExportMetalTextureInfoEXT info{
        .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT,
        .image = image,
        .plane = VK_IMAGE_ASPECT_PLANE_0_BIT,
    };
    VkExportMetalObjectsInfoEXT objects{
        .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT,
        .pNext = &info,
    };
    s.export_objects(s.device, &objects);
    return static_cast<id>(info.mtlTexture);
}

void Commit(State& s) {
    Send(s.set, "commit");
}

} // namespace

void Init(const Vulkan::Instance& instance) {
    auto& s = Get();
    std::scoped_lock lock{s.mutex};
    const char* env = std::getenv("BB_RESIDENCY_SET");
    if ((env && env[0] == '0') || !instance.IsMetalObjectsSupported() || s.set) {
        return;
    }
    s.device = instance.GetDevice();
    s.export_objects = reinterpret_cast<PFN_vkExportMetalObjectsEXT>(
        instance.GetDevice().getProcAddr("vkExportMetalObjectsEXT"));
    if (!s.export_objects) {
        return;
    }
    VkExportMetalDeviceInfoEXT device_info{
        .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT,
    };
    VkExportMetalCommandQueueInfoEXT queue_info{
        .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_COMMAND_QUEUE_INFO_EXT,
        .pNext = &device_info,
        .queue = instance.GetGraphicsQueue(),
    };
    VkExportMetalObjectsInfoEXT objects{
        .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT,
        .pNext = &queue_info,
    };
    s.export_objects(s.device, &objects);
    const id device = static_cast<id>(device_info.mtlDevice);
    const id queue = static_cast<id>(queue_info.mtlCommandQueue);
    const SEL new_set = sel_registerName("newResidencySetWithDescriptor:error:");
    if (!device || !queue || !class_respondsToSelector(object_getClass(device), new_set)) {
        std::printf("GPU: no Metal residency sets (macOS 15 or newer needed)\n");
        return;
    }
    const id descriptor =
        Send(reinterpret_cast<id>(objc_getClass("MTLResidencySetDescriptor")), "new");
    reinterpret_cast<void (*)(id, SEL, unsigned long)>(objc_msgSend)(
        descriptor, sel_registerName("setInitialCapacity:"), 4096);
    id error = nullptr;
    s.set = reinterpret_cast<id (*)(id, SEL, id, id*)>(objc_msgSend)(device, new_set, descriptor,
                                                                    &error);
    Send(descriptor, "release");
    if (!s.set) {
        std::printf("GPU: Metal residency set not created\n");
        return;
    }
    SendObject(queue, "addResidencySet:", s.set);
    Send(s.set, "requestResidency");
    std::printf("GPU: GPU memory kept resident (Metal residency set; BB_RESIDENCY_SET=0: off)\n");
}

bool Active() {
    return Get().set != nullptr;
}

bool AddMemory(VkDeviceMemory memory) {
    auto& s = Get();
    if (!s.set || !memory) {
        return false;
    }
    std::scoped_lock lock{s.mutex};
    if (auto it = s.memories.find(memory); it != s.memories.end()) {
        ++it->second.second;
        return true;
    }
    const id buffer = ExportBuffer(s, memory);
    if (!buffer) {
        return false;
    }
    s.memories.emplace(memory, std::pair{buffer, 1u});
    SendObject(s.set, "addAllocation:", buffer);
    Commit(s);
    return true;
}

void RemoveMemory(VkDeviceMemory memory) {
    auto& s = Get();
    if (!s.set || !memory) {
        return;
    }
    std::scoped_lock lock{s.mutex};
    const auto it = s.memories.find(memory);
    if (it == s.memories.end() || --it->second.second != 0) {
        return;
    }
    SendObject(s.set, "removeAllocation:", it->second.first);
    s.memories.erase(it);
    Commit(s);
}

void AddImage(VkImage image) {
    auto& s = Get();
    if (!s.set || !image) {
        return;
    }
    std::scoped_lock lock{s.mutex};
    if (s.images.contains(image)) {
        return;
    }
    const id texture = ExportTexture(s, image);
    if (!texture) {
        return;
    }
    s.images.emplace(image, texture);
    SendObject(s.set, "addAllocation:", texture);
    Commit(s);
}

void RemoveImage(VkImage image) {
    auto& s = Get();
    if (!s.set || !image) {
        return;
    }
    std::scoped_lock lock{s.mutex};
    const auto it = s.images.find(image);
    if (it == s.images.end()) {
        return;
    }
    SendObject(s.set, "removeAllocation:", it->second);
    s.images.erase(it);
    Commit(s);
}
#else
void Init(const Vulkan::Instance&) {}
bool Active() {
    return false;
}
bool AddMemory(VkDeviceMemory) {
    return false;
}
void RemoveMemory(VkDeviceMemory) {}
void AddImage(VkImage) {}
void RemoveImage(VkImage) {}
#endif

} // namespace BbMetalResidency
