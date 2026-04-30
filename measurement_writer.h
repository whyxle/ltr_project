#ifndef MEASUREMENT_WRITER_H
#define MEASUREMENT_WRITER_H

#include <QFile>
#include <QString>
#include <QTextStream>
#include <QVector>

#include "sync_timeline.h"

class MeasurementWriter
{
public:
    bool open(const QString& path, const QString& moduleName, int rateHz, const QString& unit);
    bool append(const QVector<TimedSample>& samples, double unitFactor);
    void close();

    bool isOpen() const;
    QString filePath() const;
    QString lastError() const;

private:
    bool checkStatus(const QString& operation);

    QFile m_file;
    QTextStream m_stream;
    QString m_path;
    QString m_lastError;
    quint64 m_sampleIndex = 0;
};

#endif // MEASUREMENT_WRITER_H
