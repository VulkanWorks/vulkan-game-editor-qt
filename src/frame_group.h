#pragma once

#include "sprite_info.h"

enum FixedFrameGroup
{
    OutfitIdle = 0,
    OutfitMoving = 1,
    ObjectInitial = 2
};

struct FrameGroup
{
    FrameGroup(FixedFrameGroup fixedGroup, uint32_t id, SpriteInfo &&spriteInfo)
        : fixedGroup(fixedGroup), id(id), spriteInfo(std::move(spriteInfo)) {}

    FixedFrameGroup fixedGroup;
    uint32_t id;
    SpriteInfo spriteInfo;

    FrameGroup(const FrameGroup &) = delete;
    FrameGroup(FrameGroup &&) = default;
};