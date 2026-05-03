#ifndef LTR212_H
#define LTR212_H

#include "module.h"
#include "ltr_result.h"
#include "LTR/ltr212api.h"

class LTR212 : public Module
{
public:
    LTR212();
    ~LTR212() override;


    bool open(const QString& crate_sn, int slot) override;
    bool open(const QString& crate_sn, int slot, const QString& bios);

    void close() override;
    bool apply_config() override;
    bool start() override;
    bool stop() override;
    QVector<DWORD> receive_data(DWORD timeout, int* error_code = nullptr) override;
    bool get_config() override;
    QVector<double> process_data(const QVector<DWORD>& raw_data, bool to_volts = true, int* error_code = nullptr);


    void set_acq_mode(INT mode);
    void set_use_clb(INT use);
    void set_use_fabric_clb(INT use);
    void set_ref_voltage(INT ref);
    void set_ac_mode(INT ac);
    void set_logical_channels(int count, const INT* channel_table);
    void set_size();

    QPair<QVector<DWORD>, QVector<DWORD>> receive_data_with_marks(
        DWORD timeout, int* error_code = nullptr);

    QString module_name() const { return QString::fromLatin1(m_handle.ModuleInfo.Name); }
    QString module_serial() const { return QString::fromLatin1(m_handle.ModuleInfo.Serial); }
    LtrResult last_result() const { return m_lastResult; }

    TLTR212* handle() { return &m_handle; }

private:
    TLTR212 m_handle;
    LtrResult m_lastResult;
    bool m_is_open;
    int m_slot;
};

#endif
