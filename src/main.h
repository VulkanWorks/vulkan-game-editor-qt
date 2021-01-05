#pragma once

#include <QApplication>
#include <QVulkanInstance>
#include <filesystem>
#include <optional>
#include <stdarg.h>
#include <string>
#include <utility>
#include <vector>

#include "gui/mainwindow.h"
#include "gui/vulkan_window.h"

// TemporaryTest includes
#include <memory>

#include "map.h"
#include "octree.h"
// End TemporaryTest includes

namespace MainUtils
{
    void printOutfitAtlases(std::vector<uint32_t> outfitIds);
}

class MainApplication : public QApplication
{
  public:
    MainApplication(int &argc, char **argv);

    void initializeUI();

    int run();

    MainWindow mainWindow;

  public slots:
    void onApplicationStateChanged(Qt::ApplicationState state);
    void onFocusWindowChanged(QWindow *window);
    void onFocusWidgetChanged(QWidget *widget);

    void loadStyleSheet(const QString &path);

  private:
    QVulkanInstance vulkanInstance;

    QWindow *focusedWindow = nullptr;
    QWidget *prevWidget = nullptr;
    QWidget *currentWidget = nullptr;

    QWindow *vulkanWindow = nullptr;
};

namespace TemporaryTest
{
    void loadAllTexturesIntoMemory();

    void addChunk(Position from, vme::octree::Tree &tree);
    void testOctree();

    std::shared_ptr<Map> makeTestMap1();
    std::shared_ptr<Map> makeTestMap2();
} // namespace TemporaryTest