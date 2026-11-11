#include "TerrainDetailWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace worldwright::gui {
namespace {

constexpr int kImageSide =
    TerrainDefinition::InnerVerticesPerTileSide * 2 + 1;

std::size_t tileKey(int x, int y) noexcept {
    return static_cast<std::size_t>(y) * WorldDefinition::GridWidth +
           static_cast<std::size_t>(x);
}

QColor interpolate(const QColor& a, const QColor& b, double amount) {
    amount = std::clamp(amount, 0.0, 1.0);
    return {qRound(a.red() + (b.red() - a.red()) * amount),
            qRound(a.green() + (b.green() - a.green()) * amount),
            qRound(a.blue() + (b.blue() - a.blue()) * amount)};
}

}  // namespace

TerrainDetailWidget::TerrainDetailWidget(WorldEditorModel* model,
                                         QWidget* parent)
    : QWidget(parent), m_model(model) {
    setObjectName("terrainDetailCanvas");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 320);
    updateCursor();
}

void TerrainDetailWidget::setActiveTile(TileCoord tile) {
    if (!WorldDefinition::isValidTile(tile.x, tile.y) ||
        !m_model->world().hasTile(tile.x, tile.y)) {
        return;
    }
    m_active = tile;
    m_minHeight = std::numeric_limits<double>::infinity();
    m_maxHeight = -std::numeric_limits<double>::infinity();
    for (int y = std::max(0, tile.y - 1);
         y <= std::min(WorldDefinition::GridHeight - 1, tile.y + 1); ++y) {
        for (int x = std::max(0, tile.x - 1);
             x <= std::min(WorldDefinition::GridWidth - 1, tile.x + 1); ++x) {
            expandHeightRangeForTile(x, y);
        }
    }
    if (!std::isfinite(m_minHeight) || !std::isfinite(m_maxHeight)) {
        m_minHeight = -10.0;
        m_maxHeight = 10.0;
    } else if (std::abs(m_maxHeight - m_minHeight) < 1e-6) {
        const double flatHeight = m_minHeight;
        m_minHeight = flatHeight - 10.0;
        m_maxHeight = flatHeight + 10.0;
    }
    m_tileImages.clear();
    fitTerrain();
}

std::optional<TileCoord> TerrainDetailWidget::activeTile() const noexcept {
    return m_active;
}

void TerrainDetailWidget::setTool(TerrainTool tool) {
    m_tool = tool;
    m_sculpting = false;
    m_sampler.reset();
    updateCursor();
    update();
}

TerrainTool TerrainDetailWidget::tool() const noexcept { return m_tool; }

void TerrainDetailWidget::setBrushRadius(double radius) {
    if (TerrainDetailInteraction::validBrushParameters(radius, m_strength,
                                                        m_tool == TerrainTool::Smooth)) {
        m_radius = radius;
        update();
    }
}

void TerrainDetailWidget::setBrushStrength(double strength) {
    if (TerrainDetailInteraction::validBrushParameters(m_radius, strength,
                                                        m_tool == TerrainTool::Smooth)) {
        m_strength = strength;
    }
}

double TerrainDetailWidget::brushRadius() const noexcept { return m_radius; }
double TerrainDetailWidget::brushStrength() const noexcept { return m_strength; }

double TerrainDetailWidget::zoomPercent() const noexcept {
    return m_view.pixelsPerUnit * TerrainHeightEditor::TileSize / 5.0 * 100.0;
}

void TerrainDetailWidget::fitTerrain() {
    if (!m_active.has_value()) {
        return;
    }
    m_autoFit = true;
    m_view = TerrainDetailInteraction::fitActive(*m_active, width(), height());
    update();
    notifyZoom();
}

void TerrainDetailWidget::invalidateTerrain() {
    m_tileImages.clear();
    update();
}

TerrainPoint TerrainDetailWidget::worldAt(const QPointF& point) const noexcept {
    return TerrainDetailInteraction::screenToWorld(
        m_view, {point.x(), point.y()});
}

QPointF TerrainDetailWidget::screenAt(TerrainPoint point) const noexcept {
    const auto screen = TerrainDetailInteraction::worldToScreen(m_view, point);
    return {screen.x, screen.y};
}

bool TerrainDetailWidget::editableAt(TerrainPoint point) const noexcept {
    return m_model != nullptr && TerrainHeightEditor::editableAt(*m_model, point);
}

void TerrainDetailWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(24, 27, 32));
    if (!m_active.has_value()) {
        painter.setPen(QColor(210, 214, 220));
        painter.drawText(rect(), Qt::AlignCenter, "No active terrain tile");
        return;
    }

    const auto topLeft = worldAt({0.0, 0.0});
    const auto bottomRight = worldAt({static_cast<double>(width()),
                                      static_cast<double>(height())});
    const int firstX = std::max(0, static_cast<int>(std::floor(
                                      topLeft.x / TerrainHeightEditor::TileSize)));
    const int firstY = std::max(0, static_cast<int>(std::floor(
                                      topLeft.y / TerrainHeightEditor::TileSize)));
    const int lastX = std::min(WorldDefinition::GridWidth - 1,
                               static_cast<int>(std::floor(
                                   bottomRight.x / TerrainHeightEditor::TileSize)));
    const int lastY = std::min(WorldDefinition::GridHeight - 1,
                               static_cast<int>(std::floor(
                                   bottomRight.y / TerrainHeightEditor::TileSize)));
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (int y = firstY; y <= lastY; ++y) {
        for (int x = firstX; x <= lastX; ++x) {
            const QPointF screen = screenAt({x * TerrainHeightEditor::TileSize,
                                             y * TerrainHeightEditor::TileSize});
            const double side = TerrainHeightEditor::TileSize *
                                m_view.pixelsPerUnit;
            const QRectF tileRect(screen.x(), screen.y(), side, side);
            if (m_model->world().hasTile(x, y)) {
                painter.drawImage(tileRect, tileImage(x, y));
            } else {
                painter.fillRect(tileRect, ((x + y) & 1) ? QColor(42, 46, 53)
                                                         : QColor(37, 41, 47));
            }
            painter.setPen(QPen(QColor(15, 17, 20, 180), 1.0));
            painter.drawRect(tileRect);
        }
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    const auto activeScreen = screenAt(
        {m_active->x * TerrainHeightEditor::TileSize,
         m_active->y * TerrainHeightEditor::TileSize});
    const double activeSide = TerrainHeightEditor::TileSize * m_view.pixelsPerUnit;
    painter.setPen(QPen(QColor(255, 196, 55), 3.0));
    painter.drawRect(QRectF(activeScreen.x(), activeScreen.y(), activeSide,
                            activeSide).adjusted(1.5, 1.5, -1.5, -1.5));
    if (activeSide >= 70.0) {
        painter.setPen(QColor(255, 236, 180));
        painter.drawText(QRectF(activeScreen.x() + 8, activeScreen.y() + 6,
                                activeSide - 16, 24),
                         "Active ADT " + QString::number(m_active->x) + "," +
                             QString::number(m_active->y));
    }

    if (m_hover.has_value() && m_tool != TerrainTool::Pan &&
        editableAt(*m_hover)) {
        const QPointF center = screenAt(*m_hover);
        const double radius = TerrainDetailInteraction::brushRadiusPixels(
            m_view, m_radius);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(0, 0, 0, 210), 4.0));
        painter.drawEllipse(center, radius, radius);
        painter.setPen(QPen(QColor(255, 255, 255, 235), 1.5, Qt::DashLine));
        painter.drawEllipse(center, radius, radius);
    }

    const QRect legendRect(width() - 176, height() - 48, 152, 14);
    QLinearGradient gradient(legendRect.topLeft(), legendRect.topRight());
    gradient.setColorAt(0.0, heightColor(m_minHeight));
    gradient.setColorAt(0.35, heightColor(m_minHeight +
                                          (m_maxHeight - m_minHeight) * 0.35));
    gradient.setColorAt(0.7, heightColor(m_minHeight +
                                         (m_maxHeight - m_minHeight) * 0.7));
    gradient.setColorAt(1.0, heightColor(m_maxHeight));
    painter.fillRect(legendRect, gradient);
    painter.setPen(QColor(238, 240, 244));
    painter.drawRect(legendRect);
    painter.drawText(legendRect.left(), legendRect.top() - 5,
                     QString::number(m_minHeight, 'f', 1));
    painter.drawText(QRect(legendRect.right() - 70, legendRect.top() - 18, 70, 16),
                     Qt::AlignRight, QString::number(m_maxHeight, 'f', 1));
    painter.drawText(QRect(legendRect.left(), legendRect.bottom() + 2,
                           legendRect.width(), 16), Qt::AlignCenter,
                     "Elevation");
}

void TerrainDetailWidget::mousePressEvent(QMouseEvent* event) {
    m_lastMouse = event->position();
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton ||
        (event->button() == Qt::LeftButton && m_tool == TerrainTool::Pan)) {
        m_panning = true;
        m_autoFit = false;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton || m_tool == TerrainTool::Pan) {
        return;
    }
    const auto point = worldAt(event->position());
    if (!editableAt(point)) {
        return;
    }
    m_sculpting = true;
    // Quarter-radius spacing overlaps falloffs without making deformation
    // depend on the GUI's mouse-event frequency.
    applySamples(m_sampler.begin(point, m_radius * 0.25));
}

void TerrainDetailWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_panning) {
        const auto delta = event->position() - m_lastMouse;
        m_view.panX += delta.x();
        m_view.panY += delta.y();
        m_lastMouse = event->position();
        update();
    }
    const auto point = worldAt(event->position());
    m_hover = point;
    if (hoverChanged) {
        hoverChanged(point, editableAt(point));
    }
    if (m_sculpting && (event->buttons() & Qt::LeftButton)) {
        applySamples(m_sampler.append(point));
    } else if (m_sculpting) {
        m_sculpting = false;
        m_sampler.reset();
    }
    update();
}

void TerrainDetailWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton ||
        (event->button() == Qt::LeftButton && m_panning)) {
        m_panning = false;
        updateCursor();
    }
    if (event->button() == Qt::LeftButton) {
        m_sculpting = false;
        m_sampler.reset();
    }
}

void TerrainDetailWidget::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() == 0) {
        event->ignore();
        return;
    }
    m_autoFit = false;
    m_view = TerrainDetailInteraction::zoomAt(
        m_view, {event->position().x(), event->position().y()},
        event->angleDelta().y() > 0 ? 1.2 : 1.0 / 1.2);
    update();
    notifyZoom();
    event->accept();
}

void TerrainDetailWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_autoFit && m_active.has_value()) {
        m_view = TerrainDetailInteraction::fitActive(
            *m_active, event->size().width(), event->size().height());
        notifyZoom();
    }
}

void TerrainDetailWidget::leaveEvent(QEvent* event) {
    m_hover.reset();
    update();
    if (hoverChanged) {
        hoverChanged(std::nullopt, false);
    }
    QWidget::leaveEvent(event);
}

void TerrainDetailWidget::applySamples(const std::vector<TerrainPoint>& samples) {
    bool changed = false;
    for (const auto sample : samples) {
        changed = applyBrush(sample) || changed;
    }
    if (changed && terrainChanged) {
        terrainChanged();
    }
}

bool TerrainDetailWidget::applyBrush(TerrainPoint center) {
    if (!editableAt(center) ||
        !TerrainDetailInteraction::validBrushParameters(
            m_radius, m_strength, m_tool == TerrainTool::Smooth)) {
        return false;
    }
    const TerrainBrush brush{center, m_radius, m_strength};
    bool applied = false;
    if (m_tool == TerrainTool::Raise) {
        applied = TerrainHeightEditor::raise(*m_model, brush);
    } else if (m_tool == TerrainTool::Lower) {
        applied = TerrainHeightEditor::lower(*m_model, brush);
    } else if (m_tool == TerrainTool::Smooth) {
        applied = TerrainHeightEditor::smooth(*m_model, brush);
    }
    if (applied) {
        invalidateBrush(center);
    }
    return applied;
}

void TerrainDetailWidget::invalidateBrush(TerrainPoint center) {
    const int minX = std::max(0, static_cast<int>(std::floor(
        (center.x - m_radius) / TerrainHeightEditor::TileSize)));
    const int minY = std::max(0, static_cast<int>(std::floor(
        (center.y - m_radius) / TerrainHeightEditor::TileSize)));
    const int maxX = std::min(WorldDefinition::GridWidth - 1,
                              static_cast<int>(std::floor(
                                  (center.x + m_radius) /
                                  TerrainHeightEditor::TileSize)));
    const int maxY = std::min(WorldDefinition::GridHeight - 1,
                              static_cast<int>(std::floor(
                                  (center.y + m_radius) /
                                  TerrainHeightEditor::TileSize)));
    const double oldMin = m_minHeight;
    const double oldMax = m_maxHeight;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            m_tileImages.erase(tileKey(x, y));
            expandHeightRangeForTile(x, y);
        }
    }
    if (oldMin != m_minHeight || oldMax != m_maxHeight) {
        m_tileImages.clear();
    }
    update();
}

void TerrainDetailWidget::expandHeightRangeForTile(int tileX, int tileY) {
    const auto* terrain = m_model->terrainAt(tileX, tileY);
    if (terrain == nullptr) {
        return;
    }
    for (int row = 0; row < TerrainDefinition::OuterVerticesPerTileSide; ++row) {
        for (int column = 0;
             column < TerrainDefinition::OuterVerticesPerTileSide; ++column) {
            const double value = terrain->outerHeight(row, column);
            m_minHeight = std::min(m_minHeight, value);
            m_maxHeight = std::max(m_maxHeight, value);
        }
    }
    for (int row = 0; row < TerrainDefinition::InnerVerticesPerTileSide; ++row) {
        for (int column = 0;
             column < TerrainDefinition::InnerVerticesPerTileSide; ++column) {
            const double value = terrain->innerHeight(row, column);
            m_minHeight = std::min(m_minHeight, value);
            m_maxHeight = std::max(m_maxHeight, value);
        }
    }
}

const QImage& TerrainDetailWidget::tileImage(int tileX, int tileY) {
    const auto key = tileKey(tileX, tileY);
    const auto found = m_tileImages.find(key);
    if (found != m_tileImages.end()) {
        return found->second;
    }
    const auto* terrain = m_model->terrainAt(tileX, tileY);
    return m_tileImages.emplace(key, renderTile(*terrain)).first->second;
}

QImage TerrainDetailWidget::renderTile(const TerrainDefinition& terrain) const {
    QImage image(kImageSide, kImageSide, QImage::Format_RGB32);
    for (int halfY = 0; halfY < kImageSide; ++halfY) {
        for (int halfX = 0; halfX < kImageSide; ++halfX) {
            double height = 0.0;
            if ((halfX & 1) == 0 && (halfY & 1) == 0) {
                height = terrain.outerHeight(halfY / 2, halfX / 2);
            } else if ((halfX & 1) != 0 && (halfY & 1) != 0) {
                height = terrain.innerHeight(halfY / 2, halfX / 2);
            } else if ((halfX & 1) != 0) {
                const int row = halfY / 2;
                height = (terrain.outerHeight(row, halfX / 2) +
                          terrain.outerHeight(row, halfX / 2 + 1)) * 0.5;
            } else {
                const int column = halfX / 2;
                height = (terrain.outerHeight(halfY / 2, column) +
                          terrain.outerHeight(halfY / 2 + 1, column)) * 0.5;
            }
            image.setPixelColor(halfX, halfY, heightColor(height));
        }
    }
    return image;
}

QColor TerrainDetailWidget::heightColor(double height) const {
    const double range = std::max(1e-9, m_maxHeight - m_minHeight);
    const double t = std::clamp((height - m_minHeight) / range, 0.0, 1.0);
    const QColor low(24, 55, 105);
    const QColor middle(46, 139, 87);
    const QColor high(132, 91, 55);
    const QColor peak(235, 238, 232);
    if (t < 0.35) {
        return interpolate(low, middle, t / 0.35);
    }
    if (t < 0.72) {
        return interpolate(middle, high, (t - 0.35) / 0.37);
    }
    return interpolate(high, peak, (t - 0.72) / 0.28);
}

void TerrainDetailWidget::updateCursor() {
    if (m_panning) {
        setCursor(Qt::ClosedHandCursor);
    } else if (m_tool == TerrainTool::Pan) {
        setCursor(Qt::OpenHandCursor);
    } else {
        setCursor(Qt::CrossCursor);
    }
}

void TerrainDetailWidget::notifyZoom() {
    if (zoomChanged) {
        zoomChanged(zoomPercent());
    }
}

}  // namespace worldwright::gui
