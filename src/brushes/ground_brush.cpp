
#include "ground_brush.h"

#include <numeric>

#include "../debug.h"
#include "../items.h"
#include "../map_view.h"
#include "../position.h"
#include "../random.h"
#include "../tile.h"

GroundBrush::GroundBrush(std::string id, const std::string &name, std::vector<WeightedItemId> &&weightedIds)
    : Brush(name), _weightedIds(std::move(weightedIds)), id(id), _iconServerId(_weightedIds.at(0).id)
{
    initialize();
}

GroundBrush::GroundBrush(std::string id, const std::string &name, std::vector<WeightedItemId> &&weightedIds, uint32_t iconServerId)
    : Brush(name), _weightedIds(std::move(weightedIds)), id(id), _iconServerId(iconServerId)
{
    initialize();
}

GroundBrush::GroundBrush(const std::string &id, std::vector<WeightedItemId> &&weightedIds)
    : Brush(id), _weightedIds(std::move(weightedIds)), id(id), _iconServerId(_weightedIds.at(0).id)
{
    initialize();
}

void GroundBrush::setIconServerId(uint32_t serverId)
{
    _iconServerId = serverId;
}

BrushResource GroundBrush::brushResource() const
{
    return _brushResource;
}

void GroundBrush::initialize()
{
    // Sort by weights descending to optimize iteration in sampleServerId() for the most common cases.
    std::sort(_weightedIds.begin(), _weightedIds.end(), [](const WeightedItemId &a, const WeightedItemId &b) { return a.weight > b.weight; });

    for (auto &entry : _weightedIds)
    {
        totalWeight += entry.weight;
        entry.weight = totalWeight;
        serverIds.emplace(entry.id);
    }

    _nextId = sampleServerId();

    _brushResource.id = _iconServerId;
    _brushResource.type = BrushResourceType::ItemType;
    _brushResource.variant = 0;
}

void GroundBrush::apply(MapView &mapView, const Position &position)
{
    mapView.addItem(position, Item(nextServerId()));
}

uint32_t GroundBrush::iconServerId() const
{
    return _iconServerId;
}

BrushType GroundBrush::type() const
{
    return BrushType::Ground;
}

bool GroundBrush::erasesItem(uint32_t serverId) const
{
    return serverIds.find(serverId) != serverIds.end();
}

uint32_t GroundBrush::nextServerId()
{
    uint32_t result = _nextId;

    _nextId = sampleServerId();

    return result;
}

uint32_t GroundBrush::sampleServerId()
{
    uint32_t weight = Random::global().nextInt<uint32_t>(static_cast<uint32_t>(0), totalWeight);

    for (const auto &entry : _weightedIds)
    {
        if (weight < entry.weight)
        {
            return entry.id;
        }
    }

    // If we get here, something is off with `weight` for some weighted ID or with `totalWeight`.
    VME_LOG_ERROR(
        "[GroundBrush::nextServerId] Brush " << _name
                                             << ": Could not find matching weight for randomly generated weight "
                                             << weight << " (totalWeight: " << totalWeight << ".");

    // No match in for-loop. Use the first ID.
    return _weightedIds.at(0).id;
}

std::string GroundBrush::brushId() const noexcept
{
    return id;
}

void GroundBrush::setName(std::string name)
{
    _name = name;
}

std::vector<ThingDrawInfo> GroundBrush::getPreviewTextureInfo() const
{
    return std::vector<ThingDrawInfo>{DrawItemType(_nextId, PositionConstants::Zero)};
}

const std::string GroundBrush::getDisplayId() const
{
    return id;
}
