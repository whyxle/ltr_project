#include "ltr_result.h"

#include "LTR/ltr11api.h"
#include "LTR/ltr114api.h"
#include "LTR/ltr212api.h"

namespace {

QString errorStringFromPtr(LPCSTR text)
{
    if (!text || !*text)
        return {};
    return QString::fromLocal8Bit(text).trimmed();
}

} // namespace

QString ltr_error_text(LtrApiModule module, INT code)
{
    QString text;
    switch (module) {
    case LtrApiModule::Ltr11:
        text = errorStringFromPtr(LTR11_GetErrorString(code));
        break;
    case LtrApiModule::Ltr114:
        text = errorStringFromPtr(LTR114_GetErrorString(code));
        break;
    case LtrApiModule::Ltr212:
        text = errorStringFromPtr(LTR212_GetErrorString(code));
        break;
    case LtrApiModule::Common:
        break;
    }

    if (text.isEmpty())
        text = errorStringFromPtr(LTR_GetErrorString(code));

    if (text.isEmpty())
        text = QString("code=%1").arg(code);
    else
        text = QString("%1 (code=%2)").arg(text).arg(code);

    return text;
}

LtrResult make_ltr_result(LtrApiModule module, const QString& operation, INT code, bool ok)
{
    LtrResult result;
    result.ok = ok;
    result.code = code;
    result.operation = operation;
    result.message = ok
                         ? QString("%1: %2").arg(operation, ltr_error_text(module, code))
                         : QString("%1 failed: %2").arg(operation, ltr_error_text(module, code));
    return result;
}

LtrResult make_ltr_success(const QString& operation)
{
    LtrResult result;
    result.ok = true;
    result.code = LTR_OK;
    result.operation = operation;
    result.message = operation.isEmpty() ? QStringLiteral("OK") : QString("%1: OK").arg(operation);
    return result;
}
