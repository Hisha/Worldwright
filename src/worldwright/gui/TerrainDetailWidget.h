#ifndef WORLDWRIGHT_GUI_TERRAIN_DETAIL_WIDGET_H
#define WORLDWRIGHT_GUI_TERRAIN_DETAIL_WIDGET_H

#include "worldwright/TerrainDetailInteraction.h"
#include "worldwright/WorldEditorModel.h"

#include <QImage>
#include <QWidget>

#include <functional>
#include <optional>
#include <unordered_map>

class QEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

namespace worldwright::gui {

enum class TerrainTool { Raise, Lower, Smooth, Pan };

class TerrainDetailWidget final : public QWidget {
public:
    explicit TerrainDetailWidget(WorldEditorModel* model,
                                 QWidget* parent = nullptr);

    std::function<void()> terrainChanged;
    std::function<void(std::optional<TerrainPoint>, bool)> hoverChanged;
    std::function<void(double)> zoomChanged;

    void setActiveTile(TileCoord tile);
    std::optional<TileCoord> activeTile() const noexcept;
    void setTool(TerrainTool tool);
    TerrainTool tool() const noexcept;
    void setBrushRadius(double radius);
    void setBrushStrength(double strength);
    double brushRadius() const noexcept;
    double brushStrength() const noexcept;
    double zoomPercent() const noexcept;
    void fitTerrain();
    void invalidateTerrain();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    TerrainPoint worldAt(const QPointF& point) const noexcept;
    QPointF screenAt(TerrainPoint point) const noexcept;
    bool editableAt(TerrainPoint point) const noexcept;
    void applySamples(const std::vector<TerrainPoint>& samples);
    bool applyBrush(TerrainPoint center);
    void invalidateBrush(TerrainPoint center);
    void expandHeightRangeForTile(int tileX, int tileY);
    const QImage& tileImage(int tileX, int tileY);
    QImage renderTile(const TerrainDefinition& terrain) const;
    QColor heightColor(double height) const;
    void updateCursor();
    void notifyZoom();

    WorldEditorModel* m_model = nullptr;
    std::optional<TileCoord> m_active;
    TerrainDetailView m_view;
    TerrainTool m_tool = TerrainTool::Raise;
    TerrainStrokeSampler m_sampler;
    double m_radius = TerrainHeightEditor::CellSize * 4.0;
    double m_strength = 1.0;
    double m_minHeight = -10.0;
    double m_maxHeight = 10.0;
    bool m_autoFit = true;
    bool m_panning = false;
    bool m_sculpting = false;
    std::optional<TerrainPoint> m_hover;
    QPointF m_lastMouse;
    std::unordered_map<std::size_t, QImage> m_tileImages;
};

}  // namespace worldwright::gui

#endif
