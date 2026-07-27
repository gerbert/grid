/**
 * @file weather.c
 * @brief In-memory weather forecast synchronized through PebbleKit JS.
 */
#include "grid.h"

#ifdef PBL_PLATFORM_EMERY
#define WEATHER_FONT       FONT_KEY_GOTHIC_18_BOLD
#define WEATHER_TEXT_COLOR GColorCyan
#define WEATHER_ICON_GAP   4
#else
#define WEATHER_FONT       FONT_KEY_GOTHIC_14_BOLD
#define WEATHER_TEXT_COLOR GColorWhite
#define WEATHER_ICON_GAP   3
#endif

static uint8_t weather_refresh_hours(const Settings *settings)
{
    uint8_t interval = settings ? settings->weather.refresh_hrs : 0;

    if (interval < GRID_WEATHER_MIN_UPDATE_INTERVAL_HOURS || interval > GRID_WEATHER_MAX_UPDATE_INTERVAL_HOURS)
        return GRID_WEATHER_DEFAULT_UPDATE_INTERVAL_HOURS;

    return interval;
}

static uint8_t weather_retry_minutes(const Settings *settings)
{
    uint8_t interval = settings ? settings->weather.retry_min : 0;

    if (interval < GRID_WEATHER_MIN_RETRY_INTERVAL_MINUTES || interval > GRID_WEATHER_MAX_RETRY_INTERVAL_MINUTES)
        return GRID_WEATHER_DEFAULT_RETRY_INTERVAL_MINUTES;

    return interval;
}

static bool weather_condition_is_valid(uint8_t condition_id) { return condition_id < WEATHER_CONDITION_COUNT; }

static const char *weather_condition_text(uint8_t condition_id)
{
    switch ((WeatherCondition)condition_id) {
    case WEATHER_CONDITION_CLEAR:
        return "CLEAR";
    case WEATHER_CONDITION_MOSTLY_CLEAR:
        return "MOSTLY CLEAR";
    case WEATHER_CONDITION_PARTLY_CLOUDY:
        return "PARTLY CLOUDY";
    case WEATHER_CONDITION_OVERCAST:
        return "OVERCAST";
    case WEATHER_CONDITION_FOG:
        return "FOG";
    case WEATHER_CONDITION_DRIZZLE:
        return "DRIZZLE";
    case WEATHER_CONDITION_FREEZING_DRIZZLE:
        return "ICY DRIZZLE";
    case WEATHER_CONDITION_RAIN:
        return "RAIN";
    case WEATHER_CONDITION_FREEZING_RAIN:
        return "FREEZING RAIN";
    case WEATHER_CONDITION_SNOW:
        return "SNOW";
    case WEATHER_CONDITION_SNOW_GRAINS:
        return "SNOW GRAINS";
    case WEATHER_CONDITION_SHOWERS:
        return "SHOWERS";
    case WEATHER_CONDITION_SNOW_SHOWERS:
        return "SNOW SHOWERS";
    case WEATHER_CONDITION_THUNDERSTORM:
        return "THUNDERSTORM";
    case WEATHER_CONDITION_HAIL_STORM:
        return "HAIL STORM";
    case WEATHER_CONDITION_UNKNOWN:
    case WEATHER_CONDITION_COUNT:
        return "WEATHER";
    }

    return "WEATHER";
}

static const char *weather_condition_glyph(uint8_t condition_id)
{
    static const char *const glyphs[WEATHER_CONDITION_COUNT] = {
        "\xEF\x81\xBB", // U+F07B: wi-na
        "\xEF\x80\x8D", // U+F00D: wi-day-sunny
        "\xEF\x80\x8C", // U+F00C: wi-day-sunny-overcast
        "\xEF\x80\x82", // U+F002: wi-day-cloudy
        "\xEF\x80\x93", // U+F013: wi-cloudy
        "\xEF\x80\x94", // U+F014: wi-fog
        "\xEF\x80\x9C", // U+F01C: wi-sprinkle
        "\xEF\x82\xB5", // U+F0B5: wi-sleet
        "\xEF\x80\x99", // U+F019: wi-rain
        "\xEF\x80\x97", // U+F017: wi-rain-mix
        "\xEF\x80\x9B", // U+F01B: wi-snow
        "\xEF\x81\xB6", // U+F076: wi-snowflake-cold
        "\xEF\x80\x9A", // U+F01A: wi-showers
        "\xEF\x81\xA4", // U+F064: wi-snow-wind
        "\xEF\x80\x9E", // U+F01E: wi-thunderstorm
        "\xEF\x80\x95", // U+F015: wi-hail
    };

    if (!weather_condition_is_valid(condition_id))
        condition_id = WEATHER_CONDITION_UNKNOWN;

    return glyphs[condition_id];
}

static void weather_release_icon_font(Weather *self)
{
    if (!self || !self->icon_font)
        return;

    fonts_unload_custom_font(self->icon_font);
    self->icon_font = NULL;
}

static GFont weather_get_icon_font(Weather *self)
{
    if (!self)
        return NULL;

    if (self->icon_font)
        return self->icon_font;

#ifdef PBL_PLATFORM_EMERY
    self->icon_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_WEATHER_ICONS_24));
#else
    self->icon_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_WEATHER_ICONS_18));
#endif

    return self->icon_font;
}

static const WeatherSlot *weather_current_slot(const Weather *self, time_t now)
{
    if (!self || self->count == 0 || self->count > WEATHER_SLOT_COUNT || self->interval_minutes == 0)
        return NULL;

    if (now <= self->start_at)
        return &self->slots[0];

    time_t   interval_seconds = (time_t)self->interval_minutes * SECONDS_PER_MINUTE;
    time_t   elapsed          = now - self->start_at;
    uint32_t index            = (uint32_t)(elapsed / interval_seconds);

    if (index >= self->count)
        index = self->count - 1;

    return &self->slots[index];
}

static void weather_draw_text(const WeatherSlot *slot, GRect bounds, GContext *ctx)
{
    char value[40];

    snprintf(value, sizeof(value),
             "%d\xC2\xB0"
             "C  %s",
             (int)slot->temperature_c, weather_condition_text(slot->condition_id));

    graphics_context_set_text_color(ctx, WEATHER_TEXT_COLOR);
    graphics_draw_text(ctx, value, fonts_get_system_font(WEATHER_FONT), bounds, GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
}

static void weather_draw_icon(Weather *self, const WeatherSlot *slot, GRect bounds, GContext *ctx)
{
    GFont icon_font = weather_get_icon_font(self);

    if (!icon_font) {
        weather_draw_text(slot, bounds, ctx);
        return;
    }

    char        temperature[12];
    const char *glyph            = weather_condition_glyph(slot->condition_id);
    GFont       temperature_font = fonts_get_system_font(WEATHER_FONT);

    snprintf(temperature, sizeof(temperature),
             "%d\xC2\xB0"
             "C",
             (int)slot->temperature_c);

    GRect measure_bounds   = GRect(0, 0, bounds.size.w, 48);
    GSize temperature_size = graphics_text_layout_get_content_size(
        temperature, temperature_font, measure_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    GSize glyph_size = graphics_text_layout_get_content_size(glyph, icon_font, measure_bounds,
                                                             GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    int   temperature_width = temperature_size.w + 2;
    int   glyph_width       = glyph_size.w + 2;
    int   total_width       = temperature_width + WEATHER_ICON_GAP + glyph_width;
    int   left              = bounds.origin.x + (bounds.size.w - total_width) / 2;
    int   glyph_y           = bounds.origin.y + (bounds.size.h - glyph_size.h) / 2;

    graphics_context_set_text_color(ctx, WEATHER_TEXT_COLOR);
    graphics_draw_text(ctx, temperature, temperature_font,
                       GRect(left, bounds.origin.y, temperature_width, bounds.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, glyph, icon_font,
                       GRect(left + temperature_width + WEATHER_ICON_GAP, glyph_y, glyph_width, glyph_size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void weather_layer_update_proc(Layer *layer, GContext *ctx)
{
    Weather *self = *(Weather **)layer_get_data(layer);

    if (!self)
        return;

    const WeatherSlot *slot = weather_current_slot(self, time(NULL));

    if (!slot)
        return;

    GRect bounds = layer_get_bounds(layer);

    if (self->settings && self->settings->weather.display_mode == WEATHER_DISPLAY_ICON)
        weather_draw_icon(self, slot, bounds, ctx);
    else
        weather_draw_text(slot, bounds, ctx);
}

static void weather_clear(Weather *self)
{
    if (!self)
        return;

    weather_release_icon_font(self);
    memset(self->slots, 0, sizeof(self->slots));
    self->start_at         = 0;
    self->next_update_at   = 0;
    self->interval_minutes = 0;
    self->count            = 0;
    self->in_progress      = false;
    self->refresh_pending  = false;

    if (self->layer)
        layer_set_hidden(self->layer, true);
}

static void weather_schedule_retry(Weather *self, const Settings *settings)
{
    if (!self)
        return;

    time_t delay =
        self->refresh_pending ? SECONDS_PER_MINUTE : (time_t)weather_retry_minutes(settings) * SECONDS_PER_MINUTE;

    self->next_update_at  = time(NULL) + delay;
    self->in_progress     = false;
    self->refresh_pending = false;
}

static bool weather_request_update(Weather *self, const Settings *settings)
{
    if (!self || !settings || !settings->weather.enabled || self->in_progress)
        return false;

    DictionaryIterator *iterator = NULL;

    if (app_message_outbox_begin(&iterator) != APP_MSG_OK || !iterator) {
        weather_schedule_retry(self, settings);
        return false;
    }

    if (dict_write_uint8(iterator, MESSAGE_KEY_WEATHER_REQUEST, settings->weather.provider_id) != DICT_OK) {
        weather_schedule_retry(self, settings);
        return false;
    }

    if (app_message_outbox_send() != APP_MSG_OK) {
        weather_schedule_retry(self, settings);
        return false;
    }

    self->in_progress    = true;
    self->next_update_at = time(NULL) + (time_t)GRID_WEATHER_REQUEST_TIMEOUT_MINUTES * SECONDS_PER_MINUTE;
    return true;
}

static uint16_t weather_read_uint16(const uint8_t *value)
{
    return (uint16_t)value[0] | (uint16_t)((uint16_t)value[1] << 8);
}

static uint32_t weather_read_uint32(const uint8_t *value)
{
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8) | ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

static bool weather_apply_forecast(Weather *self, const Settings *settings, const Tuple *tuple)
{
    if (!self || !settings || !tuple || tuple->type != TUPLE_BYTE_ARRAY || tuple->length < WEATHER_FORECAST_HEADER_SIZE)
        return false;

    const uint8_t *payload          = tuple->value->data;
    uint8_t        count            = payload[0];
    uint16_t       interval_minutes = weather_read_uint16(payload + 1);
    uint32_t       start_at         = weather_read_uint32(payload + 3);
    uint16_t       expected_length  = WEATHER_FORECAST_HEADER_SIZE + count * WEATHER_FORECAST_SLOT_SIZE;

    if (count == 0 || count > WEATHER_SLOT_COUNT || interval_minutes == 0 || tuple->length != expected_length)
        return false;

    for (uint8_t i = 0; i < count; i++) {
        const uint8_t *slot_payload = payload + WEATHER_FORECAST_HEADER_SIZE + i * WEATHER_FORECAST_SLOT_SIZE;

        if (!weather_condition_is_valid(slot_payload[1]))
            return false;
    }

    memset(self->slots, 0, sizeof(self->slots));

    for (uint8_t i = 0; i < count; i++) {
        const uint8_t *slot_payload = payload + WEATHER_FORECAST_HEADER_SIZE + i * WEATHER_FORECAST_SLOT_SIZE;

        self->slots[i].temperature_c = (int8_t)slot_payload[0];
        self->slots[i].condition_id  = slot_payload[1];
    }

    time_t delay =
        self->refresh_pending ? SECONDS_PER_MINUTE : (time_t)weather_refresh_hours(settings) * SECONDS_PER_HOUR;

    self->count            = count;
    self->interval_minutes = interval_minutes;
    self->start_at         = (time_t)start_at;
    self->next_update_at   = time(NULL) + delay;
    self->in_progress      = false;
    self->refresh_pending  = false;

    weather_refresh_display(self);
    return true;
}

bool weather_init(Weather *self, Layer *details, const ScreenGeometry *geometry, const Settings *settings)
{
    if (!self || !details || !geometry || !settings)
        return false;

    memset(self, 0, sizeof(*self));
    self->settings = settings;
    self->layer    = layer_create_with_data(geometry->weather, sizeof(Weather *));

    if (!self->layer)
        return false;

    *(Weather **)layer_get_data(self->layer) = self;
    layer_set_update_proc(self->layer, weather_layer_update_proc);
    layer_set_hidden(self->layer, true);
    layer_add_child(details, self->layer);

    return true;
}

void weather_refresh_display(Weather *self)
{
    if (!self || !self->layer)
        return;

    if (!self->settings || !self->settings->weather.enabled || self->count == 0) {
        weather_release_icon_font(self);
        layer_set_hidden(self->layer, true);
        return;
    }

    if (self->settings->weather.display_mode != WEATHER_DISPLAY_ICON)
        weather_release_icon_font(self);

    layer_set_hidden(self->layer, false);
    layer_mark_dirty(self->layer);
}

void weather_schedule_refresh(Weather *self)
{
    if (!self)
        return;

    if (self->in_progress) {
        self->refresh_pending = true;
        return;
    }

    self->refresh_pending = false;
    self->next_update_at  = time(NULL) + SECONDS_PER_MINUTE;
}

void weather_tick(Weather *self, const Settings *settings, time_t now)
{
    if (!self || !settings)
        return;

    if (!settings->weather.enabled) {
        if (self->count || self->in_progress || self->refresh_pending || self->next_update_at)
            weather_clear(self);

        return;
    }

    if (self->next_update_at == 0)
        self->next_update_at = now;

    if (self->count > 0 && self->layer && self->interval_minutes > 0 && now >= self->start_at) {
        time_t interval_seconds = (time_t)self->interval_minutes * SECONDS_PER_MINUTE;

        if ((now - self->start_at) % interval_seconds < SECONDS_PER_MINUTE)
            layer_mark_dirty(self->layer);
    }

    if (self->in_progress) {
        if (self->next_update_at <= now)
            weather_schedule_retry(self, settings);

        return;
    }

    if (self->next_update_at > now)
        return;

    weather_request_update(self, settings);
}

void weather_handle_message(Weather *self, const Settings *settings, DictionaryIterator *iterator)
{
    if (!self || !settings || !iterator || !settings->weather.enabled)
        return;

    Tuple *forecast = dict_find(iterator, MESSAGE_KEY_WEATHER_FORECAST);

    if (forecast) {
        if (!weather_apply_forecast(self, settings, forecast))
            weather_schedule_retry(self, settings);
        return;
    }

    if (dict_find(iterator, MESSAGE_KEY_WEATHER_UPDATE_FAILED))
        weather_update_failed(self, settings);
}

void weather_update_failed(Weather *self, const Settings *settings)
{
    if (!self || !self->in_progress)
        return;

    weather_schedule_retry(self, settings);
}

void weather_deinit(Weather *self)
{
    if (!self)
        return;

    Layer *layer = self->layer;

    self->layer    = NULL;
    self->settings = NULL;
    weather_clear(self);

    if (layer)
        layer_destroy(layer);
}
