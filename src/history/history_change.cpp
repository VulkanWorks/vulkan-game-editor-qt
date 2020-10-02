#include "history_change.h"

#include "../map_view.h"

namespace MapHistory
{
  /*
    Static methods
  */
  std::optional<Select> Select::fullTile(Tile &tile)
  {
    if (tile.isEmpty())
      return {};

    bool includesGround = tile.hasGround() && !tile.ground()->selected;

    std::vector<uint16_t> indices;
    for (int i = 0; i < tile.itemCount(); ++i)
    {
      if (!tile.itemSelected(i))
      {
        indices.emplace_back(i);
      }
    }

    return Select(tile.position(), indices, includesGround);
  }

  std::optional<MapHistory::Deselect> Deselect::fullTile(Tile &tile)
  {
    if (tile.isEmpty())
      return {};

    bool includesGround = tile.hasGround() && tile.ground()->selected;

    std::vector<uint16_t> indices;
    for (int i = 0; i < tile.itemCount(); ++i)
    {
      if (tile.itemSelected(i))
      {
        indices.emplace_back(i);
      }
    }

    return Deselect(tile.position(), indices, includesGround);
  }

  std::optional<Select> Select::topItem(Tile &tile)
  {
    if (tile.topItemSelected())
      return {};

    std::vector<uint16_t> indices;

    bool isTopGround = tile.getTopItem() == tile.ground();
    if (!isTopGround)
    {
      indices.emplace_back(static_cast<uint16_t>(tile.itemCount() - 1));
    }

    return Select(tile.position(), indices, isTopGround);
  }

  std::optional<Deselect> Deselect::topItem(Tile &tile)
  {
    if (!tile.topItemSelected())
      return {};

    std::vector<uint16_t> indices;
    bool isTopGround = tile.getTopItem() == tile.ground();
    if (!isTopGround)
    {
      indices.emplace_back(static_cast<uint16_t>(tile.itemCount() - 1));
    }
    return Deselect(tile.position(), indices, isTopGround);
  }

  SetTile::SetTile(Tile &&tile) : tile(std::move(tile)) {}

  void SetTile::commit(MapView &mapView)
  {
    DEBUG_ASSERT(!committed, "Attempted to commit an action that is already marked as committed.");

    std::unique_ptr<Tile> currentTilePtr = mapView.setTileInternal(std::move(tile));
    Tile *currentTile = currentTilePtr.release();

    tile = std::move(*currentTile);

    committed = true;
  }

  void SetTile::undo(MapView &mapView)
  {
    DEBUG_ASSERT(committed, "Attempted to undo an action that is not marked as committed.");

    tile.initEntities();
    std::unique_ptr<Tile> currentTilePointer = mapView.setTileInternal(std::move(tile));
    Tile *currentTile = currentTilePointer.release();

    tile = std::move(*currentTile);

    committed = false;
  }

  RemoveTile::RemoveTile(Position pos) : data(pos) {}

  void RemoveTile::commit(MapView &mapView)
  {
    Position &position = std::get<Position>(data);
    std::unique_ptr<Tile> currentTilePointer = mapView.removeTileInternal(position);
    Tile *currentTile = currentTilePointer.release();

    data = std::move(*currentTile);
  }

  void RemoveTile::undo(MapView &mapView)
  {
    Tile &tile = std::get<Tile>(data);
    Position pos = tile.position();

    tile.initEntities();
    mapView.setTileInternal(std::move(tile));

    data = pos;
  }

  Move::Move(Position from, Position to, bool ground, std::vector<uint16_t> &indices)
      : fromPosition(from), toPosition(to), moveData(Move::Partial(ground, std::move(indices))) {}

  Move::Move(Position from, Position to)
      : fromPosition(from), toPosition(to), moveData(Move::Entire{}) {}

  Move::Partial::Partial(bool ground, std::vector<uint16_t> indices)
      : ground(ground), indices(indices) {}

  Move::UndoData::UndoData(Tile &&fromTile, Tile &&toTile)
      : fromTile(std::move(fromTile)), toTile(std::move(toTile)) {}

  Move Move::entire(Position from, Position to)
  {
    return Move(from, to);
  }

  Move Move::entire(const Tile &tile, Position to)
  {
    return Move(tile.position(), to);
  }

  Move Move::selected(const Tile &tile, Position to)
  {
    std::vector<uint16_t> indices;
    auto &items = tile.items();
    for (int i = 0; i < tile.itemCount(); ++i)
    {
      if (items.at(i).selected)
        indices.emplace_back(i);
    }

    bool moveGround = tile.hasGround() && tile.ground()->selected;

    return Move(tile.position(), to, moveGround, indices);
  }

  void Move::commit(MapView &mapView)
  {
    Tile &from = mapView.getOrCreateTile(fromPosition);
    Tile &to = mapView.getOrCreateTile(toPosition);

    undoData = std::make_optional(Move::UndoData(from.deepCopy(), to.deepCopy()));

    std::visit(
        util::overloaded{
            [this, &mapView, &from, &to](const Entire) {
              if (from.hasGround())
              {
                mapView.map()->moveTile(fromPosition, toPosition);
              }
              else
              {
                for (size_t i = 0; i < from.itemCount(); ++i)
                  to.addItem(from.dropItem(i));
              }
            },

            [this, &mapView, &from, &to](const Partial &partial) {
              for (const auto i : partial.indices)
              {
                DEBUG_ASSERT(i < from.itemCount(), "Index out of bounds.");
                to.addItem(from.dropItem(i));
              }

              if (partial.ground && from.hasGround())
                to.setGround(from.dropGround());
            }},
        moveData);

    mapView.updateSelection(fromPosition);
    mapView.updateSelection(toPosition);
  }

  void Move::undo(MapView &mapView)
  {
    mapView.map()->insertTile(std::move(undoData.value().fromTile));
    mapView.map()->insertTile(std::move(undoData.value().toTile));
  }

  SelectMultiple::SelectMultiple(std::unordered_set<Position, PositionHash> positions, bool select)
      : positions(positions),
        select(select)
  {
  }

  void SelectMultiple::commit(MapView &mapView)
  {
    if (select)
      mapView.selection.merge(positions);
    else
      mapView.selection.deselect(positions);
  }

  void SelectMultiple::undo(MapView &mapView)
  {
    if (select)
      mapView.selection.deselect(positions);
    else
      mapView.selection.merge(positions);
  }

  Select::Select(Position position,
                 std::vector<uint16_t> indices,
                 bool includesGround)
      : position(position),
        indices(indices),
        includesGround(includesGround) {}

  void Select::commit(MapView &mapView)
  {
    Tile *tile = mapView.getTile(position);
    for (const auto i : indices)
      tile->setItemSelected(i, true);

    if (includesGround)
      tile->setGroundSelected(true);

    mapView.selection.setSelected(position, tile->hasSelection());
  }

  void Select::undo(MapView &mapView)
  {
    Tile *tile = mapView.getTile(position);
    for (const auto i : indices)
      tile->setItemSelected(i, false);

    if (includesGround)
      tile->setGroundSelected(false);

    mapView.selection.setSelected(position, tile->hasSelection());
  }

  Deselect::Deselect(Position position,
                     std::vector<uint16_t> indices,
                     bool includesGround)
      : position(position),
        indices(indices),
        includesGround(includesGround) {}

  void Deselect::commit(MapView &mapView)
  {
    Tile *tile = mapView.getTile(position);
    for (const auto i : indices)
      tile->setItemSelected(i, false);

    if (includesGround)
      tile->setGroundSelected(false);

    mapView.selection.setSelected(position, tile->hasSelection());
  }

  void Deselect::undo(MapView &mapView)
  {
    Tile *tile = mapView.getTile(position);
    for (const auto i : indices)
      tile->setItemSelected(i, true);

    if (includesGround)
      tile->setGroundSelected(true);

    mapView.selection.setSelected(position, tile->hasSelection());
  }

  void Action::markAsCommitted()
  {
    committed = true;
  }

  void Change::commit(MapView &mapView)
  {
    std::visit(
        util::overloaded{
            [&mapView](std::unique_ptr<ChangeItem> &change) {
              change->commit(mapView);
              change->committed = true;
            },
            [](std::monostate &s) {
              DEBUG_ASSERT(false, "[MapHistory::Change::commit] An empty action should never get here.");
            },
            [&mapView](auto &change) {
              change.commit(mapView);
              change.committed = true;
            }},
        data);
  }

  void Change::undo(MapView &mapView)
  {
    std::visit(
        util::overloaded{
            [&mapView](std::unique_ptr<ChangeItem> &change) {
              change->undo(mapView);
              change->committed = false;
            },
            [](std::monostate &s) {
              DEBUG_ASSERT(false, "[MapHistory::Change::undo] An empty action should never get here.");
            },
            [&mapView](auto &change) {
              change.undo(mapView);
              change.committed = false;
            }},
        data);
  }
} // namespace MapHistory