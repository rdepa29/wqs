#include "DataStreamParser.h"

namespace wqs {

DataStreamParser::DataStreamParser(QObject *parent)
    : QObject(parent)
{
}

DataStreamParser::~DataStreamParser() = default;

void DataStreamParser::parse(const QByteArray &data)
{
    emit this->data(data);
}

void DataStreamParser::finish()
{
    emit streamFinished();
}

void DataStreamParser::reset()
{
}

StdioCollector::StdioCollector(QObject *parent)
    : DataStreamParser(parent)
{
}

void StdioCollector::parse(const QByteArray &data)
{
    m_data += data;
    emit dataChanged();
    emit textChanged();
}

void StdioCollector::finish()
{
    DataStreamParser::finish();
}

void StdioCollector::reset()
{
    m_data.clear();
    emit dataChanged();
    emit textChanged();
}

SplitParser::SplitParser(QObject *parent)
    : DataStreamParser(parent)
{
}

void SplitParser::setSplitMarker(const QString &marker)
{
    if (m_splitMarker == marker)
        return;
    m_splitMarker = marker;
    emit splitMarkerChanged();
}

void SplitParser::parse(const QByteArray &data)
{
    m_buffer += data;

    const QByteArray marker = m_splitMarker.toUtf8();
    if (marker.isEmpty()) {
        if (!m_buffer.isEmpty()) {
            const QByteArray line = m_buffer;
            m_buffer.clear();
            emit read(QString::fromUtf8(line));
            emit bufferChanged();
        }
        return;
    }

    qsizetype index;
    while ((index = m_buffer.indexOf(marker)) >= 0) {
        const QByteArray line = m_buffer.left(index);
        m_buffer.remove(0, index + marker.size());
        emit read(QString::fromUtf8(line));
    }
    emit bufferChanged();
}

void SplitParser::finish()
{
    if (!m_buffer.isEmpty()) {
        const QByteArray line = m_buffer;
        m_buffer.clear();
        emit read(QString::fromUtf8(line));
        emit bufferChanged();
    }
    DataStreamParser::finish();
}

void SplitParser::reset()
{
    m_buffer.clear();
    emit bufferChanged();
}

} // namespace wqs
