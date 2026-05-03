#ifndef LTR_RESULT_H
#define LTR_RESULT_H

#include <QString>

#include "LTR/ltrapi.h"

enum class LtrApiModule
{
    Common,
    Ltr11,
    Ltr114,
    Ltr212
};

struct LtrResult
{
    bool ok = true;
    INT code = LTR_OK;
    QString operation;
    QString message;
};

QString ltr_error_text(LtrApiModule module, INT code);
LtrResult make_ltr_result(LtrApiModule module, const QString& operation, INT code, bool ok = false);
LtrResult make_ltr_success(const QString& operation = QString());

#endif
