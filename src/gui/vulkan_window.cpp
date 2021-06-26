#include "vulkan_window.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <memory>

#include <QAction>
#include <QDialog>
#include <QDrag>
#include <QFocusEvent>
#include <QKeyCombination>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QQuickView>
#include <QWidget>

#include "../../vendor/rollbear-visit/visit.hpp"
#include "../brushes/brush.h"
#include "../brushes/creature_brush.h"
#include "../item_location.h"
#include "../logger.h"
#include "../map_renderer.h"
#include "../map_view.h"
#include "../position.h"
#include "../qt/logging.h"
#include "gui.h"
#include "mainwindow.h"
#include "qt_util.h"

// Static
std::unordered_set<const VulkanWindow *> VulkanWindow::instances;

VulkanWindow::VulkanWindow(std::shared_ptr<Map> map, EditorAction &editorAction)
    : QVulkanWindow(nullptr),
      vulkanInfo(this),
      editorAction(editorAction),
      mapView(std::make_unique<MapView>(std::make_unique<QtUtil::QtUiUtils>(this), editorAction, map)),
      scrollAngleBuffer(0)
{
    instances.emplace(this);

    connect(this, &VulkanWindow::scrollEvent, [=](int scrollDelta) { this->mapView->zoom(scrollDelta); });
    mapView->onMapItemDragStart<&VulkanWindow::mapItemDragStartEvent>(this);

    contextMenu = new QtContextMenu(this, widget);
    connect(contextMenu, &QtContextMenu::reopenRequest, this, &VulkanWindow::reopenContextMenuRequest);

    setShortcut(Qt::Key_Escape, ShortcutAction::Escape);
    setShortcut(Qt::Key_Delete, ShortcutAction::Delete);
    setShortcut(Qt::ControlModifier, Qt::Key_0, ShortcutAction::ResetZoom);
    setShortcut(Qt::KeypadModifier, Qt::Key_Plus, ShortcutAction::FloorUp);
    setShortcut(Qt::KeypadModifier, Qt::Key_Minus, ShortcutAction::FloorDown);
    setShortcut(Qt::Key_Q, ShortcutAction::LowerFloorShade);
}

VulkanWindow::~VulkanWindow()
{
    instances.erase(this);
}

void VulkanWindow::shortcutPressedEvent(ShortcutAction action, QKeyEvent *event)
{
    switch (action)
    {
        case ShortcutAction::Escape:
            mapView->escapeEvent();
            break;
        case ShortcutAction::Delete:
            mapView->deleteSelectedItems();
            break;
        case ShortcutAction::ResetZoom:
            mapView->resetZoom();
            break;
        case ShortcutAction::FloorUp:
            mapView->floorUp();
            break;
        case ShortcutAction::FloorDown:
            mapView->floorDown();

            break;
        case ShortcutAction::LowerFloorShade:
            mapView->toggleViewOption(MapView::ViewOption::ShadeLowerFloors);
            break;
    }
}

//>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>Events>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>

void VulkanWindow::mapItemDragStartEvent(Tile *tile, Item *item)
{
    ItemDrag::MapItem mapItem(mapView.get(), tile, item);
    dragOperation.emplace(ItemDrag::DragOperation::create(std::move(mapItem), mapView.get(), this));
    dragOperation->setRenderCondition([this] { return !this->containsMouse(); });
    dragOperation->start();
}

void VulkanWindow::shortcutReleasedEvent(ShortcutAction action, QKeyEvent *event)
{
    switch (action)
    {
        // case ShortcutAction::Pan:
        // {
        //     auto pan = mapView->editorAction.as<MouseAction::Pan>();
        //     if (pan)
        //     {
        //         unsetCursor();
        //         mapView->editorAction.setPrevious();
        //     }
        //     break;
        // }
        default:
            break;
    }
}

void VulkanWindow::mousePressEvent(QMouseEvent *event)
{
    bool mouseInside = containsMouse();
    mapView->setUnderMouse(mouseInside);
    // VME_LOG_D("VulkanWindow::mousePressEvent");
    mouseState.buttons = event->buttons();

    switch (event->button())
    {

        case Qt::MouseButton::LeftButton:
            if (!contextMenu->isHidden())
            {
                closeContextMenu();
            }
            else
            {
                mapView->mousePressEvent(QtUtil::vmeMouseEvent(event));
            }

            break;
        default:
            break;
    }

    event->ignore();
}

void VulkanWindow::mouseMoveEvent(QMouseEvent *event)
{
    // VME_LOG_D("VulkanWindow Move: " << event->pos());
    bool mouseInside = containsMouse();
    mapView->setUnderMouse(mouseInside);

    mouseState.buttons = event->buttons();
    mapView->mouseMoveEvent(QtUtil::vmeMouseEvent(event));

    // Update drag
    if (dragOperation)
    {
        dragOperation->mouseMoveEvent(event);
    }

    auto pos = event->scenePosition();
    util::Point<float> mousePos(pos.x(), pos.y());
    emit mousePosChanged(mousePos);

    event->ignore();
    QVulkanWindow::mouseMoveEvent(event);
}

void VulkanWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MouseButton::RightButton)
    {
        if (contextMenu->isHidden())
        {
            showContextMenu(event->globalPosition().toPoint());
        }
    }
    else if (event->button() == Qt::MouseButton::LeftButton)
    {
        // VME_LOG_D("VulkanWindow::mouseReleaseEvent");
        // Propagate drag operation
        if (dragOperation)
        {
            bool accepted = dragOperation->sendDropEvent(event);
            if (!accepted)
            {
                mapView->editorAction.as<MouseAction::DragDropItem>()->moveDelta.emplace(PositionConstants::Zero);
            }

            mapView->editorAction.unlock();
            mapView->editorAction.setPrevious();

            dragOperation.reset();
        }

        mouseState.buttons = event->buttons();
        mapView->mouseReleaseEvent(QtUtil::vmeMouseEvent(event));
    }
}

void VulkanWindow::wheelEvent(QWheelEvent *event)
{
    /*
    The minimum rotation amount for a scroll to be registered, in eighths of a degree.
    For example, 120 MinRotationAmount = (120 / 8) = 15 degrees of rotation.
  */
    const int MinRotationAmount = 120;

    // The relative amount that the wheel was rotated, in eighths of a degree.
    const int deltaY = event->angleDelta().y();

    scrollAngleBuffer += deltaY;
    if (std::abs(scrollAngleBuffer) >= MinRotationAmount)
    {
        emit scrollEvent(scrollAngleBuffer / 8);
        scrollAngleBuffer = 0;
    }
}

void VulkanWindow::keyReleaseEvent(QKeyEvent *e)
{
    if (e->isAutoRepeat())
        return;

    auto shortcut = shortcuts.find(e->key() | e->modifiers());
    if (shortcut != shortcuts.end())
    {
        shortcutReleasedEvent(shortcut->second);
        return;
    }

    switch (e->key())
    {
        case Qt::Key_Control:
        {
            auto brush = mapView->editorAction.as<MouseAction::MapBrush>();
            if (brush)
            {
                brush->erase = false;
            }
        }
    }
}

void VulkanWindow::keyPressEvent(QKeyEvent *e)
{
    switch (e->key())
    {
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
            e->ignore();
            emit keyPressedEvent(e);
            break;
        case Qt::Key_Control:
        {
            auto brush = mapView->editorAction.as<MouseAction::MapBrush>();
            if (brush)
            {
                brush->erase = true;
            }
            break;
        }
        default:
            e->ignore();
            QVulkanWindow::keyPressEvent(e);
            break;
    }
}

void VulkanWindow::setShortcut(Qt::KeyboardModifiers modifiers, Qt::Key key, ShortcutAction shortcut)
{
    int combination = QKeyCombination(modifiers, key).toCombined();
    shortcuts.emplace(combination, shortcut);
    shortcutActionToKeyCombination.emplace(shortcut, combination);
}

void VulkanWindow::setShortcut(Qt::Key key, ShortcutAction shortcut)
{
    int combination = QKeyCombination(key).toCombined();
    shortcuts.emplace(combination, shortcut);
    shortcutActionToKeyCombination.emplace(shortcut, combination);
}

std::optional<ShortcutAction> VulkanWindow::getShortcutAction(QKeyEvent *event) const
{
    auto shortcut = shortcuts.find(event->key() | event->modifiers());

    return shortcut != shortcuts.end() ? std::optional<ShortcutAction>(shortcut->second) : std::nullopt;
}

void VulkanWindow::dragEnterEvent(QDragEnterEvent *event)
{
    auto eventMimeData = event->mimeData();
    if (eventMimeData->hasFormat(ItemDrag::DraggableItemFormat))
    {
        DEBUG_ASSERT(util::hasDynamicType<ItemDrag::MimeData *>(eventMimeData), "Incorrect MimeData type.");

        auto mimeData = static_cast<const ItemDrag::MimeData *>(eventMimeData);
        mapView->overlay().draggedItem = mimeData->draggableItem->item();
    }
}

void VulkanWindow::dragMoveEvent(QDragMoveEvent *event)
{
    // TODO Only draw when game position changes
    mapView->requestDraw();
}

void VulkanWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    mapView->overlay().draggedItem = nullptr;
}

void VulkanWindow::dropEvent(QDropEvent *event)
{
    auto eventMimeData = event->mimeData();
    if (!eventMimeData->hasFormat(ItemDrag::DraggableItemFormat))
    {
        event->ignore();
        return;
    }

    DEBUG_ASSERT(util::hasDynamicType<ItemDrag::MimeData *>(eventMimeData), "Incorrect MimeData type.");
    auto mimeData = static_cast<const ItemDrag::MimeData *>(eventMimeData);

    mapView->overlay().draggedItem = nullptr;

    auto droppedItem = mimeData->draggableItem.get();

    if (util::hasDynamicType<ItemDrag::MapItem *>(droppedItem))
    {
        VME_LOG_D("dropEvent: MapItem");
        event->accept();
    }
    else if (util::hasDynamicType<ItemDrag::ContainerItemDrag *>(droppedItem))
    {
        VME_LOG_D("dropEvent: ContainerItemDrag");
        event->accept();
        auto containerDrag = static_cast<ItemDrag::ContainerItemDrag *>(droppedItem);

        mapView->history.beginTransaction(TransactionType::MoveItems);

        ContainerLocation move(containerDrag->position, containerDrag->tileIndex, std::move(containerDrag->containerIndices));

        auto &tile = mapView->getOrCreateTile(mapView->mouseGamePos());

        mapView->moveFromContainerToMap(move, tile);

        mapView->history.endTransaction(TransactionType::MoveItems);
    }
    else
    {
        event->ignore();
        return;
    }
}

bool VulkanWindow::event(QEvent *event)
{
    // if (!(event->type() == QEvent::UpdateRequest) && !(event->type() == QEvent::MouseMove))
    // {
    //     qDebug() << "[" << QString(debugName.c_str()) << "] " << event->type() << " { " << mapToGlobal(position()) << " }";
    // }

    switch (event->type())
    {
        case QEvent::Enter:
        {
            bool panning = mapView->editorAction.is<MouseAction::Pan>();
            if (panning)
            {
                setCursor(Qt::OpenHandCursor);
            }
            mapView->setUnderMouse(true);
            break;
        }

        case QEvent::DragEnter:
            dragEnterEvent(static_cast<QDragEnterEvent *>(event));
            mapView->setUnderMouse(true);
            mapView->dragEnterEvent();
            break;

        case QEvent::DragMove:
            dragMoveEvent(static_cast<QDragMoveEvent *>(event));
            break;

        case QEvent::Leave:
        {

            mapView->setUnderMouse(false);
            break;
        }

        case QEvent::DragLeave:
            dragLeaveEvent(static_cast<QDragLeaveEvent *>(event));
            mapView->setUnderMouse(false);
            mapView->dragLeaveEvent();
            break;

        case QEvent::Drop:
            dropEvent(static_cast<QDropEvent *>(event));
            break;

        case QEvent::ShortcutOverride:
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            auto action = getShortcutAction(keyEvent);
            if (action)
            {
                shortcutPressedEvent(action.value());
                return true;
            }
            break;
        }

        default:
            event->ignore();
            break;
    }

    return QVulkanWindow::event(event);
}

void VulkanWindow::eyedrop(const Position position) const
{
    TileThing topThing = mapView->map()->getTopThing(position);

    rollbear::visit(
        util::overloaded{
            [this](Item *item) {
                bool success = mainWindow->selectBrush(Brush::getOrCreateRawBrush(item->serverId()));

                // If the brush was not found in any tileset in any palette
                if (!success)
                {
                    mapView->editorAction.setRawBrush(item->serverId());
                }

                mapView->requestDraw();
            },
            [this](Creature *creature) {
                mainWindow->selectBrush(Brush::getCreatureBrush(creature->creatureType.id()));
                mapView->requestDraw();
            },

            [](const auto &arg) {
            }},
        topThing);
}

void VulkanWindow::lostFocus()
{
    if (contextMenu)
    {
        closeContextMenu();
    }
}

QWidget *VulkanWindow::wrapInWidget(QWidget *parent)
{
    QWidget *wrapper = QWidget::createWindowContainer(this, parent);
    QtUtil::setMapView(*wrapper, mapView.get());
    QtUtil::setVulkanWindow(*wrapper, this);
    wrapper->setObjectName("VulkanWindow wrapper");

    widget = wrapper;

    return wrapper;
}

QVulkanWindowRenderer *VulkanWindow::createRenderer()
{
    if (!renderer)
    {
        // Memory deleted by QT when QT closes
        renderer = new VulkanWindow::Renderer(*this);
    }

    return renderer;
}

QRect VulkanWindow::localGeometry() const
{
    return QRect(QPoint(0, 0), QPoint(width(), height()));
}

void VulkanWindow::closeContextMenu()
{
    contextMenu->close();
}

void VulkanWindow::reopenContextMenuRequest(QPoint globalPos)
{
    if (!localGeometry().contains(mapFromGlobal(globalPos)))
    {
        contextMenu->close();
        return;
    }
    else
    {
        showContextMenu(globalPos);
    }
}

void VulkanWindow::showContextMenu(QPoint position)
{
    auto menu = contextMenu;
    menu->close();
    menu->clear();

    // TODO Select the top thing that is right-clicked
    // TODO Make context menu change depending on the item that is being clicked

    QAction *cut = new QAction(tr("Cut"), menu);
    cut->setShortcut(Qt::CTRL | Qt::Key_X);
    menu->addAction(cut);

    QAction *copy = new QAction(tr("Copy"), menu);
    copy->setShortcut(Qt::CTRL | Qt::Key_C);
    menu->addAction(copy);

    QAction *paste = new QAction(tr("Paste"), menu);
    paste->setShortcut(Qt::CTRL | Qt::Key_V);
    menu->addAction(paste);

    QAction *del = new QAction(tr("Delete"), menu);
    del->setShortcut(Qt::Key_Delete);
    menu->addAction(del);

    menu->popup(position);
}

MapView *VulkanWindow::getMapView() const
{
    return mapView.get();
}

util::Size VulkanWindow::vulkanSwapChainImageSize() const
{
    QSize size = swapChainImageSize();
    return util::Size(size.width(), size.height());
}

void VulkanWindow::updateVulkanInfo()
{
    vulkanInfo.update();
}

bool VulkanWindow::containsMouse() const
{
    QSize windowSize = size();
    auto mousePos = mapFromGlobal(QCursor::pos());

    return (0 <= mousePos.x() && mousePos.x() <= windowSize.width()) && (0 <= mousePos.y() && mousePos.y() <= windowSize.height());
}

//>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>ContextMenu>>>>>
//>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>

QtContextMenu::QtContextMenu(VulkanWindow *window, QWidget *widget)
    : QMenu(widget)
{
}

bool QtContextMenu::selfClicked(QPoint pos) const
{
    return localGeometry().contains(pos);
}

void QtContextMenu::mouseReleaseEvent(QMouseEvent *event)
{
    switch (event->button())
    {
        case Qt::MouseButton::RightButton:
            if (!isHidden())
            {
                hide();
            }
            break;
    }

    QMenu::mousePressEvent(event);
}

void QtContextMenu::mousePressEvent(QMouseEvent *event)
{
    event->ignore();
    switch (event->button())
    {
        case Qt::MouseButton::RightButton:
        {
            if (!selfClicked(event->pos()))
            {
                emit reopenRequest(event->globalPos());
            }
            break;
        }
    }

    QMenu::mousePressEvent(event);
    // // Propagate the click event to the map window if appropriate
    // if (!selfClicked(event->pos()))
    // {
    //   auto posInWindow = window->mapFromGlobal(event->globalPos());
    //   VME_LOG_D("posInWindow: " << posInWindow);
    //   VME_LOG_D("Window geometry: " << window->geometry());
    //   if (window->localGeometry().contains(posInWindow.x(), posInWindow.y()))
    //   {
    //     VME_LOG_D("In window");
    //     window->mousePressEvent(event);
    //   }
    //   else
    //   {
    //     event->ignore();
    //     window->lostFocus();
    //   }
    // }
}

QRect QtContextMenu::localGeometry() const
{
    return QRect(QPoint(0, 0), QPoint(width(), height()));
}
QRect QtContextMenu::relativeGeometry() const
{
    VME_LOG_D("relativeGeometry");
    //  VME_LOG_D(parentWidget()->pos());
    QPoint p(geometry().left(), geometry().top());

    VME_LOG_D(parentWidget()->mapToGlobal(parentWidget()->pos()));

    VME_LOG_D("Top left: " << p);
    VME_LOG_D(mapToParent(p));

    return geometry();
}

//>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>Renderer>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>

VulkanWindow::Renderer::Renderer(VulkanWindow &window)
    : window(window),
      renderer(window.vulkanInfo, window.mapView.get()) {}

void VulkanWindow::Renderer::initResources()
{
    renderer.initResources(window.colorFormat());
};

void VulkanWindow::Renderer::initSwapChainResources()
{
    renderer.initSwapChainResources(window.vulkanSwapChainImageSize());
};

void VulkanWindow::Renderer::releaseSwapChainResources()
{
    renderer.releaseSwapChainResources();
};

void VulkanWindow::Renderer::releaseResources()
{
    renderer.releaseResources();
};

void VulkanWindow::Renderer::startNextFrame()
{
    renderer.setCurrentFrame(window.currentFrame());
    auto frame = renderer.currentFrame();
    frame->currentFrameIndex = window.currentFrame();
    frame->commandBuffer = window.currentCommandBuffer();
    frame->frameBuffer = window.currentFramebuffer();
    frame->mouseAction = window.mapView->editorAction.action();

    renderer.startNextFrame();
}
