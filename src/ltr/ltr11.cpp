#include "ltr11.h"
#include "LTR/ltrapi.h"
#include <QDebug>
#include <cstring>

LTR11::LTR11()
    : m_is_open(false)
    , m_slot(-1)
{
    std::memset(&m_handle, 0, sizeof(m_handle));
}

LTR11::~LTR11()
{
    close();
}

bool LTR11::open(const QString& crateSn, int slot)
{
    if (m_is_open) close();

    INT result = LTR11_Init(&m_handle);
    if (result != LTR_OK) {
        m_lastResult = make_ltr_result(LtrApiModule::Ltr11, "LTR11_Init", result);
        return false;
    }

    QByteArray snBytes = crateSn.toLatin1();
    result = LTR11_Open(&m_handle, SADDR_DEFAULT, SPORT_DEFAULT, snBytes.constData(), slot);
    if (result != LTR_OK) {
        m_lastResult = make_ltr_result(LtrApiModule::Ltr11, "LTR11_Open", result);
        return false;
    }

    m_crateSn = crateSn;
    m_slot = slot;
    m_is_open = true;
    m_lastResult = make_ltr_success("LTR11_Open");
    return true;
}

void LTR11::close()
{
    if (m_is_open) {
        LTR11_Close(&m_handle);
        m_is_open = false;
    }
}

bool LTR11::get_config()
{
    if (!m_is_open) {
        m_lastResult = make_ltr_result(LtrApiModule::Ltr11, "LTR11_GetConfig", LTR_ERROR_CHANNEL_CLOSED);
        return false;
    }
    const INT result = LTR11_GetConfig(&m_handle);
    m_lastResult = result == LTR_OK
                       ? make_ltr_success("LTR11_GetConfig")
                       : make_ltr_result(LtrApiModule::Ltr11, "LTR11_GetConfig", result);
    return result == LTR_OK;
}

bool LTR11::apply_config()
{
    if (!m_is_open) {
        m_lastResult = make_ltr_result(LtrApiModule::Ltr11, "LTR11_SetADC", LTR_ERROR_CHANNEL_CLOSED);
        return false;
    }

    int rc = LTR11_SetADC(&m_handle);
    if (rc != LTR_OK) {
        m_lastResult = make_ltr_result(LtrApiModule::Ltr11, "LTR11_SetADC", rc);
        return false;
    }
    m_lastResult = make_ltr_success("LTR11_SetADC");
    return true;
}



bool LTR11::start()
{
    if (!m_is_open) {
        m_lastResult = make_ltr_result(LtrApiModule::Ltr11, "LTR11_Start", LTR_ERROR_CHANNEL_CLOSED);
        return false;
    }
    const INT result = LTR11_Start(&m_handle);
    m_lastResult = result == LTR_OK
                       ? make_ltr_success("LTR11_Start")
                       : make_ltr_result(LtrApiModule::Ltr11, "LTR11_Start", result);
    return result == LTR_OK;
}

bool LTR11::stop()
{
    if (!m_is_open)
        return true;
    const INT result = LTR11_Stop(&m_handle);
    m_lastResult = result == LTR_OK
                       ? make_ltr_success("LTR11_Stop")
                       : make_ltr_result(LtrApiModule::Ltr11, "LTR11_Stop", result);
    return result == LTR_OK;
}

QVector<DWORD> LTR11::receive_data(DWORD timeout, int* errorCode)
{
    QVector<DWORD> data;
    if (!m_is_open) {
        if (errorCode) *errorCode = -1;
        m_lastResult = make_ltr_result(LtrApiModule::Ltr11, "LTR_Recv", LTR_ERROR_CHANNEL_CLOSED);
        return data;
    }

    const DWORD data_size = 1024;
    DWORD buffer[data_size];
    INT received = LTR_Recv(&m_handle.Channel, buffer, nullptr, data_size, timeout);
    if (received < 0) {
        if (errorCode) *errorCode = received;
        m_lastResult = make_ltr_result(LtrApiModule::Ltr11, "LTR_Recv", received);
        return data;
    }

    data.resize(received);
    std::memcpy(data.data(), buffer, received * sizeof(DWORD));
    if (errorCode) *errorCode = 0;
    m_lastResult = make_ltr_success("LTR_Recv");
    return data;
}

void LTR11::set_ADC_rate(BYTE prescaler, BYTE divider)
{
    m_handle.ADCRate.prescaler = prescaler;
    m_handle.ADCRate.divider = divider;
}

void LTR11::set_logical_channels(int count, const BYTE* channelTable)
{
    if (count > LTR11_MAX_CHANNEL) count = LTR11_MAX_CHANNEL;
    m_handle.LChQnt = count;
    for (int i = 0; i < count; ++i)
        m_handle.LChTbl[i] = channelTable[i];
}
