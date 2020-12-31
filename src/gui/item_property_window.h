#pragma once

#include <QAbstractListModel>
#include <QListView>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickView>
#include <QUrl>

#include <memory>
#include <optional>
#include <unordered_map>

#include "../item.h"
#include "../signal.h"
#include "../util.h"
#include "draggable_item.h"
#include "qt_util.h"

class MainWindow;
class ItemPropertyWindow;

namespace GuiItemContainer
{
  struct ContainerNode;
  class ContainerModel : public QAbstractListModel
  {
    Q_OBJECT
    Q_PROPERTY(int capacity READ capacity NOTIFY capacityChanged)
    Q_PROPERTY(int size READ size)

  public:
    Q_INVOKABLE void containerItemClicked(int index);
    Q_INVOKABLE bool itemDropEvent(int index, QByteArray serializedDraggableItem);
    Q_INVOKABLE void itemDragStartEvent(int index);

    enum ContainerModelRole
    {
      ServerIdRole = Qt::UserRole + 1
    };

    ContainerModel(ContainerNode *treeNode, QObject *parent = 0);

    bool addItem(Item &&item);
    void reset();
    int capacity();
    int size();

    ItemData::Container *container() noexcept;
    ItemData::Container *container() const noexcept;

    void refresh();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

  signals:
    void capacityChanged(int capacity);

  protected:
    QHash<int, QByteArray> roleNames() const;

  private:
    void indexChanged(int index);

    ContainerNode *treeNode;
  };

  class ContainerListModel : public QAbstractListModel
  {
    Q_OBJECT
    Q_PROPERTY(int size READ size NOTIFY sizeChanged)

  public:
    enum class Role
    {
      ItemModel = Qt::UserRole + 1
    };

    ContainerListModel(QObject *parent = 0);

    void clear();
    void refresh(int index);
    void refresh(ContainerModel *model);

    void addItemModel(ContainerModel *model);
    void remove(int index);
    void remove(ContainerModel *model);

    std::vector<GuiItemContainer::ContainerModel *>::iterator find(const ContainerModel *model);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int size();

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

  signals:
    void sizeChanged(int size);

  protected:
    QHash<int, QByteArray> roleNames() const;

  private:
    std::vector<ContainerModel *> itemModels;
  };

  struct ContainerTreeSignals
  {
    Nano::Signal<void(ContainerModel *)> postOpened;
    Nano::Signal<void(ContainerModel *)> preClosed;
    Nano::Signal<bool(ContainerNode *, int, const ItemDrag::DraggableItem *)> itemDropped;
    Nano::Signal<void(ContainerNode *, int)> itemDragStarted;
  };

  struct ContainerNode : public Nano::Observer<>
  {
    ContainerNode(ItemData::Container *container, ContainerTreeSignals *_signals);
    ContainerNode(ItemData::Container *container, ContainerNode *parent);
    ~ContainerNode();

    virtual void setIndexInParent(int index) = 0;

    void onDragFinished(ItemDrag::DragOperation::DropResult result);

    virtual std::unique_ptr<ContainerNode> createChildNode(int index) = 0;
    virtual bool isRoot() const noexcept = 0;

    /*
      When an item is moved in a container with opened child containers,
      the indexInParentContainer of the children might need to be updated.
    */
    void itemMoved(int fromIndex, int toIndex);

    std::vector<uint16_t> indexChain(int index) const;
    std::vector<uint16_t> indexChain() const;

    ContainerModel *model();

    void open();
    void close();
    void toggle();

    void openChild(int index);
    void toggleChild(int index);

    void itemDropEvent(int index, ItemDrag::DraggableItem *droppedItem);
    void itemDragStartEvent(int index);

    ItemData::Container *container;
    std::unordered_map<int, std::unique_ptr<ContainerNode>> children;

  protected:
    ContainerTreeSignals *_signals;
    std::optional<ContainerModel> _model;

    bool opened = false;
  };

  struct ContainerTree
  {
    ContainerTree();

    struct Root : public ContainerNode
    {
      Root(MapView *mapView, Position mapPosition, uint16_t tileIndex, ItemData::Container *container, ContainerTreeSignals *_signals);

      void setIndexInParent(int index) override;

      std::unique_ptr<ContainerNode> createChildNode(int index) override;
      bool isRoot() const noexcept override { return true; }

    private:
      friend struct ContainerNode;
      Position mapPosition;
      MapView *mapView;
      uint16_t tileIndex;
    };

    struct Node : public ContainerNode
    {
      Node(ItemData::Container *container, ContainerNode *parent, uint16_t parentIndex);

      void setIndexInParent(int index) override;

      std::unique_ptr<ContainerNode> createChildNode(int index) override;
      bool isRoot() const noexcept { return false; }

      uint16_t indexInParentContainer;
      ContainerNode *parent;
    };

    const Item *rootItem() const;

    void setRootContainer(MapView *mapView, Position position, uint16_t tileIndex, ItemData::Container *item);
    bool hasRoot() const noexcept;

    void clear();

    void modelAddedEvent(ContainerModel *model);
    void modelRemovedEvent(ContainerModel *model);

    template <auto MemberFunction, typename T>
    void onContainerItemDrop(T *instance);

    template <auto MemberFunction, typename T>
    void onContainerItemDragStart(T *instance);

    ContainerListModel containerModel;

  private:
    ContainerTreeSignals _signals;
    std::optional<Root> root;
  };

} // namespace GuiItemContainer

class PropertyWindowEventFilter : public QtUtil::EventFilter
{
public:
  PropertyWindowEventFilter(ItemPropertyWindow *parent);

  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  ItemPropertyWindow *propertyWindow;
};

class QmlApplicationContext : public QObject
{
  Q_OBJECT
public:
  explicit QmlApplicationContext(QObject *parent = 0) : QObject(parent) {}
  Q_INVOKABLE void setCursor(Qt::CursorShape cursor)
  {
    QApplication::setOverrideCursor(cursor);
  }

  Q_INVOKABLE void resetCursor()
  {
    QApplication::restoreOverrideCursor();
  }
};

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>ItemPropertyWindow>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>

class ItemPropertyWindow : public QQuickView
{
  Q_OBJECT
signals:
  void countChanged(int count);

public:
  ItemPropertyWindow(QUrl url, MainWindow *mainWindow);

  void startContainerItemDrag(GuiItemContainer::ContainerNode *treeNode, int index);
  bool itemDropEvent(GuiItemContainer::ContainerNode *treeNode, int index, const ItemDrag::DraggableItem *droppedItem);

  QWidget *wrapInWidget(QWidget *parent = nullptr);

  void reloadSource();

  void refresh();

  void setMapView(MapView &mapView);
  void resetMapView();

  void focusItem(Item &item, Position &position, MapView &mapView);
  void focusGround(Position &position, MapView &mapView);
  void resetFocus();

  QWidget *wrapperWidget() const noexcept;

protected:
  bool event(QEvent *event);
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  friend class PropertyWindowEventFilter;
  void setCount(uint8_t count);
  void setContainerVisible(bool visible);

  /**
   * Returns a child from QML with objectName : name
   */
  inline QObject *child(const char *name);

  QUrl _url;
  MainWindow *mainWindow;
  QWidget *_wrapperWidget;

  GuiItemContainer::ContainerTree containerTree;

  struct FocusedItem
  {
    Position position;
    Item *item;
    size_t tileIndex;
  };

  struct FocusedGround
  {
    Position position;
    Item *ground;
  };

  struct State
  {
    MapView *mapView;
    std::variant<std::monostate, FocusedItem, FocusedGround> focusedItem;

    template <typename T>
    bool holds();

    template <typename T>
    T &focusedAs();
  };

  State state;

  std::optional<ItemDrag::DragOperation> dragOperation;
};

inline QObject *ItemPropertyWindow::child(const char *name)
{
  return rootObject()->findChild<QObject *>(name);
}

// Images
class ItemTypeImageProvider : public QQuickImageProvider
{
public:
  ItemTypeImageProvider()
      : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

  QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};

template <typename T>
bool ItemPropertyWindow::State::holds()
{
  static_assert(util::is_one_of<T, FocusedItem, FocusedGround>::value, "T must be FocusedItem or FocusedGround");

  return std::holds_alternative<T>(focusedItem);
}

template <typename T>
T &ItemPropertyWindow::State::focusedAs()
{
  static_assert(util::is_one_of<T, FocusedItem, FocusedGround>::value, "T must be FocusedItem or FocusedGround");

  return std::get<T>(focusedItem);
}

template <auto MemberFunction, typename T>
void GuiItemContainer::ContainerTree::onContainerItemDrop(T *instance)
{
  _signals.itemDropped.connect<MemberFunction>(instance);
}

template <auto MemberFunction, typename T>
void GuiItemContainer::ContainerTree::onContainerItemDragStart(T *instance)
{
  _signals.itemDragStarted.connect<MemberFunction>(instance);
}