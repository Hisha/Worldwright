#ifndef WORLDWRIGHT_GUI_WORLD_MAP_WIDGET_H
#define WORLDWRIGHT_GUI_WORLD_MAP_WIDGET_H

#include "worldwright/EditorInteraction.h"
#include "worldwright/WorldEditorModel.h"

#include <QPointF>
#include <QWidget>

#include <functional>
#include <optional>

class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;
class QEvent;

namespace worldwright::gui {

class WorldMapWidget final : public QWidget {
public:
    explicit WorldMapWidget(WorldEditorModel* model, QWidget* parent = nullptr);

    std::function<void()> selectionChanged;
    std::function<void()> terrainChanged;
    std::function<void(std::optional<TileCoord>)> hoverChanged;
    std::function<void(double)> zoomChanged;

    void resetView();
    void fitWorld();
    void setEditMode(EditMode mode);
    EditMode editMode() const noexcept;
    double zoomPercent() const noexcept;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    std::optional<TileCoord> tileAt(const QPointF& point) const;
    TileCoord unboundedTileAt(const QPointF& point) const;
    QPointF gridOrigin() const;
    void applyStroke(const TileCoord& from, const TileCoord& to);
    void updateCursor();
    void notifyZoom();

    WorldEditorModel* m_model = nullptr;
    MapView m_view;
    EditMode m_mode = EditMode::Select;
    bool m_autoFit = true;
    bool m_selecting = false;
    bool m_painting = false;
    bool m_panning = false;
    TileCoord m_dragStart{};
    TileCoord m_lastPaintTile{};
    std::optional<TileCoord> m_hoveredTile;
    QPointF m_lastMouse;
};

}  // namespace worldwright::gui

#endif
