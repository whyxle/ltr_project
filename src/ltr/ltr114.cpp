#include "ltr114.h"

#include <cstring>

LTR114::LTR114()
    : m_is_open(false)
    , m_slot(-1)
{
    std::memset(&m_handle, 0, sizeof(m_handle));
}

LTR114::~LTR114()
{
    close();
}

bool LTR114::open(const QString& crate_sn, int slot)
{
    if (m_is_open)
        close();

    INT result = LTR114_Init(&m_handle);
    if (result != LTR_OK) {
        m_lastResult = make_ltr_result(LtrApiModule::Ltr114, "LTR114_Init", result);
        return false;
    }

    QByteArray sn_bytes = crate_sn.toLatin1();
    result = LTR114_Open(&m_handle, SADDR_DEFAULT, SPORT_DEFAULT, sn_bytes.constData(), slot);

    if (result == LTR_WARNING_MODULE_IN_USE)
        result = LTR_OK;

    if (result != LTR_OK) {
        m_lastResult = make_ltr_result(LtrApiModule::Ltr114, "LTR114_Open", result);
        return false;
    }

    m_slot = slot;
    m_is_open = true;
    m_lastResult = make_ltr_success("LTR114_Open");
    return true;
}

void LTR114::close()
{
    if (m_is_open) {
        LTR114_Close(&m_handle);
        m_is_open = false;
    }
}

bool LTR114::get_config()
{
    if (!m_is_open) {
        m_lastResult = make_ltr_result(LtrApiModule::Ltr114, "LTR114_GetConfig", LTR_ERROR_CHANNEL_CLOSED);
        return false;
    }
    const INT result = LTR114_GetConfig(&m_handle);
    m_lastResult = result == LTR_OK
                       ? make_ltr_success("LTR114_GetConfig")
                       : make_ltr_result(LtrApiModule::Ltr114, "LTR114_GetConfig", result);
    return result == LTR_OK;
}

bool LTR114::apply_config()
{
    if (!m_is_open) {
        m_lastResult = make_ltr_result(LtrApiModule::Ltr114, "LTR114_SetADC", LTR_ERROR_CHANNEL_CLOSED);
        return false;
    }

    const INT result = LTR114_SetADC(&m_handle);
    m_lastResult = result == LTR_OK
                       ? make_ltr_success("LTR114_SetADC")
                       : make_ltr_result(LtrApiModule::Ltr114, "LTR114_SetADC", result);
    return result == LTR_OK;
}

bool LTR114::start()
{
    if (!m_is_open) {
        m_lastResult = make_ltr_result(LtrApiModule::Ltr114, "LTR114_Start", LTR_ERROR_CHANNEL_CLOSED);
        return false;
    }
    const INT result = LTR114_Start(&m_handle);
    m_lastResult = result == LTR_OK
                       ? make_ltr_success("LTR114_Start")
                       : make_ltr_result(LtrApiModule::Ltr114, "LTR114_Start", result);
    return result == LTR_OK;
}

bool LTR114::stop()
{
    if (!m_is_open)
        return true;
    const INT result = LTR114_Stop(&m_handle);
    m_lastResult = result == LTR_OK
                       ? make_ltr_success("LTR114_Stop")
                       : make_ltr_result(LtrApiModule::Ltr114, "LTR114_Stop", result);
    return result == LTR_OK;
}

QVector<DWORD> LTR114::receive_data(DWORD timeout, int* error_code)
{
    QVector<DWORD> data;

    if (!m_is_open) {
        if (error_code)
            *error_code = -1;
        m_lastResult = make_ltr_result(LtrApiModule::Ltr114, "LTR114_Recv", LTR_ERROR_CHANNEL_CLOSED);
        return data;
    }

    INT requested_size = static_cast<INT>(10 * m_handle.FrameLength);
    if (requested_size <= 0)
        requested_size = 10;

    data.resize(requested_size);

    INT recv_result = LTR114_Recv(&m_handle, data.data(), nullptr, requested_size, timeout);
    if (recv_result < 0) {
        if (error_code)
            *error_code = recv_result;
        m_lastResult = make_ltr_result(LtrApiModule::Ltr114, "LTR114_Recv", recv_result);
        data.clear();
        return data;
    }

    data.resize(recv_result);

    if (error_code)
        *error_code = 0;
    m_lastResult = make_ltr_success("LTR114_Recv");

    return data;
}

void LTR114::set_freq_divider(DWORD freq_divider)
{
    m_handle.FreqDivider = freq_divider;
}

void LTR114::set_logical_channels(int channel_count, const TLTR114_LCHANNEL* channel_table)
{
    if (channel_count < 0)
        channel_count = 0;




    m_handle.LChQnt = channel_count;

    for (int i = 0; i < channel_count; ++i)
        m_handle.LChTbl[i] = channel_table[i];
}

void LTR114::set_sync_mode(DWORD sync_mode)
{
    m_handle.SyncMode = sync_mode;
}

void LTR114::set_interval(DWORD interval)
{
    m_handle.Interval = interval;
}

QPair<QVector<DWORD>, QVector<DWORD>> LTR114::receive_data_with_marks(DWORD timeout, int* error_code)
{
    QVector<DWORD> data;
    QVector<DWORD> tmark;

    if (!m_is_open) {
        if (error_code) *error_code = -1;
        m_lastResult = make_ltr_result(LtrApiModule::Ltr114, "LTR114_Recv", LTR_ERROR_CHANNEL_CLOSED);
        return {data, tmark};
    }

    INT requested_size = static_cast<INT>(10 * m_handle.FrameLength);
    if (requested_size <= 0) requested_size = 10;

    data.resize(requested_size);
    tmark.resize(requested_size);

    INT recv_result = LTR114_Recv(&m_handle, data.data(), tmark.data(), requested_size, timeout);

    if (recv_result < 0) {
        if (error_code) *error_code = recv_result;
        m_lastResult = make_ltr_result(LtrApiModule::Ltr114, "LTR114_Recv", recv_result);
        return {{}, {}};
    }

    data.resize(recv_result);
    tmark.resize(recv_result);

    if (error_code) *error_code = 0;
    m_lastResult = make_ltr_success("LTR114_Recv");
    return {data, tmark};
}
