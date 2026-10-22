#pragma once
#include <QObject>
#include <QString>

class Session : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString userId READ userId WRITE setUserId NOTIFY changed)
    Q_PROPERTY(QString accessToken READ accessToken WRITE setAccessToken NOTIFY changed)
public:
    explicit Session(QObject* parent=nullptr) : QObject(parent) {}

    QString userId() const { return m_userId; }
    QString accessToken() const { return m_accessToken; }

    void setUserId(const QString& v) { if (m_userId == v) return; m_userId = v; emit changed(); }
    void setAccessToken(const QString& v) { if (m_accessToken == v) return; m_accessToken = v; emit changed(); }

signals:
    void changed();

private:
    QString m_userId;
    QString m_accessToken;
};
