// SPDX-License-Identifier: GPL-2.0-or-later
// bbport (macOS): GPU memory kept resident in a Metal residency set attached to the graphics
// queue. Without one the driver makes the memory a command buffer uses resident (wires it) for
// that command buffer and lets it go after: the game's ~6 GiB imported memory pool was wired ~3
// times a second, 70-80 ms each, and ~200k smaller wires and unwires (texture and buffer memory)
// took ~1.4 s of every 6 s, with the GPU idle meanwhile (Metal System Trace, 2026-10-07).
// BB_RESIDENCY_SET=0 turns it off.
#pragma once

#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {
class Instance;
}

namespace BbMetalResidency {

/// Once the device and its queues exist. No-op where unsupported (other platforms, macOS < 15).
void Init(const Vulkan::Instance& instance);
bool Active();

/// Device memory (its MTLBuffer), counted: resident from the first add to the last remove.
bool AddMemory(VkDeviceMemory memory);
void RemoveMemory(VkDeviceMemory memory);
/// An image's MTLTexture (MoltenVK gives each image its own), until removed before destruction.
void AddImage(VkImage image);
void RemoveImage(VkImage image);

} // namespace BbMetalResidency
