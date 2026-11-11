#include "Terrain3DWidget.h"

#include "worldwright/WorldEditorModel.h"
#include "worldwright/TerrainHeightEditor.h"
#include "worldwright/WorldDefinition.h"

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QSurfaceFormat>
#include <QWheelEvent>
#include <QFocusEvent>
#include <QHideEvent>

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace worldwright::gui {
namespace {

constexpr char vertexShader[] = R"(
#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
uniform mat4 mvp;
uniform mat3 normalMatrix;
out vec3 surfaceNormal;
out float elevation;
void main() {
    gl_Position = mvp * vec4(position, 1.0);
    surfaceNormal = normalize(normalMatrix * normal);
    elevation = position.y;
}
)";

constexpr char fragmentShader[] = R"(
#version 330 core
in vec3 surfaceNormal;
in float elevation;
uniform vec3 baseColor;
uniform vec3 lightDirection;
uniform bool unlit;
out vec4 color;
void main() {
    float diffuse = max(dot(normalize(surfaceNormal),
                            normalize(lightDirection)), 0.0);
    float lighting = unlit ? 1.0 : 0.30 + 0.70 * diffuse;
    float elevationTint = unlit ? 1.0 : clamp(0.92 + elevation * 0.00018,
                                               0.75, 1.15);
    color = vec4(baseColor * lighting * elevationTint, 1.0);
}
)";

}  // namespace

Terrain3DWidget::Terrain3DWidget(WorldEditorModel* model, QWidget* parent)
    : QOpenGLWidget(parent), m_model(model) {
    QSurfaceFormat requested;
    requested.setVersion(3, 3);
    requested.setProfile(QSurfaceFormat::CoreProfile);
    requested.setDepthBufferSize(24);
    requested.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    setFormat(requested);
    setObjectName("terrain3DViewport");
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(320, 240);
    setToolTip("Left-drag: orbit; middle-drag: pan; wheel: zoom");
}

void Terrain3DWidget::setTool(Terrain3DTool tool) {
    cancelInteraction();
    m_tool = tool;
    setCursor(tool == Terrain3DTool::Navigate ? Qt::ArrowCursor
                                              : Qt::CrossCursor);
    setToolTip(tool == Terrain3DTool::Navigate
                   ? "Left-drag: orbit; middle-drag: pan; wheel: zoom"
                   : "Left-drag: sculpt; right-drag: orbit; middle-drag: pan; "
                     "wheel: zoom");
    rebuildCursor();
    update();
}

void Terrain3DWidget::setBrushRadius(double radius) {
    if (TerrainDetailInteraction::validBrushParameters(
            radius, m_strength, m_tool == Terrain3DTool::Smooth)) {
        m_radius = radius;
        rebuildCursor();
        update();
    }
}

void Terrain3DWidget::setBrushStrength(double strength) {
    if (TerrainDetailInteraction::validBrushParameters(
            m_radius, strength, m_tool == Terrain3DTool::Smooth)) {
        m_strength = strength;
    }
}

void Terrain3DWidget::cancelInteraction() {
    m_sculpting = false;
    m_sampler.reset();
    m_dragButtons = Qt::NoButton;
    releaseMouse();
}

Terrain3DWidget::~Terrain3DWidget() {
    if (context() != nullptr && context()->isValid()) {
        makeCurrent();
        releaseGpuResources();
        doneCurrent();
    }
}

void Terrain3DWidget::refreshTerrain() {
    if (m_model == nullptr) return;
    m_mesh = TerrainMeshBuilder::build(*m_model, m_model->primarySelection());
    if (m_initialized) {
        makeCurrent();
        uploadMesh();
        doneCurrent();
    }
    rebuildCursor();
    update();
}

void Terrain3DWidget::fitTerrain() {
    m_camera.fit(m_mesh.bounds,
                 height() > 0 ? static_cast<double>(width()) / height() : 1.0);
    update();
}

void Terrain3DWidget::resetCamera() {
    m_camera.reset();
    update();
}

void Terrain3DWidget::setWireframeVisible(bool visible) {
    m_wireframeVisible = visible;
    update();
}

void Terrain3DWidget::setBoundariesVisible(bool visible) {
    m_boundariesVisible = visible;
    update();
}

void Terrain3DWidget::initializeGL() {
    if (context() == nullptr || !context()->isValid()) {
        reportGraphicsError("No valid OpenGL context is available.");
        return;
    }
    const QSurfaceFormat format = context()->format();
    if (format.majorVersion() < 3 ||
        (format.majorVersion() == 3 && format.minorVersion() < 3)) {
        reportGraphicsError(
            QString("OpenGL 3.3 or newer is required; this system provided %1.%2.")
                .arg(format.majorVersion()).arg(format.minorVersion()));
        return;
    }
    if (!initializeOpenGLFunctions()) {
        reportGraphicsError("OpenGL 3.3 functions could not be initialized.");
        return;
    }
    m_program = std::make_unique<QOpenGLShaderProgram>();
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                             vertexShader) ||
        !m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                             fragmentShader) ||
        !m_program->link()) {
        reportGraphicsError("Terrain shaders could not be initialized: " +
                            m_program->log());
        m_program.reset();
        return;
    }
    glEnable(GL_DEPTH_TEST);
    m_initialized = true;
    uploadMesh();
}

void Terrain3DWidget::resizeGL(int, int) {}

void Terrain3DWidget::paintGL() {
    if (!m_initialized || m_program == nullptr) {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(26, 33, 41));
        if (!m_graphicsError.isEmpty()) {
            painter.setPen(Qt::white);
            painter.drawText(rect().adjusted(24, 24, -24, -24),
                             Qt::AlignCenter | Qt::TextWordWrap,
                             "3D View unavailable\n" + m_graphicsError);
        }
        return;
    }
    glClearColor(0.10f, 0.13f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (m_mesh.empty()) return;

    const auto eye = m_camera.position();
    const auto target = m_camera.target();
    QMatrix4x4 projection;
    projection.perspective(45.0f,
                           height() > 0 ? static_cast<float>(width()) / height()
                                        : 1.0f,
                           static_cast<float>(m_camera.nearPlane()),
                           static_cast<float>(m_camera.farPlane()));
    QMatrix4x4 view;
    view.lookAt(QVector3D(eye.x, eye.y, eye.z),
                QVector3D(target.x, target.y, target.z),
                QVector3D(0.0f, 1.0f, 0.0f));
    m_program->bind();
    m_program->setUniformValue("mvp", projection * view);
    m_program->setUniformValue("normalMatrix", view.normalMatrix());
    m_program->setUniformValue("lightDirection",
                               QVector3D(-0.35f, 0.85f, -0.40f));
    m_program->setUniformValue("unlit", false);
    m_program->setUniformValue("baseColor", QVector3D(0.44f, 0.60f, 0.39f));
    glBindVertexArray(m_terrainVao);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_mesh.indices.size()),
                   GL_UNSIGNED_INT, nullptr);
    if (m_wireframeVisible) {
        // Pull the line pass slightly toward the camera so it remains a crisp
        // overlay instead of competing with the shaded triangles for depth.
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        m_program->setUniformValue("unlit", true);
        m_program->setUniformValue("baseColor", QVector3D(0.55f, 0.90f, 1.00f));
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(m_mesh.indices.size()),
                       GL_UNSIGNED_INT, nullptr);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
    }
    if (m_boundariesVisible && !m_mesh.boundaryVertices.empty()) {
        m_program->setUniformValue("unlit", true);
        m_program->setUniformValue("baseColor", QVector3D(0.95f, 0.56f, 0.12f));
        glBindVertexArray(m_boundaryVao);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0,
                     static_cast<GLsizei>(m_mesh.boundaryVertices.size()));
        glLineWidth(1.0f);
    }
    if (m_tool != Terrain3DTool::Navigate &&
        !m_cursorVertices.empty()) {
        m_program->setUniformValue("unlit", true);
        m_program->setUniformValue("baseColor", QVector3D(1.00f, 0.20f, 0.72f));
        glBindVertexArray(m_cursorVao);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0,
                     static_cast<GLsizei>(m_cursorVertices.size()));
        glLineWidth(1.0f);
    }
    glBindVertexArray(0);
    m_program->release();
}

void Terrain3DWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    updateHover(event->position());
    if (event->button() == Qt::LeftButton &&
        m_tool != Terrain3DTool::Navigate) {
        if (m_hoverPick.has_value() && editableAt(m_hoverPick->terrain)) {
            m_sculpting = true;
            grabMouse();
            applySamples(m_sampler.begin(m_hoverPick->terrain, m_radius * 0.25));
        }
        event->accept();
        return;
    }
    const bool orbitButton = event->button() == Qt::RightButton ||
        (event->button() == Qt::LeftButton &&
         m_tool == Terrain3DTool::Navigate);
    if (orbitButton || event->button() == Qt::MiddleButton) {
        m_dragButtons |= event->button();
        m_lastMouse = event->position().toPoint();
        event->accept();
        return;
    }
    QOpenGLWidget::mousePressEvent(event);
}

void Terrain3DWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPoint current = event->position().toPoint();
    const QPoint delta = current - m_lastMouse;
    m_lastMouse = current;
    const bool orbiting = m_dragButtons.testFlag(Qt::RightButton) ||
        (m_tool == Terrain3DTool::Navigate &&
         m_dragButtons.testFlag(Qt::LeftButton));
    if (orbiting) {
        m_camera.orbit(delta.x() * 0.35, delta.y() * 0.30);
        update();
        event->accept();
    } else if (m_dragButtons.testFlag(Qt::MiddleButton)) {
        const double scale = m_camera.distance() /
                             std::max(100, std::min(width(), height()));
        m_camera.pan(-delta.x() * scale, delta.y() * scale);
        update();
        event->accept();
    } else {
        updateHover(event->position());
        if (m_sculpting && (event->buttons() & Qt::LeftButton) &&
            m_hoverPick.has_value()) {
            applySamples(m_sampler.append(m_hoverPick->terrain));
        } else if (m_sculpting && !(event->buttons() & Qt::LeftButton)) {
            cancelInteraction();
        }
        event->accept();
    }
}

void Terrain3DWidget::mouseReleaseEvent(QMouseEvent* event) {
    m_dragButtons &= ~event->button();
    if (event->button() == Qt::LeftButton && m_sculpting) {
        m_sculpting = false;
        m_sampler.reset();
        releaseMouse();
    }
    event->accept();
}

void Terrain3DWidget::leaveEvent(QEvent* event) {
    cancelInteraction();
    m_hoverPick.reset();
    rebuildCursor();
    if (hoverChanged) hoverChanged(std::nullopt);
    QOpenGLWidget::leaveEvent(event);
}

void Terrain3DWidget::focusOutEvent(QFocusEvent* event) {
    cancelInteraction();
    QOpenGLWidget::focusOutEvent(event);
}

void Terrain3DWidget::hideEvent(QHideEvent* event) {
    cancelInteraction();
    QOpenGLWidget::hideEvent(event);
}

void Terrain3DWidget::updateHover(const QPointF& position) {
    if (width() <= 0 || height() <= 0 || m_mesh.empty()) {
        m_hoverPick.reset();
    } else {
        const double normalizedX = position.x() * 2.0 / width() - 1.0;
        const double normalizedY = 1.0 - position.y() * 2.0 / height();
        const auto ray = TerrainPicking::screenRay(
            m_camera, normalizedX, normalizedY,
            static_cast<double>(width()) / height());
        m_hoverPick = TerrainPicking::intersect(m_mesh, ray);
        if (m_hoverPick.has_value() && !editableAt(m_hoverPick->terrain)) {
            m_hoverPick.reset();
        }
    }
    rebuildCursor();
    if (hoverChanged) hoverChanged(m_hoverPick);
    update();
}

void Terrain3DWidget::rebuildCursor() {
    m_cursorVertices.clear();
    if (m_tool != Terrain3DTool::Navigate && m_hoverPick.has_value() &&
        m_radius > 0.0) {
        constexpr int segments = 64;
        constexpr double pi = 3.14159265358979323846;
        std::optional<TerrainMeshVertex> first;
        std::optional<TerrainMeshVertex> previous;
        for (int i = 0; i <= segments; ++i) {
            const double angle = 2.0 * pi * (i % segments) / segments;
            const TerrainPoint point{
                m_hoverPick->terrain.x + std::cos(angle) * m_radius,
                m_hoverPick->terrain.y + std::sin(angle) * m_radius};
            const auto elevation = TerrainPicking::surfaceElevation(m_mesh, point);
            std::optional<TerrainMeshVertex> current;
            if (elevation.has_value()) {
                current = TerrainMeshVertex{static_cast<float>(point.x),
                    static_cast<float>(*elevation + 0.15),
                    static_cast<float>(point.y), 0.0f, 1.0f, 0.0f};
            }
            if (i == 0) first = current;
            if (previous.has_value() && current.has_value()) {
                m_cursorVertices.push_back(*previous);
                m_cursorVertices.push_back(*current);
            }
            previous = current;
        }
    }
    if (m_initialized) {
        makeCurrent();
        uploadCursor();
        doneCurrent();
    }
}

bool Terrain3DWidget::editableAt(TerrainPoint point) const noexcept {
    return m_model != nullptr && TerrainHeightEditor::editableAt(*m_model, point);
}

void Terrain3DWidget::applySamples(
    const std::vector<TerrainPoint>& samples) {
    bool changed = false;
    for (const auto sample : samples) changed = applyBrush(sample) || changed;
    if (!changed) return;
    // Rebuild the bounded rendered neighborhood from the authoritative model;
    // this refreshes both positions and geometry-derived normals.
    refreshTerrain();
    if (terrainChanged) terrainChanged();
}

bool Terrain3DWidget::applyBrush(TerrainPoint center) {
    if (!editableAt(center) ||
        !TerrainDetailInteraction::validBrushParameters(
            m_radius, m_strength, m_tool == Terrain3DTool::Smooth)) return false;
    const TerrainBrush brush{center, m_radius, m_strength};
    if (m_tool == Terrain3DTool::Raise)
        return TerrainHeightEditor::raise(*m_model, brush);
    if (m_tool == Terrain3DTool::Lower)
        return TerrainHeightEditor::lower(*m_model, brush);
    if (m_tool == Terrain3DTool::Smooth)
        return TerrainHeightEditor::smooth(*m_model, brush);
    return false;
}

void Terrain3DWidget::wheelEvent(QWheelEvent* event) {
    m_camera.zoom(event->angleDelta().y() / 120.0);
    update();
    event->accept();
}

void Terrain3DWidget::uploadMesh() {
    releaseGpuResources();
    if (m_mesh.vertices.empty()) return;
    glGenVertexArrays(1, &m_terrainVao);
    glGenBuffers(1, &m_terrainVbo);
    glGenBuffers(1, &m_terrainIbo);
    glBindVertexArray(m_terrainVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_terrainVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_mesh.vertices.size() *
                                        sizeof(TerrainMeshVertex)),
                 m_mesh.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_terrainIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_mesh.indices.size() *
                                        sizeof(std::uint32_t)),
                 m_mesh.indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainMeshVertex),
                          reinterpret_cast<void*>(offsetof(TerrainMeshVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainMeshVertex),
                          reinterpret_cast<void*>(offsetof(TerrainMeshVertex, nx)));
    glBindVertexArray(0);

    if (!m_mesh.boundaryVertices.empty()) {
        glGenVertexArrays(1, &m_boundaryVao);
        glGenBuffers(1, &m_boundaryVbo);
        glBindVertexArray(m_boundaryVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_boundaryVbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(m_mesh.boundaryVertices.size() *
                                            sizeof(TerrainMeshVertex)),
                     m_mesh.boundaryVertices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainMeshVertex),
            reinterpret_cast<void*>(offsetof(TerrainMeshVertex, x)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainMeshVertex),
            reinterpret_cast<void*>(offsetof(TerrainMeshVertex, nx)));
        glBindVertexArray(0);
    }
    uploadCursor();
}

void Terrain3DWidget::uploadCursor() {
    if (m_cursorVbo) glDeleteBuffers(1, &m_cursorVbo);
    if (m_cursorVao) glDeleteVertexArrays(1, &m_cursorVao);
    m_cursorVbo = m_cursorVao = 0;
    if (m_cursorVertices.empty()) return;
    glGenVertexArrays(1, &m_cursorVao);
    glGenBuffers(1, &m_cursorVbo);
    glBindVertexArray(m_cursorVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_cursorVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_cursorVertices.size() *
                                        sizeof(TerrainMeshVertex)),
                 m_cursorVertices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainMeshVertex),
                          reinterpret_cast<void*>(offsetof(TerrainMeshVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainMeshVertex),
                          reinterpret_cast<void*>(offsetof(TerrainMeshVertex, nx)));
    glBindVertexArray(0);
}

void Terrain3DWidget::releaseGpuResources() {
    if (m_cursorVbo) glDeleteBuffers(1, &m_cursorVbo);
    if (m_cursorVao) glDeleteVertexArrays(1, &m_cursorVao);
    if (m_boundaryVbo) glDeleteBuffers(1, &m_boundaryVbo);
    if (m_boundaryVao) glDeleteVertexArrays(1, &m_boundaryVao);
    if (m_terrainIbo) glDeleteBuffers(1, &m_terrainIbo);
    if (m_terrainVbo) glDeleteBuffers(1, &m_terrainVbo);
    if (m_terrainVao) glDeleteVertexArrays(1, &m_terrainVao);
    m_boundaryVbo = m_boundaryVao = 0;
    m_cursorVbo = m_cursorVao = 0;
    m_terrainIbo = m_terrainVbo = m_terrainVao = 0;
}

void Terrain3DWidget::reportGraphicsError(const QString& message) {
    m_graphicsError = message;
    qWarning("Worldwright 3D View: %s", qPrintable(message));
    update();
}

}  // namespace worldwright::gui
