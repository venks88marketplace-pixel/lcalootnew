#include "ShapeModel.h"

ShapeModel::ShapeModel(QObject* parent) : QAbstractListModel(parent) {}

int ShapeModel::rowCount(const QModelIndex&) const { return m_shapes.size(); }

QVariant ShapeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() ||
        index.row() < 0 ||
        index.row() >= m_shapes.size() ) {
        return {};
    }
    const auto& s = m_shapes[index.row()];
    switch (role) {
    case TypeRole:
        return s.shapeType;
    case PointsRole:
    {
        return QVariant::fromValue(s.points);
    }
    case P1Role: return
            s.p1;
    case P2Role:
        return s.p2;
    case TextRole:
        return s.text;
    case ColorRole:
        return s.color;
    case LayerRole:
        return s.layer;
    default:
        return {};
    }
}

QHash<int,QByteArray> ShapeModel::roleNames() const {
    return {
        {TypeRole, "shapeType"},
        {PointsRole, "points"},
        {P1Role, "p1"},
        {P2Role, "p2"},
        {TextRole, "text"},
        {ColorRole, "color"},
        {LayerRole, "layer"}
    };
}

void ShapeModel::clear() {
    if (m_shapes.isEmpty()) return;
    beginRemoveRows({}, 0, m_shapes.size()-1);
    m_shapes.clear();
    endRemoveRows();
}

void ShapeModel::addCircle(qreal x, qreal y, qreal r, const QColor& c, int layer) {
    beginInsertRows({}, m_shapes.size(), m_shapes.size());
    m_shapes.push_back({ShapeModel::Circle,
                        {},
                        {x,y},
                        {r,r},
                        "",
                        c,
                        layer});
    endInsertRows();
}

void ShapeModel::addLine(qreal x1, qreal y1, qreal x2, qreal y2, const QColor& c, int layer) {
    beginInsertRows({}, m_shapes.size(), m_shapes.size());
    m_shapes.push_back({ShapeModel::Line,
                        {},
                        {x1,y1},
                        {x2,y2},
                        "",
                        c,
                        layer});
    endInsertRows();
}

void ShapeModel::addRect(qreal x, qreal y, qreal w, qreal h, const QColor& c, int layer) {
    beginInsertRows({}, m_shapes.size(), m_shapes.size());
    m_shapes.push_back({ShapeModel::Rect,
                        {},
                        {x,y},
                        {w,h},
                        "",
                        c,
                        layer});
    endInsertRows();
}

void ShapeModel::addLabel(qreal x, qreal y, const QString& t, const QColor& c, int layer) {
    beginInsertRows({}, m_shapes.size(), m_shapes.size());
    m_shapes.push_back({ShapeModel::Label,
                        {},
                        QPointF(x,y),
                        QPointF(),
                        t,
                        c,
                        layer});
    endInsertRows();
}

void ShapeModel::addPolyline(
    const QVector<QPointF>& points,
    const QColor& c,
    int layer) {
    if (points.size() < 2 )
        return;
    const int row = m_shapes.size();
    beginInsertRows(QModelIndex(), row, row);
    Shapes shape;
    shape.shapeType = Type::Polyline;
    shape.points = points;
    shape.color = c;
    shape.layer = layer;
    m_shapes.append(std::move(shape));
    endInsertRows();
}

