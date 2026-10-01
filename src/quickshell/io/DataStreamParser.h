#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace wqs {

/// Port of Quickshell's `DataStreamParser`, the base for objects that process a running
/// process's stdout/stderr. `Process` feeds bytes in and calls finish() when the process
/// exits.
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

/// Port of `StdioCollector`: buffers the entire stream and exposes it as `text`/`data`
/// once the process exits (signalled by `streamFinished`).
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

/// Port of `SplitParser`: splits the stream on `splitMarker` (default newline) and emits
/// `read(line)` per line.
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
