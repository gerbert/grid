/**
 * @file i18n.c
 * @brief Clock text and date abbreviations for the Pebble system locale.
 */
#include "include/i18n.h"

#include <string.h>

#include "include/locales/en.h"
#include "include/locales/de.h"
#include "include/locales/it.h"
#include "include/locales/es.h"
#include "include/locales/ru.h"
#include "include/locales/fr.h"
#include "include/locales/pt.h"
#include "include/locales/bg.h"
#include "include/locales/ca.h"
#include "include/locales/da.h"
#include "include/locales/ja.h"
#include "include/locales/sv.h"
#include "include/locales/zh.h"
#include "include/locales/he.h"
#include "include/locales/ar.h"
#include "include/locales/my.h"
#include "include/locales/zh_tw.h"
#include "include/locales/nl.h"
#include "include/locales/pl.h"
#include "include/locales/nb.h"
#include "include/locales/uk.h"

static const GridLocale *s_locale = &s_en;

void grid_i18n_init(void)
{
    const char *locale = i18n_get_system_locale();

    s_locale = &s_en;
    if (!locale)
        return;

    if (locale[0] == 'd' && locale[1] == 'e')
        s_locale = &s_de;
    else if (locale[0] == 'i' && locale[1] == 't')
        s_locale = &s_it;
    else if (locale[0] == 'e' && locale[1] == 's')
        s_locale = &s_es;
    else if (locale[0] == 'r' && locale[1] == 'u')
        s_locale = &s_ru;
    else if (locale[0] == 'f' && locale[1] == 'r')
        s_locale = &s_fr;
    else if (locale[0] == 'p' && locale[1] == 't')
        s_locale = &s_pt;
    else if (locale[0] == 'b' && locale[1] == 'g')
        s_locale = &s_bg;
    else if (locale[0] == 'c' && locale[1] == 'a')
        s_locale = &s_ca;
    else if (locale[0] == 'd' && locale[1] == 'a')
        s_locale = &s_da;
    else if (locale[0] == 'j' && locale[1] == 'a')
        s_locale = &s_ja;
    else if (locale[0] == 's' && locale[1] == 'v')
        s_locale = &s_sv;
    else if (strcmp(locale, "zh_CN") == 0 || strcmp(locale, "zh") == 0 ||
             strcmp(locale, "zh_Hans") == 0 || strcmp(locale, "zh_Hans_CN") == 0 ||
             strcmp(locale, "en_CN") == 0)
        s_locale = &s_zh;
    else if (locale[0] == 'h' && locale[1] == 'e')
        s_locale = &s_he;
    else if (locale[0] == 'a' && locale[1] == 'r')
        s_locale = &s_ar;
    else if (locale[0] == 'm' && locale[1] == 'y')
        s_locale = &s_my;
    else if (strcmp(locale, "en_TW") == 0 || strcmp(locale, "zh_TW") == 0 ||
             strcmp(locale, "zh_HK") == 0 || strcmp(locale, "zh_MO") == 0 ||
             strncmp(locale, "zh_Hant", 7) == 0)
        s_locale = &s_zh_tw;
    else if (locale[0] == 'n' && locale[1] == 'l')
        s_locale = &s_nl;
    else if (locale[0] == 'p' && locale[1] == 'l')
        s_locale = &s_pl;
    else if ((locale[0] == 'n' && locale[1] == 'b') ||
             (locale[0] == 'n' && locale[1] == 'o'))
        s_locale = &s_nb;
    else if (locale[0] == 'u' && locale[1] == 'k')
        s_locale = &s_uk;
}

const char *grid_i18n(uint8_t text_id)
{
    if (text_id >= GRID_TEXT_COUNT)
        text_id = WEATHER_CONDITION_UNKNOWN;

    return s_locale->text[text_id] ? s_locale->text[text_id] : s_en.text[text_id];
}

const char *grid_i18n_weekday(int weekday)
{
    if (weekday < 0 || weekday >= 7)
        return s_en.weekday[0];

    return s_locale->weekday[weekday] ? s_locale->weekday[weekday] : s_en.weekday[weekday];
}

const char *grid_i18n_month(int month)
{
    if (month < 0 || month >= 12)
        return s_en.month[0];

    return s_locale->month[month] ? s_locale->month[month] : s_en.month[month];
}
