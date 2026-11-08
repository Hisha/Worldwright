#ifndef WORLDWRIGHT_GUI_MAIN_WINDOW_H
#define WORLDWRIGHT_GUI_MAIN_WINDOW_H

#include "worldwright/EditorInteraction.h"
#include "worldwright/WorldEditorModel.h"

#include <QMainWindow>

#include <optional>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace worldwright::gui {

class WorldMapWidget;

class MainWindow final : public QMainWindow {
public:
    MainWindow();

private:
    void buildUi();
    void newWorld();
    void exportWorld();
    void refreshAll();
    void refreshProperties();
    void refreshTerrainUi();
    void updateStatus();
    void setEditMode(EditMode mode);
    void applyWorldName();
    void setSelectionEnabled(bool enabled);
    void applyElevation();

    WorldEditorModel m_model;
    WorldMapWidget* m_map = nullptr;
    QTreeWidget* m_worldTree = nullptr;
    QTreeWidgetItem* m_terrainTreeItem = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLabel* m_coordinates = nullptr;
    QLabel* m_selectionCount = nullptr;
    QCheckBox* m_enabled = nullptr;
    QDoubleSpinBox* m_elevation = nullptr;
    QLabel* m_modeStatus = nullptr;
    QLabel* m_hoverStatus = nullptr;
    QLabel* m_countStatus = nullptr;
    QLabel* m_zoomStatus = nullptr;
    EditMode m_editMode = EditMode::Select;
    std::optional<TileCoord> m_hoveredTile;
    double m_zoomPercent = 100.0;
    bool m_refreshing = false;
};

}  // namespace worldwright::gui

#endif
