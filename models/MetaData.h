#pragma once
#include <QObject>
#include <QString>

class MetaData : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString projectId READ projectId WRITE setProjectId NOTIFY changed)
    Q_PROPERTY(QString deviceId READ deviceId WRITE setDeviceId NOTIFY changed)
    Q_PROPERTY(QString notes READ notes WRITE setNotes NOTIFY changed)
public:
    explicit MetaData(QObject* parent=nullptr) : QObject(parent) {}

    QString projectId() const { return m_projectId; }
    QString deviceId() const { return m_deviceId; }
    QString notes() const { return m_notes; }

    void setProjectId(const QString& v) { if (m_projectId == v) return; m_projectId = v; emit changed(); }
    void setDeviceId(const QString& v) { if (m_deviceId == v) return; m_deviceId = v; emit changed(); }
    void setNotes(const QString& v) { if (m_notes == v) return; m_notes = v; emit changed(); }

signals:
    void changed();

private:
    QString m_projectId;
    QString m_deviceId;
    QString m_notes;
};
