/**
 * @file clock.c
 * @brief Time, date, and battery presentation for //GRID.
 */
#include "grid.h"

#ifdef PBL_PLATFORM_EMERY
#define TIME_FONT_24H       FONT_KEY_LECO_60_NUMBERS_AM_PM
#define TIME_FONT_12H       FONT_KEY_LECO_42_NUMBERS
#define TIME_DATE_FONT      FONT_KEY_GOTHIC_24_BOLD
#define TIME_FONT_COLOR     GColorCyan
#define TIME_DATE_COLOR     GColorTiffanyBlue
#define BATTERY_OK_COLOR    GColorCyan
#define BATTERY_30_COLOR    GColorGreen
#define BATTERY_20_COLOR    GColorYellow
#define BATTERY_15_COLOR    GColorOrange
#define BATTERY_10_COLOR    GColorRed
#define BATTERY_EMPTY_COLOR GColorDarkGray
#else
#define TIME_FONT_24H       FONT_KEY_LECO_32_BOLD_NUMBERS
#define TIME_FONT_12H       FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM
#define TIME_DATE_FONT      FONT_KEY_GOTHIC_18_BOLD
#define TIME_FONT_COLOR     GColorWhite
#define TIME_DATE_COLOR     GColorWhite
#define BATTERY_OK_COLOR    GColorWhite
#define BATTERY_30_COLOR    GColorWhite
#define BATTERY_20_COLOR    GColorWhite
#define BATTERY_15_COLOR    GColorWhite
#define BATTERY_10_COLOR    GColorWhite
#define BATTERY_EMPTY_COLOR GColorWhite
#endif

#define TIME_LAYER_BG_COLOR GColorClear
#define BATTERY_PCT_FONT    FONT_KEY_GOTHIC_14
#define BATTERY_SEGMENTS    10
#define BATTERY_SEGMENT_GAP 2

static GColor battery_color_for_pct(int pct)
{
    if (pct <= 10)
        return BATTERY_10_COLOR;
    if (pct <= 15)
        return BATTERY_15_COLOR;
    if (pct <= 20)
        return BATTERY_20_COLOR;
    if (pct <= 30)
        return BATTERY_30_COLOR;

    return BATTERY_OK_COLOR;
}

static int battery_filled_segments(int pct)
{
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;

    if (pct <= 10)
        return pct;

    return (BATTERY_SEGMENTS * pct) / 100;
}

static void draw_dim_battery_segment(GContext *ctx, GRect segment, GColor color)
{
    graphics_context_set_stroke_color(ctx, color);

    for (int y = segment.origin.y; y < segment.origin.y + segment.size.h; y++) {
        for (int x = segment.origin.x; x < segment.origin.x + segment.size.w; x++) {
            if ((x + y) % 2 == 0)
                graphics_draw_pixel(ctx, GPoint(x, y));
        }
    }
}

static void draw_date(const Clock *self, GContext *ctx)
{
    GFont font = fonts_get_system_font(TIME_DATE_FONT);

    graphics_context_set_text_color(ctx, TIME_DATE_COLOR);
    graphics_draw_text(ctx, self->date_buf, font, self->geometry->date, GTextOverflowModeWordWrap, GTextAlignmentCenter,
                       NULL);
}

static void draw_battery_bar(const Clock *self, GContext *ctx)
{
    GRect area   = self->geometry->battery_bar;
    int   seg_w  = (area.size.w - (BATTERY_SEGMENTS - 1) * BATTERY_SEGMENT_GAP) / BATTERY_SEGMENTS;
    int   filled = battery_filled_segments(self->battery_pct);
    bool  has_partial =
        self->battery_pct > 10 && self->battery_pct < 100 && (BATTERY_SEGMENTS * self->battery_pct) % 100 != 0;
    GColor color = battery_color_for_pct(self->battery_pct);

    for (int i = 0; i < BATTERY_SEGMENTS; i++) {
        int   x   = area.origin.x + i * (seg_w + BATTERY_SEGMENT_GAP);
        GRect seg = GRect(x, area.origin.y, seg_w, area.size.h);

        if (i < filled) {
            graphics_context_set_fill_color(ctx, color);
            graphics_fill_rect(ctx, seg, 0, GCornerNone);
        } else if (has_partial && i == filled) {
            draw_dim_battery_segment(ctx, seg, color);
        } else {
            graphics_context_set_stroke_color(ctx, BATTERY_EMPTY_COLOR);
            graphics_context_set_stroke_width(ctx, 1);
            graphics_draw_rect(ctx, seg);
        }
    }
}

static void draw_battery_label(const Clock *self, GContext *ctx)
{
    GColor color  = battery_color_for_pct(self->battery_pct);
    GFont  font   = fonts_get_system_font(BATTERY_PCT_FONT);
    char   buf[8] = {0};

    snprintf(buf, sizeof(buf), "%3d%%", self->battery_pct);

    graphics_context_set_text_color(ctx, color);
    graphics_draw_text(ctx, buf, font, self->geometry->battery_label, GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
}

static void update_time_display(Clock *self, const struct tm *time_info)
{
    if (!self || !self->time_layer || !time_info)
        return;

    if (clock_is_24h_style()) {
        strftime(self->time_buf, sizeof(self->time_buf), "%H:%M", time_info);
        text_layer_set_font(self->time_layer, fonts_get_system_font(TIME_FONT_24H));
    } else {
        char tmp[12] = {0};

        strftime(tmp, sizeof(tmp), "%I:%M %p", time_info);

        if (tmp[0] == '0')
            strcpy(self->time_buf, tmp + 1);
        else
            strcpy(self->time_buf, tmp);

        text_layer_set_font(self->time_layer, fonts_get_system_font(TIME_FONT_12H));
    }

    text_layer_set_text(self->time_layer, self->time_buf);
}

void clock_refresh_details(Clock *self)
{
    if (!self)
        return;

    time_t     now       = time(NULL);
    struct tm *time_info = localtime(&now);

    if (time_info) {
        strftime(self->date_buf, sizeof(self->date_buf), "[%a %d %b]", time_info);

        for (char *p = self->date_buf; *p; p++) {
            if (*p >= 'a' && *p <= 'z')
                *p = (char)(*p - 32);
        }
    }

    self->battery_pct = battery_state_service_peek().charge_percent;
}

static void details_layer_update_proc(Layer *layer, GContext *ctx)
{
    Clock *self = *(Clock **)layer_get_data(layer);

    draw_date(self, ctx);
    draw_battery_bar(self, ctx);
    draw_battery_label(self, ctx);
}

static void tick_handler(struct tm *tick_time, __attribute__((__unused__)) TimeUnits units_changed)
{
    App *app = app_from_active_window();

    if (!app)
        return;

    update_time_display(&app->clock, tick_time);
    weather_tick(&app->weather, &app->settings.value, time(NULL));
}

bool clock_init(Clock *self, Layer *root, Layer *details, const ScreenGeometry *geometry)
{
    if (!self || !root || !details || !geometry)
        return false;

    self->geometry   = geometry;
    self->time_layer = text_layer_create(geometry->time);

    if (!self->time_layer)
        goto fail;

    text_layer_set_background_color(self->time_layer, TIME_LAYER_BG_COLOR);
    text_layer_set_text_color(self->time_layer, TIME_FONT_COLOR);
    text_layer_set_text_alignment(self->time_layer, GTextAlignmentCenter);
    layer_add_child(root, text_layer_get_layer(self->time_layer));

    self->details_layer = layer_create_with_data(layer_get_bounds(details), sizeof(Clock *));

    if (!self->details_layer)
        goto fail;

    *(Clock **)layer_get_data(self->details_layer) = self;
    layer_set_update_proc(self->details_layer, details_layer_update_proc);
    layer_add_child(details, self->details_layer);

    time_t     now       = time(NULL);
    struct tm *time_info = localtime(&now);

    if (time_info)
        update_time_display(self, time_info);

    clock_refresh_details(self);
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    self->tick_subscribed = true;

    return true;

fail:
    clock_deinit(self);
    return false;
}

void clock_deinit(Clock *self)
{
    if (!self)
        return;

    bool       tick_subscribed = self->tick_subscribed;
    Layer     *details_layer   = self->details_layer;
    TextLayer *time_layer      = self->time_layer;

    self->tick_subscribed = false;
    self->details_layer   = NULL;
    self->time_layer      = NULL;
    self->geometry        = NULL;

    if (tick_subscribed)
        tick_timer_service_unsubscribe();
    if (details_layer)
        layer_destroy(details_layer);
    if (time_layer)
        text_layer_destroy(time_layer);
}
