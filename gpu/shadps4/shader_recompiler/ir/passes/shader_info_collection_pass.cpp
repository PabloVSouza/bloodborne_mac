// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <optional>
#include "core/emulator_settings.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/profile.h"
#include "video_core/buffer_cache/buffer_cache.h"

namespace Shader::Optimization {

void Visit(Info& info, const IR::Inst& inst) {
    switch (inst.GetOpcode()) {
    case IR::Opcode::GetAttribute:
    case IR::Opcode::GetAttributeU1:
    case IR::Opcode::GetAttributeU32:
        info.loads.Set(inst.Arg(0).Attribute(), inst.Arg(1).U32());
        break;
    case IR::Opcode::SetAttribute:
        info.stores.Set(inst.Arg(0).Attribute(), inst.Arg(2).U32());
        break;
    case IR::Opcode::GetUserData:
        info.ud_mask.Set(inst.Arg(0).ScalarReg());
        break;
    case IR::Opcode::SetPatch: {
        const auto patch = inst.Arg(0).Patch();
        if (patch <= IR::Patch::TessellationLodBottom) {
            info.stores_tess_level_outer = true;
        } else if (patch <= IR::Patch::TessellationLodInteriorV) {
            info.stores_tess_level_inner = true;
        } else {
            info.uses_patches |= 1U << IR::GenericPatchIndex(patch);
        }
        break;
    }
    case IR::Opcode::GetPatch: {
        const auto patch = inst.Arg(0).Patch();
        info.uses_patches |= 1U << IR::GenericPatchIndex(patch);
        break;
    }
    case IR::Opcode::LoadSharedU16:
    case IR::Opcode::WriteSharedU16:
        info.shared_types |= IR::Type::U16;
        break;
    case IR::Opcode::LoadSharedU32:
    case IR::Opcode::WriteSharedU32:
    case IR::Opcode::SharedAtomicIAdd32:
    case IR::Opcode::SharedAtomicISub32:
    case IR::Opcode::SharedAtomicSMin32:
    case IR::Opcode::SharedAtomicUMin32:
    case IR::Opcode::SharedAtomicSMax32:
    case IR::Opcode::SharedAtomicUMax32:
    case IR::Opcode::SharedAtomicInc32:
    case IR::Opcode::SharedAtomicDec32:
    case IR::Opcode::SharedAtomicAnd32:
    case IR::Opcode::SharedAtomicOr32:
    case IR::Opcode::SharedAtomicXor32:
    case IR::Opcode::SharedAtomicCmpSwap32:
        info.shared_types |= IR::Type::U32;
        break;
    case IR::Opcode::SharedAtomicIAdd64:
    case IR::Opcode::SharedAtomicISub64:
    case IR::Opcode::SharedAtomicSMin64:
    case IR::Opcode::SharedAtomicUMin64:
    case IR::Opcode::SharedAtomicSMax64:
    case IR::Opcode::SharedAtomicUMax64:
    case IR::Opcode::SharedAtomicInc64:
    case IR::Opcode::SharedAtomicDec64:
    case IR::Opcode::SharedAtomicAnd64:
    case IR::Opcode::SharedAtomicOr64:
    case IR::Opcode::SharedAtomicXor64:
    case IR::Opcode::SharedAtomicCmpSwap64:
        info.uses_shared_int64_atomics = true;
        [[fallthrough]];
    case IR::Opcode::LoadSharedU64:
    case IR::Opcode::WriteSharedU64:
        info.shared_types |= IR::Type::U64;
        break;
    case IR::Opcode::ConvertF16F32:
    case IR::Opcode::ConvertF32F16:
    case IR::Opcode::BitCastU16F16:
    case IR::Opcode::BitCastF16U16:
        info.uses_fp16 = true;
        break;
    case IR::Opcode::PackDouble2x32:
    case IR::Opcode::UnpackDouble2x32:
        info.uses_fp64 = true;
        break;
    case IR::Opcode::ImageWrite:
        info.has_storage_images = true;
        break;
    case IR::Opcode::QuadBroadcast:
        info.uses_group_quad = true;
        break;
    case IR::Opcode::Shuffle:
    case IR::Opcode::ShuffleXor:
        info.uses_group_shuffle = true;
        break;
    case IR::Opcode::ReadLane:
    case IR::Opcode::ReadFirstLane:
    case IR::Opcode::WriteLane:
        info.uses_group_ballot = true;
        break;
    case IR::Opcode::Discard:
    case IR::Opcode::DiscardCond:
        info.has_discard = true;
        break;
    case IR::Opcode::BitwiseXor32:
        info.has_bitwise_xor = true;
        break;
    case IR::Opcode::ImageGather:
    case IR::Opcode::ImageGatherDref:
        info.has_image_gather = true;
        break;
    case IR::Opcode::ImageQueryDimensions:
    case IR::Opcode::ImageQueryLod:
        info.has_image_query = true;
        break;
    case IR::Opcode::ImageAtomicFMax32:
    case IR::Opcode::ImageAtomicFMin32:
        info.uses_image_atomic_float_min_max = true;
        break;
    case IR::Opcode::BufferAtomicFMax32:
    case IR::Opcode::BufferAtomicFMin32:
        info.uses_buffer_atomic_float_min_max = true;
        break;
    case IR::Opcode::BufferAtomicIAdd64:
    case IR::Opcode::BufferAtomicSMax64:
    case IR::Opcode::BufferAtomicSMin64:
    case IR::Opcode::BufferAtomicUMax64:
    case IR::Opcode::BufferAtomicUMin64:
        info.uses_buffer_int64_atomics = true;
        break;
    case IR::Opcode::DataAppend:
    case IR::Opcode::DataConsume:
    case IR::Opcode::Ballot:
    case IR::Opcode::InverseBallot:
    case IR::Opcode::BallotFindLsb:
        info.uses_group_ballot = true;
        [[fallthrough]];
    case IR::Opcode::LaneId:
        info.uses_lane_id = true;
        break;
    case IR::Opcode::Memtime:
        info.uses_shader_clock = true;
        break;
    case IR::Opcode::ReadConst:
        if (!info.has_readconst) {
            info.buffers.push_back({
                .used_types = IR::Type::U32,
                .buffer_type = BufferType::Flatbuf,
            });
            info.has_readconst = true;
        }
        if (inst.Flags<u32>() == 0) {
            info.readconst_types |= Info::ReadConstType::Immediate;
            info.readconst_types |= Info::ReadConstType::Dynamic;
            info.uses_dma = true;
        }
        break;
    case IR::Opcode::PackUfloat10_11_11:
        info.uses_pack_10_11_11 = true;
        break;
    case IR::Opcode::UnpackUfloat10_11_11:
        info.uses_unpack_10_11_11 = true;
        break;
    default:
        break;
    }
}

/// bbport: a vertex shader whose every output is one of its vertex attributes' components or a
/// constant (a passthrough). A rect list drawn with it can be drawn as two triangles per rect, the
/// 4th corner's attributes computed from the other three's (as the rect-list TCS computes its
/// outputs): info.rect_position_attr is the attribute position x and y come from.
static void FindRectPassthrough(const IR::Program& program, Info& info) {
    info.rect_position_attr = -1;
    if (info.sw_stage != SwStage::Vertex || !info.has_fetch_shader) {
        return;
    }
    struct Source {
        bool constant;
        IR::Attribute attribute;
        u32 comp;
    };
    const auto resolve = [](IR::Value value) -> std::optional<Source> {
        for (int depth = 0; depth < 16; ++depth) {
            if (value.IsImmediate()) {
                return Source{true, IR::Attribute::Param0, 0};
            }
            const IR::Inst* inst = value.Inst();
            switch (inst->GetOpcode()) {
            case IR::Opcode::GetAttribute:
                if (!IR::IsParam(inst->Arg(0).Attribute()) || !inst->Arg(2).IsImmediate() ||
                    inst->Arg(2).U32() != 0) {
                    return std::nullopt;
                }
                return Source{false, inst->Arg(0).Attribute(), inst->Arg(1).U32()};
            case IR::Opcode::CompositeExtractF32x2:
            case IR::Opcode::CompositeExtractF32x3:
            case IR::Opcode::CompositeExtractF32x4: {
                if (!inst->Arg(1).IsImmediate()) {
                    return std::nullopt;
                }
                // The component of a vector: through shuffles to the construct of it.
                u32 index = inst->Arg(1).U32();
                IR::Value composite = inst->Arg(0);
                for (int step = 0;; ++step) {
                    if (composite.IsImmediate() || step == 16) {
                        return std::nullopt;
                    }
                    const IR::Inst* vector = composite.Inst();
                    const auto op = vector->GetOpcode();
                    if (op == IR::Opcode::CompositeShuffleF32x2 ||
                        op == IR::Opcode::CompositeShuffleF32x3 ||
                        op == IR::Opcode::CompositeShuffleF32x4) {
                        const u32 size = op == IR::Opcode::CompositeShuffleF32x2   ? 2
                                         : op == IR::Opcode::CompositeShuffleF32x3 ? 3
                                                                                   : 4;
                        if (index >= size || !vector->Arg(2 + index).IsImmediate()) {
                            return std::nullopt;
                        }
                        const u32 pick = vector->Arg(2 + index).U32();
                        composite = vector->Arg(pick < size ? 0 : 1);
                        index = pick < size ? pick : pick - size;
                        continue;
                    }
                    if (op == IR::Opcode::CompositeConstructF32x2 ||
                        op == IR::Opcode::CompositeConstructF32x3 ||
                        op == IR::Opcode::CompositeConstructF32x4) {
                        if (index >= vector->NumArgs()) {
                            return std::nullopt;
                        }
                        value = vector->Arg(index);
                        break;
                    }
                    return std::nullopt;
                }
                continue;
            }
            default:
                return std::nullopt;
            }
        }
        return std::nullopt;
    };
    std::array<std::optional<Source>, 2> position{};
    for (const IR::Block* const block : program.blocks) {
        for (const IR::Inst& inst : block->Instructions()) {
            switch (inst.GetOpcode()) {
            case IR::Opcode::Prologue:
            case IR::Opcode::Epilogue:
            case IR::Opcode::Void:
            case IR::Opcode::GetAttribute:
            case IR::Opcode::CompositeConstructF32x2:
            case IR::Opcode::CompositeConstructF32x3:
            case IR::Opcode::CompositeConstructF32x4:
            case IR::Opcode::CompositeExtractF32x2:
            case IR::Opcode::CompositeExtractF32x3:
            case IR::Opcode::CompositeExtractF32x4:
            case IR::Opcode::CompositeShuffleF32x2:
            case IR::Opcode::CompositeShuffleF32x3:
            case IR::Opcode::CompositeShuffleF32x4:
            case IR::Opcode::UndefU32:
            case IR::Opcode::UndefF32:
                break; // their uses by outputs are checked there
            case IR::Opcode::SetAttribute: {
                const IR::Attribute attribute = inst.Arg(0).Attribute();
                if (attribute != IR::Attribute::Position0 && !IR::IsParam(attribute)) {
                    return;
                }
                const auto source = resolve(inst.Arg(1));
                if (!source) {
                    return;
                }
                const u32 comp = inst.Arg(2).U32();
                if (attribute == IR::Attribute::Position0 && comp < 2) {
                    position[comp] = source;
                }
                break;
            }
            default:
                if (std::getenv("BB_RECT_DEBUG")) {
                    std::printf("Rect passthrough: vs %016llx no (%s)\n",
                                (unsigned long long)info.pgm_hash,
                                IR::NameOf(inst.GetOpcode()).data());
                }
                return;
            }
        }
    }
    if (std::getenv("BB_RECT_DEBUG")) {
        std::printf("Rect passthrough: vs %016llx position %d/%d\n",
                    (unsigned long long)info.pgm_hash, position[0] ? 1 : 0, position[1] ? 1 : 0);
    }
    if (!position[0] || !position[1] || position[0]->constant || position[1]->constant ||
        position[0]->attribute != position[1]->attribute) {
        return;
    }
    info.rect_position_attr =
        s8(u32(position[0]->attribute) - u32(IR::Attribute::Param0));
    info.rect_position_comp = {u8(position[0]->comp), u8(position[1]->comp)};
}

void CollectShaderInfoPass(IR::Program& program, const Profile& profile) {
    Info& info = program.info;
    for (IR::Block* const block : program.post_order_blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            Visit(info, inst);
        }
    }
    FindRectPassthrough(program, info);

    if (!EmulatorSettings.IsDirectMemoryAccessEnabled()) {
        info.uses_dma = false;
        info.readconst_types = Info::ReadConstType::None;
    }

    if (info.uses_dma) {
        info.buffers.push_back({
            .used_types = IR::Type::U64,
            .buffer_type = BufferType::BdaPagetable,
            .is_written = true,
        });
        info.buffers.push_back({
            .used_types = IR::Type::U32,
            .buffer_type = BufferType::FaultBuffer,
            .is_written = true,
        });
        LOG_ERROR(Render, "Enabling DMA for shader {:#x}", info.pgm_hash);
    }
}

} // namespace Shader::Optimization
