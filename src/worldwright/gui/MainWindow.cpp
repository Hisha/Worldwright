#include "MainWindow.h"

#include "TerrainDetailWidget.h"
#include "Terrain3DWidget.h"
#include "WorldMapWidget.h"
#include "worldwright/WorldExporter.h"

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

namespace worldwright::gui {

MainWindow::MainWindow() {
    buildUi();
    refreshAll();
}

void MainWindow::buildUi() {
    setWindowTitle("Worldwright");
    resize(1280, 820);

    auto* fileMenu = menuBar()->addMenu("&File");
    auto* newAction = fileMenu->addAction("&New World...");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this] { newWorld(); });
    auto* exportAction = fileMenu->addAction("&Export World...");
    exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(exportAction, &QAction::triggered, this, [this] { exportWorld(); });
    fileMenu->addSeparator();
    auto* quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto* viewMenu = menuBar()->addMenu("&View");
    auto* worldLayoutAction = viewMenu->addAction("&World Layout");
    auto* terrainDetailAction = viewMenu->addAction("&Terrain Detail");
    auto* terrain3DAction = viewMenu->addAction("&3D View");
    viewMenu->addSeparator();
    auto* fitAction = viewMenu->addAction("&Fit World");
    auto* resetAction = viewMenu->addAction("&Reset View");
    m_editTerrainAction = viewMenu->addAction("Edit &Terrain");
    m_returnAction = viewMenu->addAction("Return to &World Layout");
    m_returnAction->setVisible(false);
    connect(m_editTerrainAction, &QAction::triggered, this, [this] {
        if (!enterTerrainDetail()) {
            QMessageBox::information(this, "Edit Terrain",
                                     "Select one enabled terrain tile first.");
        }
    });
    connect(m_returnAction, &QAction::triggered, this,
            [this] { returnToWorldLayout(); });
    connect(worldLayoutAction, &QAction::triggered, this,
            [this] { returnToWorldLayout(); });
    connect(terrainDetailAction, &QAction::triggered, this, [this] {
        if (!enterTerrainDetail()) {
            QMessageBox::information(this, "Edit Terrain",
                                     "Select one enabled terrain tile first.");
        }
    });
    connect(terrain3DAction, &QAction::triggered, this,
            [this] { enterTerrain3D(); });

    m_layoutToolbar = addToolBar("World Layout Tools");
    m_layoutToolbar->setObjectName("worldLayoutToolbar");
    m_layoutToolbar->setMovable(false);
    m_layoutToolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* modeGroup = new QActionGroup(m_layoutToolbar);
    modeGroup->setExclusive(true);
    const auto addMode = [this, modeGroup](const QString& label,
                                           EditMode mode) {
        auto* action = m_layoutToolbar->addAction(label);
        action->setCheckable(true);
        modeGroup->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, mode] { setEditMode(mode); });
        return action;
    };
    auto* selectAction = addMode("Select", EditMode::Select);
    addMode("Paint Terrain", EditMode::PaintTerrain);
    addMode("Erase Terrain", EditMode::EraseTerrain);
    addMode("Pan", EditMode::Pan);
    selectAction->setChecked(true);
    m_layoutToolbar->addSeparator();
    m_layoutToolbar->addAction(fitAction);

    m_detailToolbar = addToolBar("Terrain Detail Tools");
    m_detailToolbar->setObjectName("terrainDetailToolbar");
    m_detailToolbar->setMovable(false);
    m_detailToolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* terrainGroup = new QActionGroup(m_detailToolbar);
    terrainGroup->setExclusive(true);
    const auto addTerrainTool = [this, terrainGroup](const QString& label,
                                                     TerrainTool tool) {
        auto* action = m_detailToolbar->addAction(label);
        action->setCheckable(true);
        terrainGroup->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, tool] { setTerrainTool(tool); });
        return action;
    };
    auto* raiseAction = addTerrainTool("Raise", TerrainTool::Raise);
    addTerrainTool("Lower", TerrainTool::Lower);
    addTerrainTool("Smooth", TerrainTool::Smooth);
    addTerrainTool("Pan", TerrainTool::Pan);
    raiseAction->setChecked(true);
    m_detailToolbar->addSeparator();
    auto* fitTerrainAction = m_detailToolbar->addAction("Fit Terrain");
    m_detailToolbar->addAction(m_returnAction);
    m_detailToolbar->setVisible(false);

    m_terrain3DToolbar = addToolBar("3D View Tools");
    m_terrain3DToolbar->setObjectName("terrain3DToolbar");
    m_terrain3DToolbar->setMovable(false);
    m_terrain3DToolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* terrain3DGroup = new QActionGroup(m_terrain3DToolbar);
    terrain3DGroup->setExclusive(true);
    const auto addTerrain3DTool = [this, terrain3DGroup](
                                      const QString& label,
                                      Terrain3DTool tool) {
        auto* action = m_terrain3DToolbar->addAction(label);
        action->setObjectName("terrain3D" + label + "Action");
        action->setCheckable(true);
        terrain3DGroup->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, tool] { setTerrain3DTool(tool); });
        return action;
    };
    auto* navigate3DAction =
        addTerrain3DTool("Navigate", Terrain3DTool::Navigate);
    addTerrain3DTool("Raise", Terrain3DTool::Raise);
    addTerrain3DTool("Lower", Terrain3DTool::Lower);
    addTerrain3DTool("Smooth", Terrain3DTool::Smooth);
    navigate3DAction->setChecked(true);
    m_terrain3DToolbar->addSeparator();
    auto* fit3DAction = m_terrain3DToolbar->addAction("Fit");
    auto* reset3DAction = m_terrain3DToolbar->addAction("Reset");
    auto* refresh3DAction = m_terrain3DToolbar->addAction("Refresh");
    m_terrain3DToolbar->addSeparator();
    auto* wireframeAction = m_terrain3DToolbar->addAction("Wireframe");
    wireframeAction->setCheckable(true);
    auto* boundariesAction = m_terrain3DToolbar->addAction("ADT Boundaries");
    boundariesAction->setCheckable(true);
    boundariesAction->setChecked(true);
    m_terrain3DToolbar->addSeparator();
    m_terrain3DToolbar->addAction(m_returnAction);
    m_terrain3DToolbar->setVisible(false);

    auto* splitter = new QSplitter(this);
    setCentralWidget(splitter);

    m_worldTree = new QTreeWidget(splitter);
    m_worldTree->setHeaderLabel("World");
    m_worldTree->setMinimumWidth(190);

    m_workspaces = new QStackedWidget(splitter);
    m_workspaces->setObjectName("workspaceStack");
    m_layoutPage = new QWidget(m_workspaces);
    auto* center = m_layoutPage;
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(8, 8, 8, 8);
    auto* instructions = new QLabel(
        "Choose a tool, then click or drag on the world. Mouse wheel zooms; "
        "right- or middle-drag pans in any tool.", center);
    instructions->setWordWrap(true);
    centerLayout->addWidget(instructions);

    auto* controls = new QWidget(center);
    auto* controlsLayout = new QHBoxLayout(controls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    auto* enableButton = new QPushButton("Enable selected", controls);
    auto* disableButton = new QPushButton("Disable selected", controls);
    auto* editTerrainButton = new QPushButton("Edit Terrain", controls);
    editTerrainButton->setObjectName("editTerrainButton");
    auto* resetViewButton = new QPushButton("Reset view", controls);
    controlsLayout->addWidget(enableButton);
    controlsLayout->addWidget(disableButton);
    controlsLayout->addWidget(editTerrainButton);
    auto* view3DButton = new QPushButton("3D View", controls);
    view3DButton->setObjectName("view3DButton");
    controlsLayout->addWidget(view3DButton);
    controlsLayout->addStretch();
    controlsLayout->addWidget(resetViewButton);
    centerLayout->addWidget(controls);
    connect(enableButton, &QPushButton::clicked, this,
            [this] { setSelectionEnabled(true); });
    connect(disableButton, &QPushButton::clicked, this,
            [this] { setSelectionEnabled(false); });
    connect(editTerrainButton, &QPushButton::clicked, this, [this] {
        if (!enterTerrainDetail()) {
            QMessageBox::information(this, "Edit Terrain",
                                     "Select one enabled terrain tile first.");
        }
    });
    connect(view3DButton, &QPushButton::clicked, this,
            [this] { enterTerrain3D(); });

    m_map = new WorldMapWidget(&m_model, center);
    centerLayout->addWidget(m_map, 1);
    connect(resetViewButton, &QPushButton::clicked, m_map,
            &WorldMapWidget::resetView);
    connect(resetAction, &QAction::triggered, m_map,
            &WorldMapWidget::resetView);
    connect(fitAction, &QAction::triggered, m_map, &WorldMapWidget::fitWorld);
    m_map->selectionChanged = [this] { refreshAll(); };
    m_map->terrainChanged = [this] { refreshTerrainUi(); };
    m_map->hoverChanged = [this](std::optional<TileCoord> tile) {
        m_hoveredTile = tile;
        updateStatus();
    };
    m_map->zoomChanged = [this](double percent) {
        m_zoomPercent = percent;
        updateStatus();
    };

    m_workspaces->addWidget(m_layoutPage);
    m_detailPage = new QWidget(m_workspaces);
    auto* detailLayout = new QVBoxLayout(m_detailPage);
    detailLayout->setContentsMargins(8, 8, 8, 8);
    auto* detailControls = new QWidget(m_detailPage);
    auto* detailControlsLayout = new QHBoxLayout(detailControls);
    detailControlsLayout->setContentsMargins(0, 0, 0, 0);
    auto* returnButton = new QPushButton("Return to World", detailControls);
    returnButton->setObjectName("returnToWorldButton");
    m_activeTerrain = new QComboBox(detailControls);
    m_activeTerrain->setObjectName("activeTerrainCombo");
    m_activeTerrain->setMinimumContentsLength(12);
    m_brushRadius = new QDoubleSpinBox(detailControls);
    m_brushRadius->setObjectName("brushRadiusSpin");
    m_brushRadius->setRange(TerrainHeightEditor::CellSize * 0.25,
                            TerrainHeightEditor::TileSize * 4.0);
    m_brushRadius->setDecimals(2);
    m_brushRadius->setSingleStep(TerrainHeightEditor::CellSize);
    m_brushRadius->setSuffix(" world units");
    m_brushRadius->setValue(TerrainHeightEditor::CellSize * 4.0);
    m_brushRadius->setToolTip(
        "World-space brush radius; one terrain cell is " +
        QString::number(TerrainHeightEditor::CellSize, 'f', 2) + " units.");
    m_brushStrength = new QDoubleSpinBox(detailControls);
    m_brushStrength->setObjectName("brushStrengthSpin");
    m_brushStrength->setRange(0.01, 100.0);
    m_brushStrength->setDecimals(2);
    m_brushStrength->setSingleStep(0.25);
    m_brushStrength->setValue(1.0);
    m_brushStrength->setToolTip(
        "Raise/Lower: elevation delta per spaced sample. Smooth: blend "
        "fraction from 0 to 1.");
    detailControlsLayout->addWidget(returnButton);
    detailControlsLayout->addSpacing(12);
    detailControlsLayout->addWidget(new QLabel("Active ADT", detailControls));
    detailControlsLayout->addWidget(m_activeTerrain);
    detailControlsLayout->addSpacing(12);
    detailControlsLayout->addWidget(new QLabel("Radius", detailControls));
    detailControlsLayout->addWidget(m_brushRadius);
    detailControlsLayout->addWidget(new QLabel("Strength", detailControls));
    detailControlsLayout->addWidget(m_brushStrength);
    detailControlsLayout->addStretch();
    detailLayout->addWidget(detailControls);
    auto* detailHelp = new QLabel(
        "Left-click or drag sculpts. Mouse wheel zooms at the cursor; "
        "right-, middle-, or Pan-tool drag moves the view.", m_detailPage);
    detailHelp->setWordWrap(true);
    detailLayout->addWidget(detailHelp);
    m_terrainDetail = new TerrainDetailWidget(&m_model, m_detailPage);
    detailLayout->addWidget(m_terrainDetail, 1);
    m_workspaces->addWidget(m_detailPage);
    connect(returnButton, &QPushButton::clicked, this,
            [this] { returnToWorldLayout(); });
    connect(fitTerrainAction, &QAction::triggered, m_terrainDetail,
            &TerrainDetailWidget::fitTerrain);
    connect(m_brushRadius, qOverload<double>(&QDoubleSpinBox::valueChanged),
            m_terrainDetail, &TerrainDetailWidget::setBrushRadius);
    connect(m_brushStrength, qOverload<double>(&QDoubleSpinBox::valueChanged),
            m_terrainDetail, &TerrainDetailWidget::setBrushStrength);
    connect(m_activeTerrain, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (m_refreshing || index < 0) {
                    return;
                }
                setActiveTerrainTile(
                    {m_activeTerrain->itemData(index, Qt::UserRole).toInt(),
                     m_activeTerrain->itemData(index, Qt::UserRole + 1).toInt()});
            });
    m_terrainDetail->terrainChanged = [this] { refreshTerrainUi(); };
    m_terrainDetail->hoverChanged =
        [this](std::optional<TerrainPoint> point, bool editable) {
            if (point.has_value()) {
                m_hoverStatus->setText(
                    "Terrain: " + QString::number(point->x, 'f', 2) + ", " +
                    QString::number(point->y, 'f', 2) +
                    (editable ? " (editable)" : " (unavailable)"));
            } else {
                m_hoverStatus->setText("Terrain: —");
            }
        };
    m_terrainDetail->zoomChanged = [this](double percent) {
        m_zoomPercent = percent;
        updateStatus();
    };

    m_terrain3DPage = new QWidget(m_workspaces);
    auto* terrain3DLayout = new QVBoxLayout(m_terrain3DPage);
    terrain3DLayout->setContentsMargins(8, 8, 8, 8);
    auto* terrain3DControls = new QWidget(m_terrain3DPage);
    auto* terrain3DControlsLayout = new QHBoxLayout(terrain3DControls);
    terrain3DControlsLayout->setContentsMargins(0, 0, 0, 0);
    m_brushRadius3D = new QDoubleSpinBox(terrain3DControls);
    m_brushRadius3D->setObjectName("brushRadius3DSpin");
    m_brushRadius3D->setRange(TerrainHeightEditor::CellSize * 0.25,
                              TerrainHeightEditor::TileSize * 4.0);
    m_brushRadius3D->setDecimals(2);
    m_brushRadius3D->setSingleStep(TerrainHeightEditor::CellSize);
    m_brushRadius3D->setSuffix(" world units");
    m_brushRadius3D->setValue(m_brushRadius->value());
    m_brushStrength3D = new QDoubleSpinBox(terrain3DControls);
    m_brushStrength3D->setObjectName("brushStrength3DSpin");
    m_brushStrength3D->setRange(0.01, 100.0);
    m_brushStrength3D->setDecimals(2);
    m_brushStrength3D->setSingleStep(0.25);
    m_brushStrength3D->setValue(m_brushStrength->value());
    terrain3DControlsLayout->addWidget(new QLabel("Radius", terrain3DControls));
    terrain3DControlsLayout->addWidget(m_brushRadius3D);
    terrain3DControlsLayout->addWidget(new QLabel("Strength", terrain3DControls));
    terrain3DControlsLayout->addWidget(m_brushStrength3D);
    terrain3DControlsLayout->addStretch();
    terrain3DLayout->addWidget(terrain3DControls);
    auto* terrain3DHelp = new QLabel(
        "Navigate: left-drag orbits. Sculpt: left-drag applies the brush and "
        "right-drag orbits. Middle-drag pans and the mouse wheel zooms in all "
        "modes. The active ADT and its enabled neighbors are shown.",
        m_terrain3DPage);
    terrain3DHelp->setWordWrap(true);
    terrain3DLayout->addWidget(terrain3DHelp);
    // Qt's offscreen QPA plugin explicitly does not support QOpenGLWidget and
    // can abort in GLX before initializeGL. Keep headless GUI tests usable and
    // report the limitation in the page; desktop X11/Wayland paths create the
    // actual hardware-accelerated viewport.
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        auto* unavailable = new QLabel(
            "3D View is unavailable with Qt's offscreen platform plugin. "
            "Run Worldwright in an X11 or Wayland desktop session with an "
            "OpenGL 3.3-capable driver.", m_terrain3DPage);
        unavailable->setObjectName("terrain3DUnavailable");
        unavailable->setAlignment(Qt::AlignCenter);
        unavailable->setWordWrap(true);
        terrain3DLayout->addWidget(unavailable, 1);
    } else {
        m_terrain3D = new Terrain3DWidget(&m_model, m_terrain3DPage);
        terrain3DLayout->addWidget(m_terrain3D, 1);
    }
    m_workspaces->addWidget(m_terrain3DPage);
    if (m_terrain3D != nullptr) {
        connect(fit3DAction, &QAction::triggered, m_terrain3D,
                &Terrain3DWidget::fitTerrain);
        connect(reset3DAction, &QAction::triggered, m_terrain3D,
                &Terrain3DWidget::resetCamera);
        connect(refresh3DAction, &QAction::triggered, m_terrain3D,
                &Terrain3DWidget::refreshTerrain);
        connect(wireframeAction, &QAction::toggled, m_terrain3D,
                &Terrain3DWidget::setWireframeVisible);
        connect(boundariesAction, &QAction::toggled, m_terrain3D,
                &Terrain3DWidget::setBoundariesVisible);
        m_terrain3D->terrainChanged = [this] {
            m_terrainDetail->invalidateTerrain();
            refreshTerrainUi();
        };
        m_terrain3D->hoverChanged = [this](
            std::optional<TerrainPickResult> hit) {
            if (!hit.has_value()) {
                m_hoverStatus->setText("Terrain: —");
                return;
            }
            m_hoverStatus->setText(
                "Terrain: " + QString::number(hit->terrain.x, 'f', 2) + ", " +
                QString::number(hit->terrain.y, 'f', 2) + "  Elevation: " +
                QString::number(hit->elevation, 'f', 2));
        };
    } else {
        fit3DAction->setEnabled(false);
        reset3DAction->setEnabled(false);
        refresh3DAction->setEnabled(false);
        wireframeAction->setEnabled(false);
        boundariesAction->setEnabled(false);
    }

    connect(m_brushRadius, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double value) { setSharedBrushRadius(value); });
    connect(m_brushStrength, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double value) { setSharedBrushStrength(value); });
    connect(m_brushRadius3D,
            qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { setSharedBrushRadius(value); });
    connect(m_brushStrength3D,
            qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { setSharedBrushStrength(value); });

    auto* properties = new QWidget(splitter);
    properties->setMinimumWidth(245);
    auto* propertiesLayout = new QVBoxLayout(properties);
    auto* title = new QLabel("Properties", properties);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    propertiesLayout->addWidget(title);
    auto* form = new QFormLayout;
    m_nameEdit = new QLineEdit(properties);
    m_coordinates = new QLabel("None", properties);
    m_selectionCount = new QLabel("0", properties);
    m_enabled = new QCheckBox(properties);
    m_elevation = new QDoubleSpinBox(properties);
    m_elevation->setRange(-1000000.0, 1000000.0);
    m_elevation->setDecimals(3);
    m_elevation->setSingleStep(1.0);
    form->addRow("World name", m_nameEdit);
    form->addRow("Selected tile", m_coordinates);
    form->addRow("Selection size", m_selectionCount);
    form->addRow("Enabled", m_enabled);
    form->addRow("Elevation", m_elevation);
    propertiesLayout->addLayout(form);
    propertiesLayout->addStretch();
    connect(m_nameEdit, &QLineEdit::editingFinished, this,
            [this] { applyWorldName(); });
    connect(m_enabled, &QCheckBox::clicked, this,
            [this](bool checked) {
                if (!m_refreshing) {
                    setSelectionEnabled(checked);
                }
            });
    connect(m_elevation, &QDoubleSpinBox::editingFinished, this,
            [this] { applyElevation(); });

    splitter->addWidget(m_worldTree);
    splitter->addWidget(m_workspaces);
    splitter->addWidget(properties);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);

    m_modeStatus = new QLabel(this);
    m_hoverStatus = new QLabel(this);
    m_countStatus = new QLabel(this);
    m_zoomStatus = new QLabel(this);
    statusBar()->addPermanentWidget(m_modeStatus);
    statusBar()->addPermanentWidget(m_hoverStatus, 1);
    statusBar()->addPermanentWidget(m_countStatus);
    statusBar()->addPermanentWidget(m_zoomStatus);
    updateStatus();
}

void MainWindow::newWorld() {
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, "New World", "World name:", QLineEdit::Normal, "NewWorld",
        &accepted);
    if (!accepted) {
        return;
    }
    std::string error;
    if (!m_model.newWorld(name.toStdString(), &error)) {
        QMessageBox::warning(this, "Invalid world name",
                             QString::fromStdString(error));
        return;
    }
    returnToWorldLayout();
    m_map->fitWorld();
    refreshAll();
}

void MainWindow::exportWorld() {
    const QString selected = QFileDialog::getExistingDirectory(
        this, "Export World", QString(), QFileDialog::ShowDirsOnly);
    if (selected.isEmpty()) {
        return;
    }
    const std::filesystem::path directory(selected.toStdString());
    const auto outputs = WorldExporter::outputPaths(m_model, directory);
    std::size_t existing = 0;
    std::error_code error;
    for (const auto& path : outputs) {
        if (std::filesystem::exists(path, error) && !error) {
            ++existing;
        }
        error.clear();
    }
    bool overwrite = false;
    if (existing > 0) {
        const auto answer = QMessageBox::question(
            this, "Replace existing files",
            QString::number(existing) +
                " output file(s) already exist. Replace them?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
        overwrite = true;
    }

    WorldExporter exporter;
    if (!exporter.exportWorld(m_model, directory, overwrite)) {
        QMessageBox::critical(this, "Export failed",
                              QString::fromStdString(exporter.errorMessage()));
        return;
    }
    QMessageBox::information(
        this, "Export complete",
        "Created " + QString::number(outputs.size()) + " file(s) in:\n" +
            selected);
}

void MainWindow::refreshAll() {
    m_worldTree->clear();
    auto* root = new QTreeWidgetItem(
        m_worldTree, QStringList(QString::fromStdString(m_model.worldName())));
    m_terrainTreeItem = new QTreeWidgetItem(root);
    m_terrainTreeItem->setText(
        0, "Terrain tiles (" + QString::number(m_model.world().tileCount()) +
               " enabled)");
    root->setExpanded(true);
    refreshProperties();
    m_map->update();
    if (m_workspaceMode == WorkspaceMode::Terrain3D && m_terrain3D != nullptr) {
        m_terrain3D->refreshTerrain();
    }
    updateStatus();
}

void MainWindow::refreshTerrainUi() {
    if (m_terrainTreeItem != nullptr) {
        m_terrainTreeItem->setText(
            0, "Terrain tiles (" +
                   QString::number(m_model.world().tileCount()) + " enabled)");
    }
    refreshProperties();
    updateStatus();
}

void MainWindow::updateStatus() {
    if (m_modeStatus == nullptr) {
        return;
    }
    QString mode;
    if (m_workspaceMode == WorkspaceMode::TerrainDetail) {
        mode = "Terrain Detail";
        if (m_terrainDetail != nullptr) {
            if (m_terrainDetail->tool() == TerrainTool::Raise) mode += " / Raise";
            if (m_terrainDetail->tool() == TerrainTool::Lower) mode += " / Lower";
            if (m_terrainDetail->tool() == TerrainTool::Smooth) mode += " / Smooth";
            if (m_terrainDetail->tool() == TerrainTool::Pan) mode += " / Pan";
        }
    } else if (m_workspaceMode == WorkspaceMode::Terrain3D) {
        mode = "3D View";
        if (m_terrain3D != nullptr) {
            if (m_terrain3D->tool() == Terrain3DTool::Navigate)
                mode += " / Navigate";
            if (m_terrain3D->tool() == Terrain3DTool::Raise)
                mode += " / Raise";
            if (m_terrain3D->tool() == Terrain3DTool::Lower)
                mode += " / Lower";
            if (m_terrain3D->tool() == Terrain3DTool::Smooth)
                mode += " / Smooth";
        }
    } else {
        mode = "World Layout / Select";
        if (m_editMode == EditMode::PaintTerrain) mode = "World Layout / Paint Terrain";
        if (m_editMode == EditMode::EraseTerrain) mode = "World Layout / Erase Terrain";
        if (m_editMode == EditMode::Pan) mode = "World Layout / Pan";
    }
    m_modeStatus->setText("Mode: " + mode);
    if (m_workspaceMode == WorkspaceMode::TerrainDetail) {
        if (m_hoverStatus->text().startsWith("Tile:")) {
            m_hoverStatus->setText("Terrain: —");
        }
    } else if (m_workspaceMode == WorkspaceMode::Terrain3D) {
        m_hoverStatus->setText("3D terrain viewport");
    } else if (m_hoveredTile.has_value()) {
        const bool enabled =
            m_model.world().hasTile(m_hoveredTile->x, m_hoveredTile->y);
        m_hoverStatus->setText(
            "Tile: " + QString::number(m_hoveredTile->x) + ", " +
            QString::number(m_hoveredTile->y) +
            (enabled ? " (enabled)" : " (disabled)"));
    } else {
        m_hoverStatus->setText("Tile: —");
    }
    m_countStatus->setText(
        "Enabled: " + QString::number(m_model.world().tileCount()));
    m_zoomStatus->setText(
        "Zoom: " + QString::number(qRound(m_zoomPercent)) + "%");
}

void MainWindow::setEditMode(EditMode mode) {
    m_editMode = mode;
    m_map->setEditMode(mode);
    updateStatus();
}

void MainWindow::refreshProperties() {
    m_refreshing = true;
    m_nameEdit->setText(QString::fromStdString(m_model.worldName()));
    const auto& selection = m_model.selection();
    m_selectionCount->setText(QString::number(selection.size()));
    if (selection.empty()) {
        m_coordinates->setText("None");
        m_enabled->setChecked(false);
        m_enabled->setEnabled(false);
        m_elevation->setEnabled(false);
    } else {
        const auto primary = selection.front();
        QString coordinateText =
            QString::number(primary.x) + ", " + QString::number(primary.y);
        if (selection.size() > 1) {
            const auto last = selection.back();
            coordinateText += " to " + QString::number(last.x) + ", " +
                              QString::number(last.y);
        }
        m_coordinates->setText(coordinateText);
        std::size_t enabledCount = 0;
        for (const auto tile : selection) {
            if (m_model.world().hasTile(tile.x, tile.y)) {
                ++enabledCount;
            }
        }
        m_enabled->setEnabled(true);
        m_enabled->setTristate(enabledCount != 0 && enabledCount != selection.size());
        if (enabledCount == selection.size()) {
            m_enabled->setCheckState(Qt::Checked);
        } else if (enabledCount == 0) {
            m_enabled->setCheckState(Qt::Unchecked);
        } else {
            m_enabled->setCheckState(Qt::PartiallyChecked);
        }
        const auto elevation = m_model.elevationAt(primary.x, primary.y);
        m_elevation->setEnabled(enabledCount > 0);
        if (elevation.has_value()) {
            m_elevation->setValue(*elevation);
        } else {
            for (const auto tile : selection) {
                const auto candidate = m_model.elevationAt(tile.x, tile.y);
                if (candidate.has_value()) {
                    m_elevation->setValue(*candidate);
                    break;
                }
            }
        }
    }
    m_refreshing = false;
}

void MainWindow::applyWorldName() {
    if (m_refreshing) {
        return;
    }
    std::string error;
    if (!m_model.setWorldName(m_nameEdit->text().toStdString(), &error)) {
        QMessageBox::warning(this, "Invalid world name",
                             QString::fromStdString(error));
    }
    refreshAll();
}

void MainWindow::setSelectionEnabled(bool enabled) {
    if (m_model.selection().empty()) {
        return;
    }
    m_model.setSelectedEnabled(enabled);
    refreshAll();
}

void MainWindow::applyElevation() {
    if (m_refreshing || m_model.selection().empty()) {
        return;
    }
    if (!m_model.setSelectedElevation(static_cast<float>(m_elevation->value()))) {
        QMessageBox::warning(this, "Invalid elevation",
                             "Select at least one enabled tile and enter a "
                             "finite elevation.");
    }
    refreshAll();
}

WorkspaceMode MainWindow::workspaceMode() const noexcept {
    return m_workspaceMode;
}

bool MainWindow::enterTerrainDetail() {
    const auto selected = m_model.primarySelection();
    if (!selected.has_value() ||
        !m_model.world().hasTile(selected->x, selected->y)) {
        return false;
    }
    m_workspaceMode = WorkspaceMode::TerrainDetail;
    setTerrainTool(m_terrainDetail->tool());
    refreshActiveTerrainChoices();
    setActiveTerrainTile(*selected);
    m_workspaces->setCurrentWidget(m_detailPage);
    m_layoutToolbar->setVisible(false);
    m_detailToolbar->setVisible(true);
    m_terrain3DToolbar->setVisible(false);
    m_editTerrainAction->setVisible(false);
    m_returnAction->setVisible(true);
    m_hoverStatus->setText("Terrain: —");
    updateStatus();
    return true;
}

void MainWindow::enterTerrain3D() {
    m_workspaceMode = WorkspaceMode::Terrain3D;
    if (m_terrain3D != nullptr) setTerrain3DTool(m_terrain3D->tool());
    m_workspaces->setCurrentWidget(m_terrain3DPage);
    m_layoutToolbar->setVisible(false);
    m_detailToolbar->setVisible(false);
    m_terrain3DToolbar->setVisible(true);
    m_editTerrainAction->setVisible(true);
    m_returnAction->setVisible(true);
    if (m_terrain3D != nullptr) {
        m_terrain3D->refreshTerrain();
        m_terrain3D->fitTerrain();
    }
    m_hoverStatus->setText("3D terrain viewport");
    updateStatus();
}

void MainWindow::returnToWorldLayout() {
    if (m_workspaces == nullptr) {
        return;
    }
    if (m_terrain3D != nullptr) m_terrain3D->cancelInteraction();
    m_workspaceMode = WorkspaceMode::WorldLayout;
    m_workspaces->setCurrentWidget(m_layoutPage);
    m_layoutToolbar->setVisible(true);
    m_detailToolbar->setVisible(false);
    m_terrain3DToolbar->setVisible(false);
    m_editTerrainAction->setVisible(true);
    m_returnAction->setVisible(false);
    m_hoverStatus->setText("Tile: —");
    updateStatus();
}

bool MainWindow::setActiveTerrainTile(TileCoord tile) {
    if (!m_model.world().hasTile(tile.x, tile.y)) {
        return false;
    }
    m_terrainDetail->setActiveTile(tile);
    for (int index = 0; index < m_activeTerrain->count(); ++index) {
        if (m_activeTerrain->itemData(index, Qt::UserRole).toInt() == tile.x &&
            m_activeTerrain->itemData(index, Qt::UserRole + 1).toInt() == tile.y) {
            const bool oldRefreshing = m_refreshing;
            m_refreshing = true;
            m_activeTerrain->setCurrentIndex(index);
            m_refreshing = oldRefreshing;
            break;
        }
    }
    updateStatus();
    return true;
}

WorldEditorModel& MainWindow::model() noexcept { return m_model; }
const WorldEditorModel& MainWindow::model() const noexcept { return m_model; }

void MainWindow::refreshActiveTerrainChoices() {
    const auto current = m_terrainDetail->activeTile();
    const bool oldRefreshing = m_refreshing;
    m_refreshing = true;
    m_activeTerrain->clear();
    for (const auto tile : m_model.world().enabledTiles()) {
        m_activeTerrain->addItem(QString::number(tile.x) + ", " +
                                     QString::number(tile.y),
                                 tile.x);
        const int index = m_activeTerrain->count() - 1;
        m_activeTerrain->setItemData(index, tile.y, Qt::UserRole + 1);
        if (current.has_value() && *current == tile) {
            m_activeTerrain->setCurrentIndex(index);
        }
    }
    m_refreshing = oldRefreshing;
}

void MainWindow::setTerrainTool(TerrainTool tool) {
    const double maximum = tool == TerrainTool::Smooth ? 1.0 : 100.0;
    m_brushStrength3D->setMaximum(maximum);
    if (tool == TerrainTool::Smooth) {
        m_brushStrength->setMaximum(1.0);
        if (m_brushStrength->value() > 1.0) {
            m_brushStrength->setValue(1.0);
        }
    } else {
        m_brushStrength->setMaximum(100.0);
    }
    m_terrainDetail->setTool(tool);
    m_terrainDetail->setBrushStrength(m_brushStrength->value());
    updateStatus();
}

void MainWindow::setTerrain3DTool(Terrain3DTool tool) {
    const double maximum = tool == Terrain3DTool::Smooth ? 1.0 : 100.0;
    m_brushStrength->setMaximum(maximum);
    m_brushStrength3D->setMaximum(maximum);
    if (m_brushStrength3D->value() > maximum)
        setSharedBrushStrength(maximum);
    if (m_terrain3D != nullptr) {
        m_terrain3D->setTool(tool);
        m_terrain3D->setBrushRadius(m_brushRadius3D->value());
        m_terrain3D->setBrushStrength(m_brushStrength3D->value());
    }
    updateStatus();
}

void MainWindow::setSharedBrushRadius(double radius) {
    if (m_brushRadius == nullptr || m_brushRadius3D == nullptr) return;
    const QSignalBlocker detailBlocker(m_brushRadius);
    const QSignalBlocker threeDBlocker(m_brushRadius3D);
    m_brushRadius->setValue(radius);
    m_brushRadius3D->setValue(radius);
    m_terrainDetail->setBrushRadius(radius);
    if (m_terrain3D != nullptr) m_terrain3D->setBrushRadius(radius);
}

void MainWindow::setSharedBrushStrength(double strength) {
    if (m_brushStrength == nullptr || m_brushStrength3D == nullptr) return;
    strength = std::min({strength, m_brushStrength->maximum(),
                         m_brushStrength3D->maximum()});
    const QSignalBlocker detailBlocker(m_brushStrength);
    const QSignalBlocker threeDBlocker(m_brushStrength3D);
    m_brushStrength->setValue(strength);
    m_brushStrength3D->setValue(strength);
    m_terrainDetail->setBrushStrength(strength);
    if (m_terrain3D != nullptr) m_terrain3D->setBrushStrength(strength);
}

}  // namespace worldwright::gui
