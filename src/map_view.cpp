#include "map_view.h"

#include "const.h"

Viewport::Viewport()
    : width(0),
      height(0),
      zoom(0.25f),
      offset(0L, 0L) {}

MapView::MapView(MapViewMouseAction &mapViewMouseAction)
    : MapView(mapViewMouseAction, std::make_shared<Map>()) {}

MapView::MapView(MapViewMouseAction &mapViewMouseAction, std::shared_ptr<Map> map)
    : mapViewMouseAction(mapViewMouseAction),
      selection(*this),
      map(map),
      dragState{},
      viewport(),
      _mousePos(),
      history(*this) {}

void MapView::selectTopItem(const Position pos)
{
  Tile *tile = getTile(pos);
  DEBUG_ASSERT(tile != nullptr, "nullptr tile");
  selectTopItem(*tile);
}

void MapView::selectTopItem(Tile &tile)
{
  auto selection = MapHistory::Select::topItem(tile);
  if (!selection.has_value())
    return;

  auto action = newAction(MapHistory::ActionType::Selection);
  action.addChange(std::move(selection.value()));

  history.commit(std::move(action));
}

void MapView::deselectTopItem(Tile &tile)
{
  auto deselection = MapHistory::Deselect::topItem(tile);
  if (!deselection.has_value())
    return;

  auto action = newAction(MapHistory::ActionType::Selection);
  action.addChange(std::move(deselection.value()));

  history.commit(std::move(action));
}

void MapView::selectAll(Tile &tile)
{
  tile.selectAll();
  selection.select(tile.position());
}

void MapView::clearSelection()
{
  selection.deselectAll();
}

bool MapView::hasSelectionMoveOrigin() const
{
  return selection.moveOrigin.has_value();
}

void MapView::addItem(const Position pos, uint16_t id)
{
  if (!Items::items.validItemType(id))
    return;

  Tile &currentTile = map->getOrCreateTile(pos);
  Tile newTile = currentTile.deepCopy();

  newTile.addItem(Item(id));

  MapHistory::Action action(MapHistory::ActionType::SetTile);
  action.addChange(MapHistory::SetTile(std::move(newTile)));

  history.commit(std::move(action));
}

void MapView::removeItems(const Position position, const std::set<size_t, std::greater<size_t>> &indices)
{
  TileLocation *location = map->getTileLocation(position);

  DEBUG_ASSERT(location->hasTile(), "The location has no tile.");

  MapHistory::Action action(MapHistory::ActionType::RemoveTile);

  Tile newTile = deepCopyTile(position);

  for (const auto index : indices)
  {
    newTile.removeItem(index);
  }

  action.addChange(MapHistory::SetTile(std::move(newTile)));

  history.commit(std::move(action));
}

void MapView::removeSelectedItems(const Tile &tile)
{
  MapHistory::Action action(MapHistory::ActionType::ModifyTile);

  Tile newTile = tile.deepCopy();

  for (size_t i = 0; i < tile.items.size(); ++i)
  {
    if (tile.items.at(i).selected)
      newTile.removeItem(i);
  }

  Item *ground = newTile.getGround();
  if (ground && ground->selected)
  {
    newTile.ground.reset();
  }

  action.addChange(MapHistory::SetTile(std::move(newTile)));

  history.commit(std::move(action));
}

Tile *MapView::getTile(const Position pos) const
{
  return map->getTile(pos);
}

void MapView::insertTile(Tile &&tile)
{
  MapHistory::Action action(MapHistory::ActionType::SetTile);

  action.addChange(MapHistory::SetTile(std::move(tile)));

  history.commit(std::move(action));
}

void MapView::removeTile(const Position position)
{
  MapHistory::Action action(MapHistory::ActionType::RemoveTile);

  action.addChange(MapHistory::RemoveTile(position));

  history.commit(std::move(action));
}

void MapView::updateViewport()
{
  const float zoom = 1 / camera.zoomFactor();
  const auto [x, y] = camera.position();

  bool changed = viewport.offset != camera.position() || viewport.zoom != zoom;

  if (changed)
  {
    viewport.zoom = zoom;
    viewport.offset = camera.position();
    notifyObservers(MapView::Observer::ChangeType::Viewport);
  }
}

void MapView::setViewportSize(int width, int height)
{
  viewport.width = width;
  viewport.height = height;
}

void MapView::setMousePos(const ScreenPosition pos)
{
  this->_mousePos = pos;
}

void MapView::deleteSelectedItems()
{
  if (selection.getPositions().empty())
  {
    return;
  }

  history.startGroup(ActionGroupType::RemoveMapItem);
  for (auto pos : selection.getPositions())
  {
    Tile &tile = *getTile(pos);
    if (tile.allSelected())
    {
      removeTile(tile.position());
    }
    else
    {
      removeSelectedItems(tile);
    }
  }

  // TODO: Save the selected item state
  selection.clear();

  history.endGroup(ActionGroupType::RemoveMapItem);
}

util::Rectangle<int> MapView::getGameBoundingRect() const
{
  MapPosition mapPos = viewport.offset.mapPos();

  auto [width, height] = ScreenPosition(viewport.width, viewport.height).mapPos(*this);
  util::Rectangle<int> rect;
  rect.x1 = mapPos.x;
  rect.y1 = mapPos.y;
  // Add one to not miss large sprites (64 in width or height) when zoomed in
  rect.x2 = mapPos.x + width + 10;
  rect.y2 = mapPos.y + height + 10;

  return rect;
}

void MapView::setDragStart(WorldPosition position)
{
  if (dragState.has_value())
  {
    dragState.value().from = position;
  }
  else
  {
    dragState = {position, position};
  }
}

bool MapView::hasSelection() const
{
  return !selection.empty();
}

bool MapView::isEmpty(Position position) const
{
  return map->isTileEmpty(position);
}

void MapView::setDragEnd(WorldPosition position)
{
  DEBUG_ASSERT(dragState.has_value(), "There is no current dragging operation.");

  dragState.value().to = position;
}

void MapView::finishMoveSelection(const Position moveDestination)
{
  if (selection.moving())
  {
    Position deltaPos = moveDestination - selection.moveOrigin.value();

    for (const Position pos : selection.getPositions())
    {
      Position newPos = pos + deltaPos;
      DEBUG_ASSERT(getTile(pos)->hasSelection(), "The tile at each position of a selection should have a selection.");

      selection.deselect(pos);
      map->moveSelectedItems(pos, newPos);
      selection.select(newPos);
    }
  }
  selection.moveOrigin.reset();
}

void MapView::endDragging()
{
  auto [fromWorldPos, toWorldPos] = dragState.value();

  Position from = fromWorldPos.toPos(*this);
  Position to = toWorldPos.toPos(*this);

  std::unordered_set<Position, PositionHash> positions;

  for (auto &location : map->getRegion(from, to))
  {
    Tile *tile = location.tile();
    if (tile && !tile->isEmpty())
    {
      positions.emplace(location.position());
    }
  }

  // Only commit a change if anything was dragged over
  if (!positions.empty())
  {
    history.startGroup(ActionGroupType::Selection);

    MapHistory::Action action(MapHistory::ActionType::Selection);

    action.addChange(MapHistory::SelectMultiple(positions));

    history.commit(std::move(action));
    history.endGroup(ActionGroupType::Selection);
  }

  dragState.reset();
  // This prevents having the mouse release trigger a deselect of the tile being hovered
  selection.blockDeselect = true;
}

bool MapView::isDragging() const
{
  return dragState.has_value();
}

void MapView::panEvent(MapView::PanEvent event)
{
  WorldPosition delta{};
  switch (event.type)
  {
  case PanType::Horizontal:
    delta.x += event.value;
    break;
  case PanType::Vertical:
    delta.y += event.value;
  }

  translateCamera(delta);
}

void MapView::mousePressEvent(VME::MouseButtons buttons)
{
  VME_LOG_D("MapView::mousePressEvent");
  if (buttons & VME::MouseButtons::LeftButton)
  {
    Position pos = _mousePos.toPos(*this);

    std::visit(
        util::overloaded{
            [this, pos](const MouseAction::None) {
              const Item *topItem = map->getTopItem(pos);
              if (!topItem)
              {
                clearSelection();
                return;
              }

              if (!topItem->selected)
              {
                clearSelection();
                history.startGroup(ActionGroupType::Selection);
                selectTopItem(pos);
                history.endGroup(ActionGroupType::Selection);
              }

              selection.moveOrigin = pos;
            },

            [this, pos](const MouseAction::RawItem &action) {
              history.startGroup(ActionGroupType::AddMapItem);
              addItem(pos, action.serverId);
              history.endGroup(ActionGroupType::AddMapItem);
            },

            [](const auto &) {
              ABORT_PROGRAM("Unknown change!");
            }},
        mapViewMouseAction.action());

    leftMouseDragPos = pos;
  }
}

void MapView::mouseMoveEvent(ScreenPosition mousePos)
{
  setMousePos(mousePos);
}

void MapView::mouseMoveEvent(VME::MouseButtons buttons)
{
  Position pos = _mousePos.toPos(*this);
  if (buttons & VME::MouseButtons::LeftButton)
  {
    if (leftMouseDragPos.has_value() && !util::contains(leftMouseDragPos, pos))
    {
      std::visit(
          util::overloaded{
              [this, pos](const MouseAction::None) {
              },

              [this, &pos](const MouseAction::RawItem &action) {
                history.startGroup(ActionGroupType::AddMapItem);
                addItem(pos, action.serverId);
                history.endGroup(ActionGroupType::AddMapItem);

                leftMouseDragPos = pos;
              },

              [](const auto &) {
                ABORT_PROGRAM("Unknown change!");
              }},
          mapViewMouseAction.action());

      leftMouseDragPos = pos;
    }
  }
}

void MapView::mouseReleaseEvent(VME::MouseButtons buttons)
{
  if (!(buttons & VME::MouseButtons::LeftButton))
  {
    leftMouseDragPos.reset();
    finishMoveSelection(mouseGamePos());
  }
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>Camera related>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/
void MapView::setCameraPosition(WorldPosition position)
{
  camera.setPosition(position);
}

void MapView::setX(long x)
{
  camera.setX(x);
}

void MapView::setY(long y)
{
  camera.setY(y);
}

void MapView::zoom(int delta)
{
  // TODO Handle the value of the zoom delta, not just its sign
  switch (util::sgn(delta))
  {
  case -1:
    camera.zoomOut(_mousePos);
    break;
  case 1:
    camera.zoomIn(_mousePos);
    break;
  default:
    break;
  }
}

void MapView::zoomOut()
{
  camera.zoomOut(_mousePos);
}
void MapView::zoomIn()
{
  camera.zoomIn(_mousePos);
}
void MapView::resetZoom()
{
  camera.resetZoom(_mousePos);
}

float MapView::getZoomFactor() const
{
  return camera.zoomFactor();
}

void MapView::translateCamera(WorldPosition delta)
{
  camera.translate(delta);
}

void MapView::translateX(long x)
{
  camera.setX(camera.x() + x);
}

void MapView::translateY(long y)
{
  camera.setY(camera.y() + y);
}

void MapView::translateZ(int z)
{
  camera.translateZ(z);
}

void MapView::addObserver(MapView::Observer *o)
{
  auto found = std::find(observers.begin(), observers.end(), o);
  if (found == observers.end())
  {
    observers.push_back(o);
    o->target = this;
  }
}

void MapView::removeObserver(MapView::Observer *o)
{
  auto found = std::find(observers.begin(), observers.end(), o);
  if (found != observers.end())
  {
    (*found)->target = nullptr;
    observers.erase(found);
  }
}

void MapView::notifyObservers(MapView::Observer::ChangeType changeType) const
{
  switch (changeType)
  {
  case Observer::ChangeType::Viewport:
    for (auto observer : observers)
    {
      observer->viewportChanged(viewport);
    }
    break;
  }
}

MapView::Observer::Observer(MapView *target)
    : target(target)
{
  if (target != nullptr)
  {
    target->addObserver(this);
  }
}

MapView::Observer::~Observer()
{
  if (target)
  {
    target->removeObserver(this);
  }
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>Internal API>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/
std::unique_ptr<Tile> MapView::setTileInternal(Tile &&tile)
{
  selection.setSelected(tile.position(), tile.hasSelection());

  TileLocation &location = map->getOrCreateTileLocation(tile.position());
  std::unique_ptr<Tile> currentTilePointer = location.replaceTile(std::move(tile));

  // Destroy the ECS entities of the old tile
  currentTilePointer->destroyEntities();

  return currentTilePointer;
}

std::unique_ptr<Tile> MapView::removeTileInternal(const Position position)
{
  Tile *oldTile = map->getTile(position);
  removeSelectionInternal(oldTile);

  oldTile->destroyEntities();

  return map->dropTile(position);
}

void MapView::removeSelectionInternal(Tile *tile)
{
  if (tile && tile->hasSelection())
    selection.deselect(tile->position());
}

MapHistory::Action MapView::newAction(MapHistory::ActionType actionType) const
{
  MapHistory::Action action(actionType);
  return action;
}
