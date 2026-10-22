#include "UIMessageModel.h"

UIMessageModel::UIMessageModel(QObject* parent) : QAbstractListModel(parent) {}

QVariant UIMessageModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};
    if (role == PartsRole) {
        const auto& msg = m_messages[index.row()];
        QVariantList parts;
        for (const auto& p : msg.parts) {
            QVariantMap m;
            m["text"] = p.text;
            m["color"] = p.color;
            parts << m;
        }
        return parts;
    }
    return {};
}
QHash<int, QByteArray> UIMessageModel::roleNames() const {
    return {
        { PartsRole, "messageParts" }
    };
}
void UIMessageModel::clear() {
    if (m_messages.isEmpty()) return;
    beginRemoveRows({}, 0, m_messages.size()-1);
    m_messages.clear();
    endRemoveRows();
}
void UIMessageModel::addMessage(QString text, QColor color, QString level) {
    Q_UNUSED(level)

    addMessageParts({
        UiMessagePart{std::move(text), color}
    });
}

void UIMessageModel::addMessageParts(const QVector<UiMessagePart>& parts)
{
    const int row = m_messages.size();
    beginInsertRows({}, row, row);

    UiMessage msg;
    msg.parts = parts;

    m_messages.push_back(std::move(msg));
    endInsertRows();
}

int UIMessageModel::rowCount(const QModelIndex&) const {
    return m_messages.size();
}
