#include "MainWindow.h"
#include "TerrainDetailWidget.h"

#include "worldwright/TerrainDetailInteraction.h"
#include "worldwright/TerrainHeightEditor.h"

#include <QApplication>
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

    std::cout << "Terrain detail GUI failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
