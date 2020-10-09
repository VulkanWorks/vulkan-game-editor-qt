#pragma once

#include <functional>
#include <optional>
#include <unordered_set>

#include "tile.h"

class MapView;

class Selection
{
public:
  Selection(MapView &mapView);
  bool blockDeselect = false;
  std::optional<Position> moveOrigin = {};

  // TODO
  // Position topLeft() const noexcept;
  // Position bottomRight() const noexcept;

  bool moving() const;

  bool contains(const Position pos) const;
  void select(const Position pos);
  void setSelected(const Position pos, bool selected);
  void deselect(const Position pos);
  void deselect(std::unordered_set<Position, PositionHash> &positions);
  void merge(std::unordered_set<Position, PositionHash> &positions);

  bool empty() const;

  const std::unordered_set<Position, PositionHash> &getPositions() const;

  void deselectAll();

  /*
    Clear the selected tile positions. NOTE: This function does not call deselect
    on the tiles. For that, use deselectAll().
  */
  void clear();

private:
  MapView &mapView;
  std::unordered_set<Position, PositionHash> positionsWithSelection;
};
