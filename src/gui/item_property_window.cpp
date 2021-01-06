#include "item_property_window.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QWidget>

#include "../../vendor/rollbear-visit/visit.hpp"
#include "../qt/logging.h"
#include "draggable_item.h"
#include "mainwindow.h"

namespace ObjectName
{
    constexpr auto CountSpinBox = "count_spinbox";
    constexpr auto ActionIdSpinBox = "action_id_spinbox";
    constexpr auto UniqueIdSpinBox = "unique_id_spinbox";

    constexpr auto ItemContainerArea = "item_container_area";
} // namespace ObjectName

PropertyWindowEventFilter::PropertyWindowEventFilter(ItemPropertyWindow *parent)
    : QtUtil::EventFilter(static_cast<QObject *>(parent)), propertyWindow(parent) {}

bool PropertyWindowEventFilter::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type())
    {
        case QEvent::MouseMove:
            if (propertyWindow->dragOperation)
            {
                auto mouseEvent = static_cast<QMouseEvent *>(event);
                propertyWindow->dragOperation->mouseMoveEvent(mouseEvent);
                return false;
            }
            break;
        default:
            break;
    }

    return QObject::eventFilter(obj, event);
}

ItemPropertyWindow::ItemPropertyWindow(QUrl url, MainWindow *mainWindow)
    : _url(url), mainWindow(mainWindow), _wrapperWidget(nullptr)
{
    VME_LOG_D("ItemPropertyWindow address: " << this);
    installEventFilter(new PropertyWindowEventFilter(this));

    containerTree.onContainerItemDrop<&ItemPropertyWindow::itemDropEvent>(this);
    containerTree.onContainerItemDragStart<&ItemPropertyWindow::startContainerItemDrag>(this);

    QVariantMap properties;
    properties.insert("containers", QVariant::fromValue(&containerTree.containerModel));

    setInitialProperties(properties);

    qmlRegisterSingletonInstance("Vme.context", 1, 0, "C_PropertyWindow", this);

    engine()->addImageProvider(QLatin1String("itemTypes"), new ItemTypeImageProvider);

    setSource(_url);
    VME_LOG_D("After ItemPropertyWindow::setSource");

    QmlApplicationContext *applicationContext = new QmlApplicationContext();
    engine()->rootContext()->setContextProperty("applicationContext", applicationContext);
}

bool ItemPropertyWindow::event(QEvent *e)
{
    // if (e->type() != QEvent::UpdateRequest)
    // {
    //   qDebug() << e;
    // }

    return QQuickView::event(e);
}

void ItemPropertyWindow::mouseMoveEvent(QMouseEvent *event)
{
    QQuickView::mouseMoveEvent(event);

    // if (dragOperation)
    // {
    //   dragOperation->mouseMoveEvent(event);
    // }
}

void ItemPropertyWindow::mouseReleaseEvent(QMouseEvent *mouseEvent)
{
    QQuickView::mouseReleaseEvent(mouseEvent);

    if (dragOperation)
    {
        bool accepted = dragOperation->sendDropEvent(mouseEvent);
        VME_LOG_D("Drop accepted? " << accepted);
        if (accepted)
        {
            refresh();
        }

        dragOperation.reset();
    }
}

void ItemPropertyWindow::setMapView(MapView &mapView)
{
    state.mapView = &mapView;
}

void ItemPropertyWindow::resetMapView()
{
    state.mapView = nullptr;
}

void ItemPropertyWindow::focusGround(Position &position, MapView &mapView)
{
    setMapView(mapView);

    setContainerVisible(false);
    // containerModel.itemContainerModels.front()->reset();
    containerTree.clear();

    setCount(1);

    Item *groundItem = mapView.getTile(position)->ground();
    DEBUG_ASSERT(groundItem != nullptr, "Can not focus nullptr ground.");

    FocusedGround ground(position, groundItem);

    state.focusedItem.emplace<FocusedGround>(position, groundItem);
}

void ItemPropertyWindow::focusItem(Item *item, Position &position, MapView &mapView)
{
    if (item->isGround())
    {
        focusGround(position, mapView);
        return;
    }
    else if (state.holds<FocusedItem>())
    {
        auto &focusedItem = state.focusedAs<FocusedItem>();
        if (item == focusedItem.item())
        {
            // The item is already focused, update it
            focusedItem.position = position;

            auto maybeTileIndex = mapView.getTile(position)->indexOf(item);
            DEBUG_ASSERT(maybeTileIndex.has_value(), "The tile did not have the item.");

            focusedItem.tileIndex = static_cast<uint16_t>(maybeTileIndex.value());

            return;
        }
    }

    setMapView(mapView);

    auto maybeTileIndex = mapView.getTile(position)->indexOf(item);
    DEBUG_ASSERT(maybeTileIndex.has_value(), "The tile did not have the item.");
    auto tileIndex = static_cast<uint16_t>(maybeTileIndex.value());

    bool isContainer = item->isContainer();

    if (isContainer)
    {
        if (containerTree.rootItem() == item)
        {
            // This is already the focused item.
            return;
        }

        Container *container = item->getOrCreateContainer();
        container->setParent(&mapView, position);

        // DEBUG CODE, REMOVE
        if (container->empty() && item->serverId() == 2000)
        {
            container->addItem(Item(2595));
            auto &parcel = container->itemAt(0);

            auto bag = Item(1987);
            bag.getOrCreateContainer();

            parcel.getOrCreateContainer()->addItem(std::move(bag));
        }

        // if (container->empty())
        // {
        //     std::vector<uint32_t> serverIds{{1987, 2148, 5710, 2673, 2463, 2649}};

        //     for (const auto id : serverIds)
        //         container->addItem(Item(id));
        // }

        containerTree.setRootContainer(&mapView, position, tileIndex, item);
    }

    setContainerVisible(isContainer);
    setCount(item->count());

    state.focusedItem.emplace<FocusedItem>(position, item, tileIndex);
}

void ItemPropertyWindow::resetFocus()
{
    // itemContainerModels.front()->reset();
    containerTree.clear();
    setContainerVisible(false);
    setCount(1);
    state.focusedItem = std::monostate{};

    resetMapView();
}

void ItemPropertyWindow::setCount(uint8_t count)
{
    auto countSpinBox = child(ObjectName::CountSpinBox);
    countSpinBox->setProperty("value", count);
}

void ItemPropertyWindow::setContainerVisible(bool visible)
{
    auto containerArea = child(ObjectName::ItemContainerArea);
    if (containerArea)
    {
        containerArea->setProperty("visible", visible);
    }
    else
    {
        VME_LOG_D("Warning: could not find objectName: " << ObjectName::ItemContainerArea);
    }
}

QWidget *ItemPropertyWindow::wrapInWidget(QWidget *parent)
{
    DEBUG_ASSERT(_wrapperWidget == nullptr, "There is already a wrapper for this window.");

    _wrapperWidget = QWidget::createWindowContainer(this, parent);
    _wrapperWidget->setObjectName("ItemPropertyWindow wrapper");

    return _wrapperWidget;
}

QWidget *ItemPropertyWindow::wrapperWidget() const noexcept
{
    return _wrapperWidget;
}

void ItemPropertyWindow::reloadSource()
{
    VME_LOG_D("ItemPropertyWindow source reloaded.");
    engine()->clearComponentCache();
    setSource(QUrl::fromLocalFile("../resources/qml/itemPropertyWindow.qml"));
}

//>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>QML Callbacks>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>

void ItemPropertyWindow::refresh()
{
    if (containerTree.hasRoot())
    {
        auto containerArea = child(ObjectName::ItemContainerArea);
        if (containerArea->property("visible").toBool())
        {
            containerTree.containerModel.refresh(0);
        }
    }
}

bool ItemPropertyWindow::itemDropEvent(GuiItemContainer::ContainerNode *containerNode, int index, const ItemDrag::DraggableItem *droppedItem)
{
    using DragSource = ItemDrag::DraggableItem::Type;
    auto &focusedItem = state.focusedAs<FocusedItem>();
    if (droppedItem->item() == focusedItem.item())
    {
        VME_LOG_D("Can not add item to itself.");
        return false;
    }

    MapView *mapView = state.mapView;

    DragSource source = droppedItem->type();
    switch (source)
    {
        case DragSource::MapItem:
        {
            auto dropped = static_cast<const ItemDrag::MapItem *>(droppedItem);

            if (!(mapView == dropped->mapView))
            {
                ABORT_PROGRAM("Drag between different MapViews is not implemented.");
            }

            auto container = containerNode->container();

            mapView->history.beginTransaction(TransactionType::MoveItems);

            MapHistory::ContainerLocation to(
                focusedItem.position,
                static_cast<uint16_t>(focusedItem.tileIndex),
                containerNode->indexChain(index));

            mapView->moveFromMapToContainer(*dropped->tile, dropped->_item, to);

            mapView->history.endTransaction(TransactionType::MoveItems);

            containerNode->itemInserted(index);

            containerNode->model()->refresh();
            break;
        }
        case DragSource::ContainerItem:
        {
            auto dropped = static_cast<const ItemDrag::ContainerItemDrag *>(droppedItem);

            if (dropped->mapView != state.mapView)
            {
                ABORT_PROGRAM("Drag between different MapViews is not implemented.");
            }

            auto targetContainer = containerNode->container();

            // Dropped on the same container slot that the drag started
            if (dropped->container() == targetContainer && index == dropped->containerIndices.back())
            {
                return true;
            }

            MapHistory::ContainerLocation from(
                dropped->position,
                dropped->tileIndex,
                dropped->containerIndices);

            MapHistory::ContainerLocation to(
                focusedItem.position,
                static_cast<uint16_t>(focusedItem.tileIndex), containerNode->indexChain(index));

            mapView->history.beginTransaction(TransactionType::MoveItems);
            mapView->moveFromContainerToContainer(from, to);
            mapView->history.endTransaction(TransactionType::MoveItems);

            // Update child indices
            if (dropped->container() == targetContainer)
            {
                containerNode->itemMoved(dropped->containerIndices.back(), index);
                containerNode->draggedIndex.reset();
            }
            else
            {

                containerNode->itemInserted(index);
            }

            containerNode->model()->refresh();
            break;
        }
        default:
            VME_LOG_D("[ItemPropertyWindow::itemDropEvent] What do we do here?");
            return false;
    }

    return true;
}

void ItemPropertyWindow::startContainerItemDrag(GuiItemContainer::ContainerNode *treeNode, int index)
{
    VME_LOG_D("ItemPropertyWindow::startContainerItemDrag");

    const auto &focusedItem = state.focusedAs<FocusedItem>();

    ItemDrag::ContainerItemDrag itemDrag;
    itemDrag.mapView = state.mapView;
    itemDrag.position = focusedItem.position;

    itemDrag.containerIndices = treeNode->indexChain(index);
    itemDrag.tileIndex = static_cast<uint16_t>(focusedItem.tileIndex);
    // Add treeNode in itemdrag. Needed to update container indices.

    treeNode->draggedIndex.emplace(index);

    dragOperation.emplace(ItemDrag::DragOperation::create(std::move(itemDrag), state.mapView, this));
    dragOperation->setRenderCondition([this] { return !state.mapView->underMouse(); });
    dragOperation->start();
    dragOperation->onDragFinished<&GuiItemContainer::ContainerNode::onDragFinished>(treeNode);
}

GuiItemContainer::ContainerModel::ContainerModel(ContainerNode *treeNode, QObject *parent)
    : QAbstractListModel(parent), treeNode(treeNode)
{
    // TODO: Maybe this reset is not necessary?
    beginResetModel();
    endResetModel();
}

void GuiItemContainer::ContainerModel::refresh()
{
    beginResetModel();
    endResetModel();
}

void GuiItemContainer::ContainerModel::containerItemClicked(int index)
{
    if (index >= size())
        return;

    VME_LOG_D("containerItemClicked. Item id: " << containerItem()->serverId() << ", index: " << index);

    if (container()->itemAt(index).isContainer())
    {
        treeNode->toggleChild(index);
    }
}

void GuiItemContainer::ContainerModel::itemDragStartEvent(int index)
{
    treeNode->itemDragStartEvent(index);
}

bool GuiItemContainer::ContainerModel::itemDropEvent(int index, QByteArray serializedDraggableItem)
{
    VME_LOG_D("Index: " << index);
    auto droppedItem = ItemDrag::DraggableItem::deserialize(serializedDraggableItem);
    if (!droppedItem)
    {
        VME_LOG("[Warning]: Could not read DraggableItem from qml QByteArray.");
        return false;
    }

    // Only accept items that can be picked up
    if (!droppedItem->item()->itemType->hasFlag(AppearanceFlag::Take))
    {
        return false;
    }

    if (droppedItem->item() == container()->item())
    {
        VME_LOG_D("Can not add item to itself.");
        return false;
    }

    treeNode->itemDropEvent(index, droppedItem.get());
    return true;
}

int GuiItemContainer::ContainerModel::size()
{
    return static_cast<int>(treeNode->container()->size());
}

int GuiItemContainer::ContainerModel::capacity()
{
    return static_cast<int>(treeNode->container()->capacity());
}

Item *GuiItemContainer::ContainerModel::containerItem() const noexcept
{
    return treeNode->containerItem();
}

Container *GuiItemContainer::ContainerModel::container() const noexcept
{
    return treeNode->container();
}

Container *GuiItemContainer::ContainerModel::container() noexcept
{
    return const_cast<Container *>(const_cast<const GuiItemContainer::ContainerModel *>(this)->container());
}

bool GuiItemContainer::ContainerModel::addItem(Item &&item)
{
    if (container()->isFull())
        return false;

    int size = static_cast<int>(container()->size());

    // ContainerModel::createIndex(size, 0);

    // beginInsertRows(QModelIndex(), size, size + 1);
    bool added = container()->addItem(std::move(item));
    // endInsertRows();

    emit dataChanged(ContainerModel::createIndex(size, 0), ContainerModel::createIndex(size + 1, 0));
    return added;
}

void GuiItemContainer::ContainerModel::indexChanged(int index)
{
    auto modelIndex = ContainerModel::createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex);
}

int GuiItemContainer::ContainerModel::rowCount(const QModelIndex &parent) const
{
    return container()->capacity();
}

QVariant GuiItemContainer::ContainerModel::data(const QModelIndex &modelIndex, int role) const
{
    auto index = modelIndex.row();
    if (index < 0 || index >= rowCount())
        return QVariant();

    if (role == ServerIdRole)
    {
        if (index >= container()->size())
        {
            return -1;
        }
        else
        {
            return container()->itemAt(index).serverId();
        }
    }

    return QVariant();
}

QHash<int, QByteArray> GuiItemContainer::ContainerModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ServerIdRole] = "serverId";

    return roles;
}

QPixmap ItemTypeImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    bool success;
    auto serverId = id.toInt(&success);
    if (!success)
    {
        QPixmap pixmap(32, 32);
        pixmap.fill(QColor("black").rgba());
        return pixmap;
    }

    return QtUtil::itemPixmap(serverId);
}

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>ContainerListModel>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>

GuiItemContainer::ContainerListModel::ContainerListModel(QObject *parent)
    : QAbstractListModel(parent) {}

std::vector<GuiItemContainer::ContainerModel *>::iterator GuiItemContainer::ContainerListModel::find(const ContainerModel *model)
{
    return std::find_if(
        itemModels.begin(),
        itemModels.end(),
        [model](const ContainerModel *_model) { return model == _model; });
}

void GuiItemContainer::ContainerListModel::addItemModel(ContainerModel *model)
{
    auto modelSize = static_cast<int>(itemModels.size());
    beginInsertRows(QModelIndex(), modelSize, modelSize);
    itemModels.emplace_back(model);
    endInsertRows();
    emit sizeChanged(size());
}

void GuiItemContainer::ContainerListModel::remove(ContainerModel *model)
{
    auto found = std::remove_if(
        itemModels.begin(),
        itemModels.end(),
        [model](const ContainerModel *_model) { return model == _model; });

    if (found == itemModels.end())
    {
        VME_LOG_D("GuiItemContainer::ContainerModel::remove: ItemModel '" << model << "' was not present.");
        return;
    }

    itemModels.erase(found);
}

void GuiItemContainer::ContainerListModel::refresh(ContainerModel *model)
{
    auto found = find(model);
    DEBUG_ASSERT(found != itemModels.end(), "model was not present.");

    (*found)->refresh();
}

void GuiItemContainer::ContainerListModel::remove(int index)
{
    beginRemoveRows(QModelIndex(), index, index);
    itemModels.erase(itemModels.begin() + index);
    endRemoveRows();
    emit sizeChanged(size());
}

int GuiItemContainer::ContainerListModel::rowCount(const QModelIndex &parent) const
{
    return static_cast<int>(itemModels.size());
}

int GuiItemContainer::ContainerListModel::size()
{
    return rowCount();
}

QVariant GuiItemContainer::ContainerListModel::data(const QModelIndex &modelIndex, int role) const
{
    auto index = modelIndex.row();
    if (index < 0 || index >= rowCount())
        return QVariant();

    if (role == to_underlying(Role::ItemModel))
    {
        return QVariant::fromValue(itemModels.at(index));
    }

    return QVariant();
}

void GuiItemContainer::ContainerListModel::clear()
{
    if (itemModels.empty())
        return;

    beginResetModel();
    itemModels.clear();
    endResetModel();
    emit sizeChanged(size());
}

void GuiItemContainer::ContainerListModel::refresh(int index)
{
    itemModels.at(index)->refresh();
    auto modelIndex = createIndex(index, 0);
    dataChanged(modelIndex, modelIndex);
}

QHash<int, QByteArray> GuiItemContainer::ContainerListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[to_underlying(Role::ItemModel)] = "itemModel";

    return roles;
}

//>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>ContainterTree>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>
using ContainterTree = GuiItemContainer::ContainerTree;

GuiItemContainer::ContainerTree::ContainerTree()
{
    _signals.postOpened.connect<&ContainerTree::modelAddedEvent>(this);
    _signals.preClosed.connect<&ContainerTree::modelRemovedEvent>(this);
}

GuiItemContainer::ContainerTree::Root::Root(
    MapView *mapView,
    Position mapPosition,
    uint16_t tileIndex,
    Item *containerItem,
    ContainerTreeSignals *_signals)
    : ContainerNode(containerItem, _signals), mapPosition(mapPosition), mapView(mapView), tileIndex(tileIndex)
{
    VME_LOG_D("Root: " << this);
}

Item *GuiItemContainer::ContainerNode::containerItem() const
{
    return trackedContainerItem.item();
}

Container *GuiItemContainer::ContainerNode::container()
{
    return trackedContainerItem.item()->getOrCreateContainer();
}

std::unique_ptr<GuiItemContainer::ContainerNode> GuiItemContainer::ContainerTree::Root::createChildNode(int index)
{
    auto childItem = &container()->itemAt(index);

    auto childContainer = childItem->getOrCreateContainer();
    childContainer->setParent(mapView, mapPosition);

    return std::make_unique<ContainerTree::Node>(childItem, this, index);
}

GuiItemContainer::ContainerTree::Node::Node(Item *containerItem, ContainerNode *parent, uint16_t parentIndex)
    : ContainerNode(containerItem, parent), parent(parent), indexInParentContainer(parentIndex)
{
    VME_LOG_D("Node() with parent: " << parent);
}

std::unique_ptr<GuiItemContainer::ContainerNode> GuiItemContainer::ContainerTree::Node::createChildNode(int index)
{
    auto childItem = &container()->itemAt(index);

    auto childContainer = childItem->getOrCreateContainer();
    childContainer->setParent(parent->container());

    return std::make_unique<ContainerTree::Node>(childItem, this, index);
}

const Item *ContainterTree::rootItem() const
{
    return root ? root->containerItem() : nullptr;
}

bool ContainterTree::hasRoot() const noexcept
{
    return root.has_value();
}

void ContainterTree::setRootContainer(MapView *mapView, Position position, uint16_t tileIndex, Item *containerItem)
{
    root.emplace(mapView, position, tileIndex, containerItem, &_signals);
    root->open();
}

void ContainterTree::clear()
{
    root.reset();
    containerModel.clear();
}

void ContainterTree::modelAddedEvent(ContainerModel *model)
{
    containerModel.addItemModel(model);
}

void ContainterTree::modelRemovedEvent(ContainerModel *model)
{
    containerModel.remove(model);
}

GuiItemContainer::ContainerNode::ContainerNode(Item *containerItem, ContainerTreeSignals *_signals)
    : trackedContainerItem(containerItem), _signals(_signals)
{
    trackedContainerItem.onChanged<&GuiItemContainer::ContainerNode::updateChildContainerPointers>(this);
}

GuiItemContainer::ContainerNode::ContainerNode(Item *containerItem, ContainerNode *parent)
    : trackedContainerItem(containerItem), _signals(parent->_signals)
{
    trackedContainerItem.onChanged<&GuiItemContainer::ContainerNode::updateChildContainerPointers>(this);
}

GuiItemContainer::ContainerNode::~ContainerNode()
{
    if (opened)
    {
        close();
    }
}

void GuiItemContainer::ContainerNode::onDragFinished(ItemDrag::DragOperation::DropResult result)
{
    using DropResult = ItemDrag::DragOperation::DropResult;

    if (result == DropResult::Accepted)
    {
        // TODO
        // It would be faster to only refresh the changed indices. But this should
        // not make a significant difference in performance, because the model will
        // have at most ~25 items (max capacity of the largest container item).
        _model->refresh();

        if (draggedIndex.has_value())
        {
            int index = draggedIndex.value();
            itemRemoved(index);
        }
    }
}

void GuiItemContainer::ContainerTree::Node::setIndexInParent(int index)
{
    indexInParentContainer = index;
    Items::items.itemMoved(&parent->container()->itemAt(index));
}

void GuiItemContainer::ContainerTree::Root::setIndexInParent(int index)
{
    ABORT_PROGRAM("Can not be used on a Root node.");
}

void GuiItemContainer::ContainerNode::itemInserted(int index)
{
    if (children.empty())
        return;

    std::vector<int> incIndices;

    for (const auto &[i, _] : children)
    {
        if (i >= index)
        {
            incIndices.push_back(i);
        }
    }

    for (int i : incIndices)
    {
        int newIndex = i + 1;

        auto mapNode = children.extract(i);
        mapNode.key() = newIndex;
        children.insert(std::move(mapNode));

        children.at(newIndex)->setIndexInParent(newIndex);
    }
}

void GuiItemContainer::ContainerNode::itemRemoved(int index)
{
    if (children.empty())
        return;

    std::vector<int> incIndices;
    std::vector<int> decIndices;

    for (const auto &[i, _] : children)
    {
        if (i >= index)
        {
            decIndices.push_back(i);
        }
    }

    for (int i : decIndices)
    {
        int newIndex = i - 1;

        auto mapNode = children.extract(i);
        mapNode.key() = newIndex;
        children.insert(std::move(mapNode));

        children.at(newIndex)->setIndexInParent(newIndex);
    }
}

void GuiItemContainer::ContainerNode::itemMoved(int fromIndex, int toIndex)
{
    if (children.empty())
        return;

    std::vector<std::pair<int, int>> changes;

    for (const auto &[i, _] : children)
    {
        int newIndex;
        if (i == fromIndex)
        {
            newIndex = toIndex;
        }
        else if (fromIndex < i && i <= toIndex)
        {
            newIndex = i - 1;
        }
        else if (toIndex <= i && i < fromIndex)
        {
            newIndex = i + 1;
        }
        else
        {
            continue;
        }

        changes.emplace_back(std::make_pair(i, newIndex));
    }

    for (const auto [fromIndex, toIndex] : changes)
    {
        auto mapNode = children.extract(fromIndex);
        mapNode.key() = toIndex;
        children.insert(std::move(mapNode));

        children.at(toIndex)->setIndexInParent(toIndex);
    }
}

void GuiItemContainer::ContainerNode::updateChildContainerPointers(Item *trackedItem)
{
    for (auto &entry : children)
    {
        auto &i = container()->itemAt(entry.first);
        Items::items.itemMoved(&i);
    }
}

std::vector<uint16_t> GuiItemContainer::ContainerNode::indexChain() const
{
    return indexChain(0);
}

std::vector<uint16_t> GuiItemContainer::ContainerNode::indexChain(int index) const
{
    std::vector<uint16_t> result;
    result.emplace_back(index);

    auto current = this;
    while (!current->isRoot())
    {
        auto node = static_cast<const ContainerTree::Node *>(current);
        result.emplace_back(node->indexInParentContainer);
        current = node->parent;
    }

    std::reverse(result.begin(), result.end());
    return result;
}

GuiItemContainer::ContainerModel *GuiItemContainer::ContainerNode::model()
{
    return _model.has_value() ? &(*_model) : nullptr;
}

void GuiItemContainer::ContainerNode::open()
{
    DEBUG_ASSERT(!opened, "Already opened.");

    _model.emplace(this);
    _signals->postOpened.fire(&_model.value());
    opened = true;
}

void GuiItemContainer::ContainerNode::close()
{
    _signals->preClosed.fire(&_model.value());
    _model.reset();
    opened = false;
}

void GuiItemContainer::ContainerNode::toggle()
{
    if (opened)
    {
        close();
    }
    else
    {
        open();
    }
}

void GuiItemContainer::ContainerNode::openChild(int index)
{
    DEBUG_ASSERT(children.find(index) == children.end(), "The child is already opened.");
    auto &child = container()->itemAt(index);

    DEBUG_ASSERT(child.isContainer(), "Must be container.");
    auto node = createChildNode(index);
    children.emplace(index, std::move(node));
    auto &c = *children.at(index);
    ContainerTree::Node *d = dynamic_cast<ContainerTree::Node *>(&c);
    children.at(index)->open();
}

void GuiItemContainer::ContainerNode::toggleChild(int index)
{
    auto child = children.find(index);
    if (child == children.end())
    {
        openChild(index);
        return;
    }
    child->second->toggle();
}

void GuiItemContainer::ContainerNode::itemDragStartEvent(int index)
{
    _signals->itemDragStarted.fire(this, index);
}

void GuiItemContainer::ContainerNode::itemDropEvent(int index, ItemDrag::DraggableItem *droppedItem)
{
    // TODO Maybe use fire_accumulate here to see if drop was accepted.
    //Drop **should** always be accepted for now, but that might change in the future.

    // Index must be in [0, size - 1]
    index = std::min(index, std::max(_model->size() - 1, 0));

    _signals->itemDropped.fire(this, index, droppedItem);
}
