#ifndef WORLDWRIGHT_GUI_MAIN_WINDOW_H
#define WORLDWRIGHT_GUI_MAIN_WINDOW_H

#include "worldwright/EditorInteraction.h"
#include "worldwright/WorldEditorModel.h"

#include <QMainWindow>

#include <optional>

class QCheckBox;
class QAction;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QToolBar;
class QTreeWidget;
class QTreeWidgetItem;

namespace worldwright::gui {

class WorldMapWidget;
class TerrainDetailWidget;
enum class TerrainTool;

enum class WorkspaceMode { WorldLayout, TerrainDetail };

class MainWindow final : public QMainWindow {
public:
    MainWindow();

    WorkspaceMode workspaceMode() const noexcept;
    bool enterTerrainDetail();
    void returnToWorldLayout();
    bool setActiveTerrainTile(TileCoord tile);
    WorldEditorModel& model() noexcept;
    const WorldEditorModel& model() const noexcept;

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
    void refreshActiveTerrainChoices();
    void setTerrainTool(TerrainTool tool);

    WorldEditorModel m_model;
    WorldMapWidget* m_map = nullptr;
    TerrainDetailWidget* m_terrainDetail = nullptr;
    QStackedWidget* m_workspaces = nullptr;
    QWidget* m_layoutPage = nullptr;
    QWidget* m_detailPage = nullptr;
    QToolBar* m_layoutToolbar = nullptr;
    QToolBar* m_detailToolbar = nullptr;
    QAction* m_editTerrainAction = nullptr;
    QAction* m_returnAction = nullptr;
    QComboBox* m_activeTerrain = nullptr;
    QDoubleSpinBox* m_brushRadius = nullptr;
    QDoubleSpinBox* m_brushStrength = nullptr;
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
    WorkspaceMode m_workspaceMode = WorkspaceMode::WorldLayout;
    bool m_refreshing = false;
};

}  // namespace worldwright::gui

#endif
