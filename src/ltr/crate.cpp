#include "crate.h"
#include "ltr11.h"
#include "ltr114.h"
#include <QDebug>
#include <cstring>

QList<QString> Crate::enumerate_crates()
{
    QList<QString> result;
    TLTR hsrv;
    if (LTR_Init(&hsrv) != 0) return result;

    hsrv.saddr = LTRD_ADDR_DEFAULT;
    hsrv.sport = LTRD_PORT_DEFAULT;
    std::strncpy(hsrv.csn, LTR_CSN_SERVER_CONTROL, LTR_CRATE_SERIAL_SIZE - 1);
    hsrv.csn[LTR_CRATE_SERIAL_SIZE - 1] = '\0';
    hsrv.cc = LTR_CC_CHNUM_CONTROL;

    if (LTR_Open(&hsrv) != LTR_OK) {
        return result;
    }

    QByteArray csnBuf(LTR_CRATES_MAX * LTR_CRATE_SERIAL_SIZE, Qt::Uninitialized);
    if (LTR_GetCrates(&hsrv, reinterpret_cast<BYTE*>(csnBuf.data())) == 0) {
        for (int i = 0; i < LTR_CRATES_MAX; ++i) {
            const char* entry = csnBuf.constData() + i * LTR_CRATE_SERIAL_SIZE;
            if (entry[0] != '\0') {
                result << QString::fromLatin1(entry, strnlen(entry, LTR_CRATE_SERIAL_SIZE)).trimmed();
            }
        }
    }
    LTR_Close(&hsrv);
    return result;
}


Crate::Crate(const QString& serial_number)
    : m_serial_number(serial_number)
    , m_hcrate(nullptr)
{
    m_hcrate = new TLTR;
    INT result = LTR_Init(m_hcrate);
    if (result != LTR_OK) {
        m_lastResult = make_ltr_result(LtrApiModule::Common, "LTR_Init", result);
        delete m_hcrate;
        m_hcrate = nullptr;
        return;
    }

    m_hcrate->saddr = LTRD_ADDR_DEFAULT;
    m_hcrate->sport = LTRD_PORT_DEFAULT;
    QByteArray snBytes = m_serial_number.toLatin1();
    std::memset(m_hcrate->csn, 0, LTR_CRATE_SERIAL_SIZE);
    std::memcpy(m_hcrate->csn, snBytes.constData(), qMin(snBytes.size(), LTR_CRATE_SERIAL_SIZE - 1));
    m_hcrate->cc = LTR_CC_CHNUM_CONTROL;

    result = LTR_Open(m_hcrate);
    if (result != LTR_OK) {
        m_lastResult = make_ltr_result(LtrApiModule::Common, "LTR_Open(crate control)", result);
        LTR_Close(m_hcrate);
        delete m_hcrate;
        m_hcrate = nullptr;
        return;
    }

    m_lastResult = make_ltr_success("LTR_Open(crate control)");
}

Crate::~Crate()
{
    if (m_hcrate) {
        LTR_Close(m_hcrate);
        delete m_hcrate;
    }
}

WORD Crate::get_slot_count() const
{
    if(!m_hcrate) return 0;

    TLTR_CRATE_STATISTIC stat;

    if (LTR_GetCrateStatistic(m_hcrate, LTR_CRATE_IFACE_UNKNOWN, m_serial_number.toLatin1().constData(),
                              &stat, sizeof(stat)) == LTR_OK) {
        return stat.modules_cnt;
    }
    return 0; // ошибка
}

QList<QPair<int, WORD>> Crate::get_modules() const
{
    QList<QPair<int, WORD>> modules;
    if (!m_hcrate) return modules;

    WORD mid[LTR_MODULES_PER_CRATE_MAX] = {};
    if (LTR_GetCrateModules(m_hcrate, mid) == 0) {
        for (int slot = 0; slot < LTR_MODULES_PER_CRATE_MAX; ++slot) {
            if (mid[slot] != LTR_MID_EMPTY) {
                modules.append(qMakePair(slot + 1, mid[slot])); // слоты в LTR нумеруются с 1
            }
        }
    }
    return modules;
}


// создаем объект модуля на основе его идентификатора
std::unique_ptr<Module> Crate::create_module(int slot) const
{
    // if (!m_hcrate || slot < 1 || slot > LTR_MODULES_PER_CRATE_MAX)
    //     return nullptr;

    WORD mid[LTR_MODULES_PER_CRATE_MAX] = {};
    if (LTR_GetCrateModules(m_hcrate, mid) != 0)
        return nullptr;

    WORD moduleId = mid[slot - 1];
    if (moduleId == LTR_MID_EMPTY || moduleId == LTR_MID_IDENTIFYING || moduleId == LTR_MID_INVALID)
        return nullptr;

    //  потом расширить для других типов модулей
    if (moduleId == LTR_MID_LTR11) {
        auto module = std::make_unique<LTR11>();
        if (module->open(m_serial_number, slot))
            return module;
    }

    if (moduleId == LTR_MID_LTR114) {
        auto module = std::make_unique<LTR114>();
        if (module->open(m_serial_number, slot))
            return module;
    }
    // нужны классы для других модулей

    return nullptr;
}

bool Crate::start_second_marks()
{
    if (!m_hcrate) {
        m_lastResult = make_ltr_result(LtrApiModule::Common, "LTR_StartSecondMark", LTR_ERROR_CHANNEL_CLOSED);
        qDebug() << m_lastResult.message;
        return false;
    }

    LTR_StopSecondMark(m_hcrate);
    const INT result = LTR_StartSecondMark(m_hcrate, LTR_MARK_INTERNAL);
    if (result != LTR_OK) {
        m_lastResult = make_ltr_result(LtrApiModule::Common, "LTR_StartSecondMark", result);
        qDebug() << m_lastResult.message;
        return false;
    }

    m_lastResult = make_ltr_success("LTR_StartSecondMark");
    return true;
}

bool Crate::make_start_mark()
{
    if (!m_hcrate) {
        m_lastResult = make_ltr_result(LtrApiModule::Common, "LTR_MakeStartMark", LTR_ERROR_CHANNEL_CLOSED);
        qDebug() << m_lastResult.message;
        return false;
    }

    const INT result = LTR_MakeStartMark(m_hcrate, LTR_MARK_INTERNAL);
    if (result != LTR_OK) {
        m_lastResult = make_ltr_result(LtrApiModule::Common, "LTR_MakeStartMark", result);
        qDebug() << m_lastResult.message;
        return false;
    }

    m_lastResult = make_ltr_success("LTR_MakeStartMark");
    return true;
}

void Crate::stop_sync_marks()
{
    if (m_hcrate) {
        LTR_StopSecondMark(m_hcrate);
        // Опционально выключаем START (не обязательно)
        LTR_MakeStartMark(m_hcrate, LTR_MARK_OFF);
    }
}
