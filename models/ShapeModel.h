#pragma once
#include <QAbstractListModel>
#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>

class ShapeModel : public QAbstractListModel {
    Q_OBJECT
public:

    enum Type {
        Circle = 0,
        Rect   = 1,
        Line   = 2,
        Label  = 3,
        Polyline = 4
    };
    Q_ENUM(Type)

    struct Shapes {
        Type shapeType;
        QVector<QPointF> points;
        QPointF p1;
        QPointF p2;
        QString text;
        QColor color;
        int layer = 0;
    };

    enum Role {
        TypeRole = Qt::UserRole + 1,
        PointsRole,
        P1Role,
        P2Role,
        TextRole,
        ColorRole,
        LayerRole
    };
    Q_ENUM(Role)

    explicit ShapeModel(QObject* parent=nullptr);

    int rowCount(const QModelIndex&) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int,QByteArray> roleNames() const override;

    Q_INVOKABLE void clear();

    Q_INVOKABLE void addCircle(qreal x, qreal y, qreal r, const QColor& c = QColor(0,255,255), int layer=0);
    Q_INVOKABLE void addRect(qreal x, qreal y, qreal w, qreal h, const QColor& c = QColor(255,255,0), int layer=1);
    Q_INVOKABLE void addLine(qreal x1, qreal y1, qreal x2, qreal y2, const QColor& c = QColor(0,255,0), int layer=2);
    Q_INVOKABLE void addLabel(qreal x, qreal y, const QString& t, const QColor& c = QColor(255,255,255), int layer=3);
    Q_INVOKABLE void addPolyline(const QVector<QPointF>& points, const QColor& c = QColor(255,255,255), int layer=4);

    QVector<Shapes> m_shapes;
};
