#pragma once
#include <string>
#include <QVariant>
#include <QString>
#include <QColor>
#include <QVector>
#include <QList>
#include <QAbstractListModel>

class UIMessageModel : public QAbstractListModel {
    Q_OBJECT

public:
    struct UiMessagePart {
        QString text;
        QColor color;
    };

    struct UiMessage {
        QVector<UiMessagePart> parts;
    };

    enum Roles {
        PartsRole = Qt::UserRole + 1
    };

    explicit UIMessageModel(QObject* parent=nullptr);

    int rowCount(const QModelIndex&) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int,QByteArray> roleNames() const override;

    Q_INVOKABLE void clear();

    Q_INVOKABLE void addMessage(QString text, QColor color, QString level);
    Q_INVOKABLE void addMessageParts(const QVector<UiMessagePart>& parts);

private:
    QVector<UiMessage> m_messages;
};