#include "WorldMapWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace worldwright::gui {

WorldMapWidget::WorldMapWidget(WorldEditorModel* model, QWidget* parent)
    : QWidget(parent), m_model(model) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 320);
    setAutoFillBackground(false);
    updateCursor();
}

void WorldMapWidget::resetView() {
    m_autoFit = false;
    m_view.tileSize = 12.0;
    m_view.panX = (width() - WorldDefinition::GridWidth * m_view.tileSize) / 2.0;
    m_view.panY = (height() - WorldDefinition::GridHeight * m_view.tileSize) / 2.0;
    update();
    notifyZoom();
}

void WorldMapWidget::fitWorld() {
    m_autoFit = true;
    m_view = EditorInteraction::fitWorld(width(), height());
    update();
    notifyZoom();
}

void WorldMapWidget::setEditMode(EditMode mode) {
    m_mode = mode;
    m_selecting = false;
    m_painting = false;
    m_panning = false;
    updateCursor();
}

EditMode WorldMapWidget::editMode() const noexcept {
    return m_mode;
}

double WorldMapWidget::zoomPercent() const noexcept {
    return m_view.tileSize / 12.0 * 100.0;
}

QPointF WorldMapWidget::gridOrigin() const {
    return {m_view.panX, m_view.panY};
}

std::optional<worldwright::TileCoord> WorldMapWidget::tileAt(
    const QPointF& point) const {
    const auto local = point - gridOrigin();
    return WorldEditorModel::tileAt(local.x(), local.y(), m_view.tileSize);
}

TileCoord WorldMapWidget::unboundedTileAt(const QPointF& point) const {
    const auto local = point - gridOrigin();
    return {static_cast<int>(std::floor(local.x() / m_view.tileSize)),
            static_cast<int>(std::floor(local.y() / m_view.tileSize))};
}

void WorldMapWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(28, 31, 36));
    painter.setRenderHint(QPainter::Antialiasing, false);

    const auto origin = gridOrigin();
    const QRectF gridRect(origin.x(), origin.y(),
                          WorldDefinition::GridWidth * m_view.tileSize,
                          WorldDefinition::GridHeight * m_view.tileSize);
    painter.fillRect(gridRect, QColor(45, 50, 58));

    const int firstX =
        std::max(0, static_cast<int>(std::floor(-origin.x() / m_view.tileSize)));
    const int firstY =
        std::max(0, static_cast<int>(std::floor(-origin.y() / m_view.tileSize)));
    const int lastX = std::min(
        WorldDefinition::GridWidth - 1,
        static_cast<int>((width() - origin.x()) / m_view.tileSize));
    const int lastY = std::min(
        WorldDefinition::GridHeight - 1,
        static_cast<int>((height() - origin.y()) / m_view.tileSize));

    for (int y = firstY; y <= lastY; ++y) {
        for (int x = firstX; x <= lastX; ++x) {
            const QRectF cell(origin.x() + x * m_view.tileSize,
                              origin.y() + y * m_view.tileSize,
                              m_view.tileSize, m_view.tileSize);
            if (m_model->world().hasTile(x, y)) {
                QRectF enabledCell = cell;
                enabledCell.adjust(0.75, 0.75, -0.75, -0.75);
                painter.fillRect(enabledCell, QColor(46, 153, 145));
                if (m_view.tileSize >= 8.0) {
                    painter.setPen(QPen(QColor(125, 214, 202), 1.0));
                    painter.drawRect(enabledCell);
                }
            }
            if (m_model->isSelected(x, y)) {
                painter.fillRect(cell, QColor(255, 184, 45, 90));
                painter.setPen(QPen(QColor(255, 205, 75), 2.0));
                QRectF outline = cell;
                outline.adjust(1.0, 1.0, -1.0, -1.0);
                painter.drawRect(outline);
            }
            if (m_view.tileSize >= 30.0) {
                painter.setPen(m_model->world().hasTile(x, y)
                                   ? QColor(245, 250, 250)
                                   : QColor(185, 192, 202));
                painter.drawText(cell, Qt::AlignCenter,
                                 QString::number(x) + "," + QString::number(y));
            }
        }
    }

    if (m_view.tileSize >= 4.0) {
        painter.setPen(QPen(QColor(76, 83, 94), 1.0));
        for (int i = 0; i <= WorldDefinition::GridSize; ++i) {
            const double position = i * m_view.tileSize;
            painter.drawLine(QPointF(origin.x() + position, origin.y()),
                             QPointF(origin.x() + position, gridRect.bottom()));
            painter.drawLine(QPointF(origin.x(), origin.y() + position),
                             QPointF(gridRect.right(), origin.y() + position));
        }
    }
    if (m_hoveredTile.has_value()) {
        const QRectF hoverRect(
            origin.x() + m_hoveredTile->x * m_view.tileSize,
            origin.y() + m_hoveredTile->y * m_view.tileSize, m_view.tileSize,
            m_view.tileSize);
        painter.setPen(QPen(QColor(245, 248, 252), 2.0, Qt::DashLine));
        QRectF outline = hoverRect;
        outline.adjust(2.0, 2.0, -2.0, -2.0);
        painter.drawRect(outline);
    }
    painter.setPen(QPen(QColor(190, 198, 208), 2.0));
    painter.drawRect(gridRect);
}

void WorldMapWidget::mousePressEvent(QMouseEvent* event) {
    m_lastMouse = event->position();
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton ||
        (event->button() == Qt::LeftButton && m_mode == EditMode::Pan)) {
        m_panning = true;
        m_autoFit = false;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        const auto tile = tileAt(event->position());
        if ((m_mode == EditMode::PaintTerrain ||
             m_mode == EditMode::EraseTerrain) && tile.has_value()) {
            m_painting = true;
            m_lastPaintTile = *tile;
            applyStroke(*tile, *tile);
        } else if (m_mode == EditMode::Select && tile.has_value()) {
            m_selecting = true;
            m_dragStart = *tile;
            m_model->selectTile(tile->x, tile->y);
            update();
            if (selectionChanged) {
                selectionChanged();
            }
        } else if (m_mode == EditMode::Select) {
            m_model->clearSelection();
            update();
            if (selectionChanged) {
                selectionChanged();
            }
        }
    }
}

void WorldMapWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_panning) {
        const auto delta = event->position() - m_lastMouse;
        m_view.panX += delta.x();
        m_view.panY += delta.y();
        m_lastMouse = event->position();
        update();
    }
    const auto tile = tileAt(event->position());
    if (tile != m_hoveredTile) {
        m_hoveredTile = tile;
        update();
    }
    if (hoverChanged) {
        hoverChanged(tile);
    }
    if (m_selecting && (event->buttons() & Qt::LeftButton) && tile.has_value()) {
        m_model->selectRectangle(m_dragStart.x, m_dragStart.y, tile->x, tile->y);
        update();
        if (selectionChanged) {
            selectionChanged();
        }
    }
    if (m_painting && (event->buttons() & Qt::LeftButton)) {
        const auto current = unboundedTileAt(event->position());
        applyStroke(m_lastPaintTile, current);
        m_lastPaintTile = current;
    } else if (m_painting) {
        m_painting = false;
    }
}

void WorldMapWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton ||
        (event->button() == Qt::LeftButton && m_panning)) {
        m_panning = false;
        updateCursor();
    }
    if (event->button() == Qt::LeftButton) {
        m_selecting = false;
        m_painting = false;
    }
}

void WorldMapWidget::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() == 0) {
        event->ignore();
        return;
    }
    m_autoFit = false;
    const double factor = event->angleDelta().y() > 0 ? 1.2 : (1.0 / 1.2);
    m_view = EditorInteraction::zoomAt(m_view, event->position().x(),
                                       event->position().y(), factor);
    update();
    notifyZoom();
    event->accept();
}

void WorldMapWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_autoFit) {
        m_view = EditorInteraction::fitWorld(event->size().width(),
                                             event->size().height());
        notifyZoom();
    }
}

void WorldMapWidget::leaveEvent(QEvent* event) {
    m_hoveredTile.reset();
    update();
    if (hoverChanged) {
        hoverChanged(std::nullopt);
    }
    QWidget::leaveEvent(event);
}

void WorldMapWidget::applyStroke(const TileCoord& from, const TileCoord& to) {
    const auto tiles = EditorInteraction::gridLine(from.x, from.y, to.x, to.y);
    if (EditorInteraction::applyTerrainStroke(*m_model, m_mode, tiles)) {
        update();
        if (terrainChanged) {
            terrainChanged();
        }
    }
}

void WorldMapWidget::updateCursor() {
    if (m_panning) {
        setCursor(Qt::ClosedHandCursor);
    } else if (m_mode == EditMode::Pan) {
        setCursor(Qt::OpenHandCursor);
    } else {
        setCursor(Qt::CrossCursor);
    }
}

void WorldMapWidget::notifyZoom() {
    if (zoomChanged) {
        zoomChanged(zoomPercent());
    }
}

}  // namespace worldwright::gui
