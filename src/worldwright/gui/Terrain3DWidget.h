#ifndef WORLDWRIGHT_GUI_TERRAIN_3D_WIDGET_H
#define WORLDWRIGHT_GUI_TERRAIN_3D_WIDGET_H

#include "worldwright/TerrainCamera.h"
#include "worldwright/TerrainDetailInteraction.h"
#include "worldwright/TerrainMesh.h"
#include "worldwright/TerrainPicking.h"

#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QPoint>

#include <memory>
#include <functional>
#include <optional>
#include <vector>

class QOpenGLShaderProgram;

namespace worldwright {
class WorldEditorModel;
}

namespace worldwright::gui {

enum class Terrain3DTool { Navigate, Raise, Lower, Smooth };

class Terrain3DWidget final : public QOpenGLWidget,
                              protected QOpenGLFunctions_3_3_Core {
public:
    explicit Terrain3DWidget(WorldEditorModel* model,
                             QWidget* parent = nullptr);
    ~Terrain3DWidget() override;

    void refreshTerrain();
    void fitTerrain();
    void resetCamera();
    void setWireframeVisible(bool visible);
    void setBoundariesVisible(bool visible);
    bool hasGraphicsError() const noexcept { return !m_graphicsError.isEmpty(); }
    QString graphicsError() const { return m_graphicsError; }
    const TerrainMesh& cpuMesh() const noexcept { return m_mesh; }
    Terrain3DTool tool() const noexcept { return m_tool; }
    double brushRadius() const noexcept { return m_radius; }
    double brushStrength() const noexcept { return m_strength; }
    std::optional<TerrainPickResult> hoverPick() const noexcept {
        return m_hoverPick;
    }
    bool sculpting() const noexcept { return m_sculpting; }

    std::function<void()> terrainChanged;
    std::function<void(std::optional<TerrainPickResult>)> hoverChanged;

    void setTool(Terrain3DTool tool);
    void setBrushRadius(double radius);
    void setBrushStrength(double strength);
    void cancelInteraction();

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void uploadMesh();
    void uploadCursor();
    void updateHover(const QPointF& position);
    void rebuildCursor();
    void applySamples(const std::vector<TerrainPoint>& samples);
    bool applyBrush(TerrainPoint center);
    bool editableAt(TerrainPoint point) const noexcept;
    void releaseGpuResources();
    void reportGraphicsError(const QString& message);

    WorldEditorModel* m_model = nullptr;
    TerrainMesh m_mesh;
    TerrainCamera m_camera;
    std::unique_ptr<QOpenGLShaderProgram> m_program;
    unsigned int m_terrainVao = 0;
    unsigned int m_terrainVbo = 0;
    unsigned int m_terrainIbo = 0;
    unsigned int m_boundaryVao = 0;
    unsigned int m_boundaryVbo = 0;
    unsigned int m_cursorVao = 0;
    unsigned int m_cursorVbo = 0;
    std::vector<TerrainMeshVertex> m_cursorVertices;
    QPoint m_lastMouse;
    Qt::MouseButtons m_dragButtons = Qt::NoButton;
    QString m_graphicsError;
    bool m_initialized = false;
    bool m_wireframeVisible = false;
    bool m_boundariesVisible = true;
    Terrain3DTool m_tool = Terrain3DTool::Navigate;
    TerrainStrokeSampler m_sampler;
    double m_radius = TerrainHeightEditor::CellSize * 4.0;
    double m_strength = 1.0;
    bool m_sculpting = false;
    std::optional<TerrainPickResult> m_hoverPick;
};

}  // namespace worldwright::gui

#endif
