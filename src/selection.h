#pragma once

#include <functional>
#include <optional>
#include <unordered_set>

#include "tile.h"
#include "util.h"
#include "octree.h"

class MapView;

class SelectionStorage
{
public:
  virtual void add(Position pos) = 0;
  virtual void add(std::vector<Position> positions, util::Rectangle<Position::value_type> bbox) = 0;

  virtual void remove(Position pos) = 0;

  virtual void update() = 0;

  virtual bool empty() const noexcept = 0;

  virtual bool contains(const Position pos) const = 0;

  virtual void clear() = 0;
  virtual size_t size() const noexcept = 0;

  virtual std::optional<Position> getCorner(bool positiveX, bool positiveY, bool positiveZ) const noexcept = 0;
  virtual std::optional<Position> getCorner(int positiveX, int positiveY, int positiveZ) const noexcept = 0;

  virtual const std::vector<Position> allPositions() const = 0;
};

class SelectionStorageOctree : public SelectionStorage
{
public:
  SelectionStorageOctree(const util::Volume<uint16_t, uint16_t, uint8_t> mapSize);

  void add(Position pos) override;
  void add(std::vector<Position> positions, util::Rectangle<Position::value_type> bbox) override;

  void remove(Position pos) override;

  void update() override;

  bool empty() const noexcept override;

  bool contains(const Position pos) const override;

  void clear() override;

  std::optional<Position> getCorner(bool positiveX, bool positiveY, bool positiveZ) const noexcept override;
  std::optional<Position> getCorner(int positiveX, int positiveY, int positiveZ) const noexcept override;

  vme::octree::Tree::iterator begin()
  {
    return tree.begin();
  }
  vme::octree::Tree::iterator end()
  {
    return tree.end();
  }

  size_t size() const noexcept override;

  const std::vector<Position> allPositions() const override;

private:
  vme::octree::Tree tree;
};

inline std::optional<Position> SelectionStorageOctree::getCorner(bool positiveX, bool positiveY, bool positiveZ) const noexcept
{
  return tree.getCorner(positiveX, positiveY, positiveZ);
}
inline std::optional<Position> SelectionStorageOctree::getCorner(int positiveX, int positiveY, int positiveZ) const noexcept
{
  return tree.getCorner(positiveX == 1, positiveY == 1, positiveZ == 1);
}

class SelectionStorageSet : public SelectionStorage
{
public:
  SelectionStorageSet();

  void add(Position pos) override;
  void add(std::vector<Position> positions, util::Rectangle<Position::value_type> bbox) override;

  void remove(Position pos) override;

  void update() override;

  bool empty() const noexcept override
  {
    return values.empty();
  }

  bool contains(const Position pos) const override
  {
    return values.find(pos) != values.end();
  }

  void clear() override;

private:
  std::unordered_set<Position, PositionHash> values;

  WorldPosition::value_type xMin, yMin, xMax, yMax;
  int zMin, zMax;

  bool staleBoundingBox = false;

  void setBoundingBox(Position pos);
  void updateBoundingBox(Position pos);
  void updateBoundingBox(util::Rectangle<Position::value_type> bbox);

  void recomputeBoundingBox();
};

class Selection
{
public:
  Selection(MapView &mapView);
  bool blockDeselect = false;
  std::optional<Position> moveOrigin = {};
  /*
    When the mouse goes outside of the map dimensions, this correction is used to
    stop the selection from also going out of bounds.
  */
  Position outOfBoundCorrection;

  // TODO

  bool moving() const;

  Position moveDelta() const;

  vme::octree::Tree::iterator begin()
  {
    return storage.begin();
  }
  vme::octree::Tree::iterator end()
  {
    return storage.end();
  }

  bool contains(const Position pos) const;
  void select(const Position pos);
  void setSelected(const Position pos, bool selected);
  void deselect(const Position pos);
  void deselect(std::vector<Position> &positions);
  void merge(std::vector<Position> &positions);

  size_t size() const noexcept;

  bool empty() const;

  void deselectAll();

  void update();

  /*
    Clear the selected tile positions. NOTE: This function does not call deselect
    on the tiles. For that, use deselectAll().
  */
  void clear();

private:
  MapView &mapView;

  SelectionStorageOctree storage;
};
