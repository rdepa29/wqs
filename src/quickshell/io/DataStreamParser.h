#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace wqs {

class DataStreamParser : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit DataStreamParser(QObject *parent = nullptr);
    ~DataStreamParser() override;

    Q_INVOKABLE virtual void parse(const QByteArray &data);
    virtual void finish();
    Q_INVOKABLE virtual void reset();

signals:
    void data(QByteArray data);
    void streamFinished();
};

class StdioCollector : public DataStreamParser
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QByteArray data READ data NOTIFY dataChanged)
    Q_PROPERTY(QString text READ text NOTIFY textChanged)

public:
    explicit StdioCollector(QObject *parent = nullptr);

    void parse(const QByteArray &data) override;
    void finish() override;
    void reset() override;

    QByteArray data() const { return m_data; }
    QString text() const { return QString::fromUtf8(m_data); }

signals:
    void dataChanged();
    void textChanged();

private:
    QByteArray m_data;
};

class SplitParser : public DataStreamParser
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString splitMarker READ splitMarker WRITE setSplitMarker NOTIFY splitMarkerChanged)
    Q_PROPERTY(QString delimiter READ splitMarker WRITE setSplitMarker NOTIFY splitMarkerChanged)
    Q_PROPERTY(QString buffer READ buffer NOTIFY bufferChanged)

public:
    explicit SplitParser(QObject *parent = nullptr);

    void parse(const QByteArray &data) override;
    void finish() override;
    void reset() override;

    QString splitMarker() const { return m_splitMarker; }
    void setSplitMarker(const QString &marker);
    QString buffer() const { return QString::fromUtf8(m_buffer); }

signals:
    void read(QString line);
    void splitMarkerChanged();
    void bufferChanged();

private:
    QString m_splitMarker = QStringLiteral("\n");
    QByteArray m_buffer;
};

} // namespace wqs
