#include "measurement_writer.h"

#include <QtGlobal>

bool MeasurementWriter::open(const QString& path,
                             const QString& moduleName,
                             int rateHz,
                             const QString& unit)
{
    close();

    m_path = path;
    m_lastError.clear();
    m_sampleIndex = 0;
    m_file.setFileName(path);

    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = QString("Failed to open measurement file %1: %2")
                          .arg(path, m_file.errorString());
        return false;
    }

    m_stream.setDevice(&m_file);
    m_stream << qMax(1, rateHz) << "\t"
             << unit << "\t"
             << moduleName << "\n";
    m_stream.flush();

    if (!checkStatus("write measurement header")) {
        close();
        return false;
    }

    return true;
}

bool MeasurementWriter::append(const QVector<TimedSample>& samples, double unitFactor)
{
    if (samples.isEmpty())
        return true;

    if (!m_file.isOpen()) {
        m_lastError = "Measurement file is not open.";
        return false;
    }

    for (const TimedSample& sample : samples) {
        m_stream << ++m_sampleIndex << "\t"
                 << QString::number(sample.value * unitFactor, 'g', 12) << "\n";
    }

    m_stream.flush();
    return checkStatus("append measurement samples");
}

void MeasurementWriter::close()
{
    if (m_stream.device())
        m_stream.flush();

    m_stream.setDevice(nullptr);

    if (m_file.isOpen())
        m_file.close();
}

bool MeasurementWriter::isOpen() const
{
    return m_file.isOpen();
}

QString MeasurementWriter::filePath() const
{
    return m_path;
}

QString MeasurementWriter::lastError() const
{
    return m_lastError;
}

bool MeasurementWriter::checkStatus(const QString& operation)
{
    if (m_stream.status() != QTextStream::Ok) {
        m_lastError = QString("Failed to %1 in %2: text stream status=%3")
                          .arg(operation, m_path)
                          .arg(static_cast<int>(m_stream.status()));
        return false;
    }

    if (m_file.error() != QFile::NoError) {
        m_lastError = QString("Failed to %1 in %2: %3")
                          .arg(operation, m_path, m_file.errorString());
        return false;
    }

    return true;
}
