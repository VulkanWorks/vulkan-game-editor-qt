#include "brush_loader.h"

#include <format>
#include <fstream>
#include <sstream>
#include <stack>

#include "../debug.h"
#include "../item_palette.h"
#include "../logger.h"
#include "../time_point.h"
#include "border_brush.h"
#include "brush.h"

using json = nlohmann::json;

std::string joinStack(std::stack<std::string> &stack, std::string delimiter)
{
    auto stackCopy = stack;
    std::ostringstream ss;
    while (!stackCopy.empty())
    {
        ss << stackCopy.top();
        stackCopy.pop();
        if (!stackCopy.empty())
            ss << delimiter;
    }

    return ss.str();
}

json asArray(const json &j, std::string key)
{
    if (!j.contains(key))
        return json{};

    json value = j.at(key);
    return value.is_array() ? value : json{};
}

std::string getString(const json &j, std::string key)
{
    return j[key].get<std::string>();
}

int getInt(const json &j, std::string key)
{
    auto value = j[key];
    if (!value.is_number_integer())
    {
        throw json::type_error::create(302, std::format("The value at key '{}' has to be an integer (it was a '{}').", key, std::string(value.type_name())));
    }

    return value.get<int>();
}

bool BrushLoader::load(std::filesystem::path path)
{
    TimePoint start;

    if (!std::filesystem::exists(path))
    {
        VME_LOG_ERROR(std::format("Could not find file '{}'", path.string()));
        return false;
    }

    std::ifstream fileStream(path);
    json rootJson = json::parse(fileStream, nullptr, true, true);
    fileStream.close();

    auto topTrace = stackTrace;

    try
    {
        auto palettes = asArray(rootJson, "palettes");
        if (!palettes.is_null())
        {
            parsePalettes(palettes);
        }

        auto brushes = asArray(rootJson, "brushes");
        if (!brushes.is_null())
        {
            parseBrushes(brushes);
        }

        auto tilesets = asArray(rootJson, "tilesets");
        if (!tilesets.is_null())
        {
            parseTilesets(tilesets);
        }

        auto creatures = asArray(rootJson, "creatures");
        if (!creatures.is_null())
        {
            parseCreatures(creatures);
        }
    }
    catch (json::exception &exception)
    {
        std::string msg = joinStack(stackTrace, " -> ");
        VME_LOG_D(msg << ": " << exception.what());
        return false;
    }

    VME_LOG("Loaded brushes in " << start.elapsedMillis() << " ms.");

    return true;
}

void BrushLoader::parseBrushes(const nlohmann::json &brushesJson)
{
    stackTrace.emplace("/brushes");
    auto topTrace = stackTrace;

    for (const json &brush : brushesJson)
    {
        stackTrace = topTrace;

        if (!brush.contains("id"))
        {
            throw json::other_error::create(403, std::format("A brush is missing an id (all brushes must have an id). Add an id to this brush: {}", brush.dump(4)));
        }

        stackTrace.emplace(std::format("Brush '{}'", brush.at("id").get<std::string>()));

        auto brushType = parseBrushType(getString(brush, "type"));
        if (!brushType)
        {
            throw json::type_error::create(302, std::format("The type must be one of ['ground', 'doodad', 'wall', 'border']."));
        }

        switch (*brushType)
        {
            case BrushType::Ground:
            {
                auto groundBrush = parseGroundBrush(brush);
                Brush::addGroundBrush(std::move(groundBrush));
                break;
            }
            case BrushType::Border:
            {
                auto borderBrush = parseBorderBrush(brush);
                Brush::addBorderBrush(std::move(borderBrush));
            }

            default:
                break;
        }
    }
    stackTrace.pop();
}

GroundBrush BrushLoader::parseGroundBrush(const json &brush)
{
    json id = brush.at("id").get<std::string>();
    json name = brush.at("name").get<std::string>();

    int lookId = getInt(brush, "lookId");
    int zOrder = getInt(brush, "zOrder");

    json items = brush.at("items");

    std::vector<WeightedItemId> weightedIds;

    for (auto &item : items)
    {
        int id = getInt(item, "id");
        int chance = getInt(item, "chance");

        weightedIds.emplace_back(id, chance);
    }

    auto groundBrush = GroundBrush(id, std::move(weightedIds));
    groundBrush.setIconServerId(lookId);
    groundBrush.setName(name);
    return groundBrush;
}

BorderBrush BrushLoader::parseBorderBrush(const nlohmann::json &brush)
{
    json id = brush.at("id").get<std::string>();
    json name = brush.at("name").get<std::string>();

    auto lookId = getInt(brush, "lookId");

    const json &items = brush.at("items");

    const json &straight = items.at("straight");
    const json &corner = items.at("corner");
    const json &diagonal = items.at("diagonal");

    std::array<uint32_t, 12> borderIds;

    auto setBorderId = [&borderIds](BorderType borderType, uint32_t serverId) {
        // -1 because first value in BorderType is BorderType::None
        int index = to_underlying(borderType) - 1;
        borderIds[index] = serverId;
    };

    setBorderId(BorderType::North, getInt(straight, "n"));
    setBorderId(BorderType::East, getInt(straight, "e"));
    setBorderId(BorderType::South, getInt(straight, "s"));
    setBorderId(BorderType::West, getInt(straight, "w"));

    setBorderId(BorderType::NorthWestCorner, getInt(corner, "nw"));
    setBorderId(BorderType::NorthEastCorner, getInt(corner, "ne"));
    setBorderId(BorderType::SouthEastCorner, getInt(corner, "se"));
    setBorderId(BorderType::SouthWestCorner, getInt(corner, "sw"));

    setBorderId(BorderType::NorthWestDiagonal, getInt(diagonal, "nw"));
    setBorderId(BorderType::NorthEastDiagonal, getInt(diagonal, "ne"));
    setBorderId(BorderType::SouthEastDiagonal, getInt(diagonal, "se"));
    setBorderId(BorderType::SouthWestDiagonal, getInt(diagonal, "sw"));

    auto borderBrush = BorderBrush(id, name, borderIds);
    borderBrush.setIconServerId(lookId);

    return borderBrush;
}

void BrushLoader::parseTilesets(const nlohmann::json &tilesetsJson)
{
    stackTrace.emplace("/tilesets");
    auto topTrace = stackTrace;

    for (const json &tileset : tilesetsJson)
    {
        stackTrace = topTrace;

        if (!tileset.contains("id"))
        {
            throw json::other_error::create(403, std::format("A tileset is missing an id (all tilesets must have an id). Add an id to this tileset: {}", tileset.dump(4)));
        }

        parseTileset(tileset);
    }
    stackTrace.pop();
}

void BrushLoader::parseTileset(const nlohmann::json &tilesetJson)
{
    auto tilesetId = tilesetJson.at("id").get<std::string>();
    auto tilesetName = tilesetJson.at("name").get<std::string>();

    stackTrace.emplace(std::format("Tileset '{}'", tilesetId));

    auto palettes = tilesetJson.at("palettes");
    for (const auto &paletteJson : palettes)
    {
        auto paletteId = paletteJson.at("id").get<std::string>();
        stackTrace.emplace(std::format("Palette '{}'", paletteId));

        auto palette = ItemPalettes::getById(paletteId);
        if (!palette)
        {
            VME_LOG_ERROR(std::format("There is no palette with id '{}'.", tilesetId));
            stackTrace.pop();
            continue;
        }

        auto tileset = std::make_unique<Tileset>(tilesetId);
        tileset->setName(tilesetName);

        auto brushes = paletteJson.at("brushes");

        for (auto &brush : brushes)
        {
            std::string brushType = brush.at("type").get<std::string>();
            if (brushType == "raw")
            {
                for (const auto &idObject : brush.at("serverIds"))
                {
                    if (idObject.is_number_integer())
                    {
                        tileset->addRawBrush(idObject.get<int>());
                    }
                    else if (idObject.is_array() && idObject.size() == 2)
                    {
                        uint32_t from = idObject[0].get<int>();
                        uint32_t to = idObject[1].get<int>();

                        for (uint32_t id = from; id <= to; ++id)
                        {
                            tileset->addRawBrush(id);
                        }
                    }
                    else
                    {
                        throw json::type_error::create(302, std::format("Invalid value in serverIds: {}. The values in the serverIds array must be server IDs or arrays of size two as [from_server_id, to_server_id]. For example: 'serverIds: [100, [103, 105]]' will yield ids [100, 103, 104, 105].", idObject.dump(4)));
                    }
                }
            }
        }

        palette->addTileset(std::move(tileset));

        stackTrace.pop();
    }

    stackTrace.pop();
}

void BrushLoader::parseCreatures(const nlohmann::json &creaturesJson)
{
    stackTrace.emplace("/creatures");
    auto topTrace = stackTrace;

    for (const json &creature : creaturesJson)
    {
        stackTrace = topTrace;

        if (!creature.contains("id"))
        {
            throw json::other_error::create(403, std::format("A creature is missing an id (all creatures must have an id). Add an id to this creature: {}", creature.dump(4)));
        }

        parseCreature(creature);
    }
    stackTrace.pop();
}

void BrushLoader::parseCreature(const nlohmann::json &creatureJson)
{
    auto id = getString(creatureJson, "id");
    stackTrace.emplace(std::format("Creature '{}'", id));

    if (!creatureJson.contains("name") || !creatureJson.at("name").is_string())
    {
        throw json::other_error::create(403, std::format("A creature is missing a name (all creatures must have a name). Add a name to this creature: {}", creatureJson.dump(4)));
    }

    if (!creatureJson.contains("type") || !creatureJson.at("type").is_string())
    {
        throw json::other_error::create(403, std::format("A creature is missing a type (either 'monster' or 'npc'). Add a type to this creature: {}", creatureJson.dump(4)));
    }

    if (!creatureJson.contains("looktype") || !creatureJson.at("looktype").is_number_integer())
    {
        throw json::other_error::create(403, std::format("A creature is missing a looktype. Add a looktype to this creature: {}", creatureJson.dump(4)));
    }

    auto name = getString(creatureJson, "name");
    auto type = getString(creatureJson, "type");
    int looktype = getInt(creatureJson, "looktype");
}

void BrushLoader::parsePalettes(const nlohmann::json &paletteJson)
{
    stackTrace.emplace("/palettes");

    for (const json &palette : paletteJson)
    {
        if (!palette.contains("id"))
        {
            throw json::other_error::create(403, std::format("A palette is missing an id (all palettes must have an id). Add an id to this palette: {}", palette.dump(4)));
        }

        if (!palette.contains("name"))
        {
            throw json::other_error::create(403, std::format("A palette is missing a name (all palettes must have a name). Add a name to this palette: {}", palette.dump(4)));
        }

        auto id = palette.at("id").get<std::string>();
        auto name = palette.at("name").get<std::string>();

        ItemPalettes::createPalette(id, name);
    }
}
