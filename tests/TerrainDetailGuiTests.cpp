#include "MainWindow.h"
#include "TerrainDetailWidget.h"
#include "Terrain3DWidget.h"

#include "worldwright/TerrainDetailInteraction.h"
#include "worldwright/TerrainHeightEditor.h"

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMouseEvent>
#include <QPushButton>

#include <iostream>
#include <cmath>

namespace {
int failures = 0;
#define CHECK(expression)                                                        \
    do {                                                                         \
        if (!(expression)) {                                                     \
            ++failures;                                                          \
            std::cerr << "FAIL line " << __LINE__ << ": " #expression << '\n'; \
        }                                                                        \
    } while (false)

double totalHeight(const worldwright::WorldEditorModel& model) {
    double total = 0.0;
    for (const auto tile : model.world().enabledTiles()) {
        const auto* terrain = model.terrainAt(tile.x, tile.y);
        for (int row = 0;
             row < worldwright::TerrainDefinition::OuterVerticesPerTileSide;
             ++row)
            for (int column = 0;
                 column < worldwright::TerrainDefinition::OuterVerticesPerTileSide;
                 ++column)
                total += terrain->outerHeight(row, column);
        for (int row = 0;
             row < worldwright::TerrainDefinition::InnerVerticesPerTileSide;
             ++row)
            for (int column = 0;
                 column < worldwright::TerrainDefinition::InnerVerticesPerTileSide;
                 ++column)
                total += terrain->innerHeight(row, column);
    }
    return total;
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    worldwright::gui::MainWindow window;
    CHECK(window.workspaceMode() ==
          worldwright::gui::WorkspaceMode::WorldLayout);
    CHECK(!window.enterTerrainDetail());

    auto& model = window.model();
    CHECK(model.setTileEnabled(30, 30, true));
    CHECK(model.setTileEnabled(31, 30, true));
    model.selectTile(30, 30);
    CHECK(window.enterTerrainDetail());
    CHECK(window.workspaceMode() ==
          worldwright::gui::WorkspaceMode::TerrainDetail);
    CHECK(window.findChild<QComboBox*>("activeTerrainCombo") != nullptr);
    CHECK(window.findChild<QDoubleSpinBox*>("brushRadiusSpin")->value() > 0.0);
    CHECK(window.findChild<QDoubleSpinBox*>("brushStrengthSpin")->value() > 0.0);
    auto* radius3D = window.findChild<QDoubleSpinBox*>("brushRadius3DSpin");
    auto* strength3D = window.findChild<QDoubleSpinBox*>("brushStrength3DSpin");
    CHECK(radius3D != nullptr);
    CHECK(strength3D != nullptr);
    CHECK(window.findChild<QAction*>("terrain3DNavigateAction")->isChecked());
    window.findChild<QDoubleSpinBox*>("brushRadiusSpin")->setValue(42.0);
    CHECK(radius3D->value() == 42.0);
    strength3D->setValue(3.5);
    CHECK(window.findChild<QDoubleSpinBox*>("brushStrengthSpin")->value() ==
          3.5);
    window.findChild<QAction*>("terrain3DSmoothAction")->trigger();
    CHECK(strength3D->maximum() == 1.0);
    CHECK(strength3D->value() <= 1.0);
    window.findChild<QAction*>("terrain3DRaiseAction")->trigger();
    CHECK(strength3D->maximum() == 100.0);
    window.show();
    app.processEvents();

    auto* canvas = dynamic_cast<worldwright::gui::TerrainDetailWidget*>(
        window.findChild<QWidget*>("terrainDetailCanvas"));
    CHECK(canvas != nullptr);
    canvas->setBrushRadius(1.0);
    canvas->setBrushStrength(3.0);
    canvas->setTool(worldwright::gui::TerrainTool::Raise);
    const auto clickWorld =
        worldwright::TerrainHeightEditor::outerVertexPosition(30, 30, 30, 30);
    const auto fitted = worldwright::TerrainDetailInteraction::fitActive(
        {30, 30}, canvas->width(), canvas->height());
    const auto clickScreen = worldwright::TerrainDetailInteraction::worldToScreen(
        fitted, clickWorld);
    const QPointF local(clickScreen.x, clickScreen.y);
    QMouseEvent press(QEvent::MouseButtonPress, local, local, local, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, local, local, local, Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &release);
    CHECK(model.terrainAt(30, 30)->outerHeight(30, 30) == 3.0f);

    canvas->setBrushStrength(1.0);
    canvas->setTool(worldwright::gui::TerrainTool::Lower);
    QMouseEvent lowerPress(QEvent::MouseButtonPress, local, local, local, Qt::LeftButton,
                           Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &lowerPress);
    QMouseEvent lowerRelease(QEvent::MouseButtonRelease, local, local, local, Qt::LeftButton,
                             Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &lowerRelease);
    CHECK(model.terrainAt(30, 30)->outerHeight(30, 30) == 2.0f);

    canvas->setTool(worldwright::gui::TerrainTool::Smooth);
    QMouseEvent smoothPress(QEvent::MouseButtonPress, local, local, local, Qt::LeftButton,
                            Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &smoothPress);
    QMouseEvent smoothRelease(QEvent::MouseButtonRelease, local, local, local, Qt::LeftButton,
                              Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &smoothRelease);
    CHECK(std::abs(model.terrainAt(30, 30)->outerHeight(30, 30)) < 0.001f);

    const auto point = worldwright::TerrainHeightEditor::outerVertexPosition(
        30, 30, 20, 20);
    CHECK(worldwright::TerrainHeightEditor::raise(model, {point, 1.0, 3.0}));
    CHECK(window.setActiveTerrainTile({31, 30}));
    CHECK(window.setActiveTerrainTile({30, 30}));
    window.returnToWorldLayout();
    CHECK(window.workspaceMode() ==
          worldwright::gui::WorkspaceMode::WorldLayout);
    CHECK(model.terrainAt(30, 30)->outerHeight(20, 20) == 3.0f);
    CHECK(window.enterTerrainDetail());
    CHECK(model.terrainAt(30, 30)->outerHeight(20, 20) == 3.0f);

    // This exercises integration and CPU synchronization only. The offscreen
    // Qt platform may not provide a physical or software OpenGL context.
    window.enterTerrain3D();
    CHECK(window.workspaceMode() == worldwright::gui::WorkspaceMode::Terrain3D);
    auto* viewport = dynamic_cast<worldwright::gui::Terrain3DWidget*>(
        window.findChild<QWidget*>("terrain3DViewport"));
    if (viewport != nullptr) {
        app.processEvents();
        CHECK(!viewport->cpuMesh().empty());
        CHECK(viewport->cpuMesh().tiles.size() == 2);
        CHECK(viewport->isValid());
        CHECK(!viewport->hasGraphicsError());

        const QPointF center(viewport->width() * 0.5,
                             viewport->height() * 0.5);
        viewport->setTool(worldwright::gui::Terrain3DTool::Navigate);
        const double beforeNavigation = totalHeight(model);
        QMouseEvent navigationPress(QEvent::MouseButtonPress, center, center,
                                    center, Qt::LeftButton, Qt::LeftButton,
                                    Qt::NoModifier);
        QApplication::sendEvent(viewport, &navigationPress);
        QMouseEvent navigationRelease(QEvent::MouseButtonRelease, center, center,
                                      center, Qt::LeftButton, Qt::NoButton,
                                      Qt::NoModifier);
        QApplication::sendEvent(viewport, &navigationRelease);
        CHECK(totalHeight(model) == beforeNavigation);

        viewport->setTool(worldwright::gui::Terrain3DTool::Raise);
        QMouseEvent sculptPress(QEvent::MouseButtonPress, center, center,
                                center, Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier);
        QApplication::sendEvent(viewport, &sculptPress);
        CHECK(totalHeight(model) > beforeNavigation);
        CHECK(viewport->sculpting());
        viewport->setTool(worldwright::gui::Terrain3DTool::Navigate);
        CHECK(!viewport->sculpting());
        QMouseEvent sculptRelease(QEvent::MouseButtonRelease, center, center,
                                  center, Qt::LeftButton, Qt::NoButton,
                                  Qt::NoModifier);
        QApplication::sendEvent(viewport, &sculptRelease);
    } else {
        CHECK(window.findChild<QWidget*>("terrain3DUnavailable") != nullptr);
    }
    window.returnToWorldLayout();
    CHECK(model.terrainAt(30, 30)->outerHeight(20, 20) == 3.0f);

    std::cout << "Terrain detail GUI failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
