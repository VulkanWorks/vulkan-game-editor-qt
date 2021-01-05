#include "item_type.h"

#include "ecs/ecs.h"
#include "ecs/item_animation.h"

const uint32_t ItemType::getPatternIndex(const Position &pos) const
{
    const SpriteInfo &spriteInfo = appearance->getSpriteInfo();
    if (spriteInfo.patternSize == 1 || isStackable())
        return 0;

    uint32_t width = spriteInfo.patternWidth;
    uint32_t height = spriteInfo.patternHeight;
    uint32_t depth = spriteInfo.patternDepth;

    uint32_t spriteIndex = (pos.x % width) + (pos.y % height) * width + (pos.z % depth) * height * width;

    return spriteIndex;
}

const TextureInfo ItemType::getTextureInfo(TextureInfo::CoordinateType coordinateType) const
{
    uint32_t spriteId = appearance->getFirstSpriteId();
    return getTextureInfo(spriteId, coordinateType);
}

const TextureInfo ItemType::getTextureInfo(const Position &pos, TextureInfo::CoordinateType coordinateType) const
{
    const SpriteInfo &spriteInfo = appearance->getSpriteInfo();

    uint32_t spriteIndex = getPatternIndex(pos);
    uint32_t spriteId = spriteInfo.spriteIds.at(spriteIndex);

    return getTextureInfo(spriteId, coordinateType);
}

const TextureInfo ItemType::getTextureInfo(uint32_t spriteId, TextureInfo::CoordinateType coordinateType) const
{
    TextureInfo info;
    info.atlas = getTextureAtlas(spriteId);
    info.window = info.atlas->getTextureWindow(spriteId, coordinateType);

    return info;
}

std::vector<TextureAtlas *> ItemType::getTextureAtlases() const
{
    auto comparator = [](TextureAtlas *a, TextureAtlas *b) { return a->sourceFile.compare(b->sourceFile); };
    std::set<TextureAtlas *, decltype(comparator)> textureAtlases(comparator);

    auto &info = this->appearance->getSpriteInfo();

    for (const auto id : info.spriteIds)
        textureAtlases.insert(getTextureAtlas(id));

    return std::vector(textureAtlases.begin(), textureAtlases.end());
}

void ItemType::cacheTextureAtlases()
{
    for (int frameGroup = 0; frameGroup < appearance->frameGroupCount(); ++frameGroup)
    {
        for (const auto spriteId : appearance->getSpriteInfo(frameGroup).spriteIds)
        {
            // Stop if the cache is full
            if (_atlases.back() != nullptr)
            {
                return;
            }
            cacheTextureAtlas(spriteId);
        }
    }
}

void ItemType::cacheTextureAtlas(uint32_t spriteId)
{
    // If nothing is cached, cache the TextureAtlas for the first sprite ID in the appearance.
    if (_atlases.front() == nullptr)
        _atlases.front() = Appearances::getTextureAtlas(this->appearance->getFirstSpriteId());

    for (int i = 0; i < _atlases.size(); ++i)
    {
        TextureAtlas *&atlas = _atlases[i];
        // End of current cache reached, caching the atlas
        if (atlas == nullptr)
        {
            atlas = Appearances::getTextureAtlas(spriteId);
            return;
        }
        else
        {
            if (atlas->firstSpriteId <= spriteId && spriteId <= atlas->lastSpriteId)
            {
                // The TextureAtlas is already cached
                return;
            }
        }
    }
}

TextureAtlas *ItemType::getTextureAtlas(uint32_t spriteId) const
{
    for (const auto atlas : _atlases)
    {
        if (atlas == nullptr)
        {
            return nullptr;
        }

        if (atlas->firstSpriteId <= spriteId && spriteId <= atlas->lastSpriteId)
        {
            return atlas;
        }
    }

    return Appearances::getTextureAtlas(spriteId);
}

std::vector<const TextureAtlas *> ItemType::atlases() const
{
    std::vector<const TextureAtlas *> result;
    for (const auto atlas : _atlases)
    {
        if (atlas == nullptr)
            return result;
        result.emplace_back(atlas);
    }

    return result;
}

// std::vector<TextureAtlas> atlases()

std::string ItemType::getPluralName() const
{
    if (!pluralName.empty())
    {
        return pluralName;
    }

    if (showCount == 0)
    {
        return name;
    }

    std::string str;
    str.reserve(name.length() + 1);
    str.assign(name);
    str += 's';
    return str;
}

bool ItemType::isGroundTile() const noexcept
{
    return group == ItemType::Group::Ground;
}
bool ItemType::isContainer() const noexcept
{
    return group == ItemType::Group::Container;
}
bool ItemType::isSplash() const noexcept
{
    return group == ItemType::Group::Splash;
}
bool ItemType::isFluidContainer() const noexcept
{
    return group == ItemType::Group::Fluid;
}

bool ItemType::isCorpse() const noexcept
{
    return appearance->hasFlag(AppearanceFlag::Corpse);
}

bool ItemType::isDoor() const noexcept
{
    return (type == ItemTypes_t::Door);
}
bool ItemType::isMagicField() const noexcept
{
    return (type == ItemTypes_t::MagicField);
}
bool ItemType::isTeleport() const noexcept
{
    return (type == ItemTypes_t::Teleport);
}
bool ItemType::isKey() const noexcept
{
    return (type == ItemTypes_t::Key);
}
bool ItemType::isDepot() const noexcept
{
    return (type == ItemTypes_t::Depot);
}
bool ItemType::isMailbox() const noexcept
{
    return (type == ItemTypes_t::Mailbox);
}
bool ItemType::isTrashHolder() const noexcept
{
    return (type == ItemTypes_t::TrashHolder);
}
bool ItemType::isBed() const noexcept
{
    return (type == ItemTypes_t::Bed);
}

bool ItemType::isRune() const noexcept
{
    return (type == ItemTypes_t::Rune);
}
bool ItemType::isPickupable() const noexcept
{
    return (allowPickupable || pickupable);
}
bool ItemType::isUseable() const noexcept
{
    return (useable);
}
bool ItemType::hasSubType() const noexcept
{
    return (isFluidContainer() || isSplash() || stackable || charges != 0);
}

bool ItemType::usesSubType() const noexcept
{
    return isStackable() || isSplash() || isFluidContainer();
}

bool ItemType::isStackable() const noexcept
{
    return stackable;
}