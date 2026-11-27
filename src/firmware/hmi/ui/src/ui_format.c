#include "hmi/ui_format.h"

#include <stdio.h>
#include <string.h>

#include "ecu/ecu_protocol.h"

int hmi_unit_decimals(uint8_t unit)
{
    switch (unit) {
    case ECU_UNIT_MS_X100: return 2;
    case ECU_UNIT_DEG_X10:
    case ECU_UNIT_PCT_X10:
    case ECU_UNIT_DEGC_X10:
    case ECU_UNIT_VOLT_X10:
    case ECU_UNIT_MS_X10: return 1;
    default: return 0;
    }
}

const char *hmi_unit_suffix(uint8_t unit)
{
    switch (unit) {
    case ECU_UNIT_RPM: return "rpm";
    case ECU_UNIT_KPA: return "kPa";
    case ECU_UNIT_MS_X100:
    case ECU_UNIT_MS_X10: return "ms";
    case ECU_UNIT_DEG_X10: return "°";
    case ECU_UNIT_PCT_X10: return "%";
    case ECU_UNIT_US: return "µs";
    case ECU_UNIT_DEGC:
    case ECU_UNIT_DEGC_X10: return "°C";
    case ECU_UNIT_VOLT_X10: return "V";
    case ECU_UNIT_PCT_PER_S: return "%/s";
    case ECU_UNIT_CC_MIN: return "cc/min";
    default: return "";
    }
}

void hmi_fmt_value(char *buf, size_t n, uint8_t unit, int32_t v, bool sign)
{
    const int dec = hmi_unit_decimals(unit);
    const char *s = v < 0 ? "-" : (sign && v > 0 ? "+" : "");
    const uint32_t a = (uint32_t)(v < 0 ? -v : v);
    if (dec == 2) {
        snprintf(buf, n, "%s%lu,%02lu", s, (unsigned long)(a / 100u), (unsigned long)(a % 100u));
    } else if (dec == 1) {
        snprintf(buf, n, "%s%lu,%lu", s, (unsigned long)(a / 10u), (unsigned long)(a % 10u));
    } else {
        snprintf(buf, n, "%s%lu", s, (unsigned long)a);
    }
}

void hmi_fmt_with_unit(char *buf, size_t n, uint8_t unit, int32_t v, bool sign)
{
    char num[24];
    hmi_fmt_value(num, sizeof(num), unit, v, sign);
    const char *suf = hmi_unit_suffix(unit);
    if (unit == ECU_UNIT_DEG_X10) {
        snprintf(buf, n, "%s%s", num, suf); /* 18,5° */
    } else if (suf[0]) {
        snprintf(buf, n, "%s %s", num, suf);
    } else {
        snprintf(buf, n, "%s", num);
    }
}

bool hmi_parse_value(const char *text, uint8_t unit, int32_t *out)
{
    const int dec = hmi_unit_decimals(unit);
    int32_t ip = 0, fp = 0;
    int fdigits = 0;
    bool neg = false, any = false, in_frac = false;
    const char *p = text;
    while (*p == ' ') p++;
    if (*p == '-' || *p == '+') {
        neg = *p == '-';
        p++;
    }
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            any = true;
            if (in_frac) {
                if (fdigits < dec) {
                    fp = fp * 10 + (*p - '0');
                    fdigits++;
                }
            } else {
                if (ip > 100000) return false;
                ip = ip * 10 + (*p - '0');
            }
        } else if ((*p == ',' || *p == '.') && !in_frac) {
            in_frac = true;
        } else if (*p == ' ') {
            break;
        } else {
            return false;
        }
    }
    if (!any) return false;
    while (fdigits < dec) {
        fp *= 10;
        fdigits++;
    }
    int32_t scale = 1;
    for (int i = 0; i < dec; i++) scale *= 10;
    int32_t v = ip * scale + fp;
    *out = neg ? -v : v;
    return true;
}
