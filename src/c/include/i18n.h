#ifndef GRID_I18N_H
#define GRID_I18N_H

#include "grid.h"

typedef struct {
    const char *text[GRID_TEXT_COUNT];
    const char *weekday[7];
    const char *month[12];
} GridLocale;

#endif // GRID_I18N_H
