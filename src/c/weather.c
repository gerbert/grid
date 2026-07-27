/**
 * @file weather.c
 * @brief In-memory weather forecast synchronized through PebbleKit JS.
 */
#include "grid.h"

#ifdef PBL_PLATFORM_EMERY
#define WEATHER_FONT         FONT_KEY_GOTHIC_18_BOLD
#define WEATHER_TEXT_COLOR   GColorCyan
#define WEATHER_ICON_GAP     4
#define WEATHER_COMBINED_GAP 4
#else
#define WEATHER_FONT         FONT_KEY_GOTHIC_14_BOLD
#define WEATHER_TEXT_COLOR   GColorWhite
#define WEATHER_ICON_GAP     3
#define WEATHER_COMBINED_GAP 3
#endif

#define WEATHER_CONDITION_FONT         FONT_KEY_GOTHIC_14
#define WEATHER_HORIZONTAL_MARGIN      4
#define WEATHER_LAYER_VERTICAL_PADDING 6
#define WEATHER_CONDITION_LINE_HEIGHT  16
#define WEATHER_CONDITION_LINE_STEP    11
#define WEATHER_TEXT_MEASURE_WIDTH     240
#define WEATHER_TEXT_MEASURE_HEIGHT    48
#define WEATHER_CONDITION_BUFFER_SIZE  24

typedef struct {
    GFont       temperature_font;
    GFont       icon_font;
    GSize       temperature_size;
    GSize       glyph_size;
    char        temperature[12];
    const char *glyph;
    int         temperature_width;
    int         glyph_width;
    int         total_width;
} WeatherIconLayout;

typedef struct {
    char first[WEATHER_CONDITION_BUFFER_SIZE];
    char second[WEATHER_CONDITION_BUFFER_SIZE];
    int  line_count;
    int  width;
} WeatherConditionLayout;

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
        return "THUNDER STORM";
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

static GSize weather_measure_text(const char *text, GFont font, int max_width)
{
    if (!text || !font || max_width <= 0)
        return GSize(0, 0);

    return graphics_text_layout_get_content_size(text, font, GRect(0, 0, max_width, WEATHER_TEXT_MEASURE_HEIGHT),
                                                 GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
}

static int weather_content_width(GSize size, int max_width)
{
    int width = size.w + 2;

    return width < max_width ? width : max_width;
}

static void weather_format_temperature(const WeatherSlot *slot, char *buffer, size_t buffer_size)
{
    snprintf(buffer, buffer_size,
             "%d\xC2\xB0"
             "C",
             (int)slot->temperature_c);
}

static void weather_draw_centered_text(const char *text, GFont font, GRect bounds, GContext *ctx)
{
    int   max_width = bounds.size.w - 2 * WEATHER_HORIZONTAL_MARGIN;
    GSize size      = weather_measure_text(text, font, max_width);
    int   width     = weather_content_width(size, max_width);
    int   left      = bounds.origin.x + (bounds.size.w - width) / 2;
    int   top       = bounds.origin.y + (bounds.size.h - size.h) / 2;

    graphics_context_set_text_color(ctx, WEATHER_TEXT_COLOR);
    graphics_draw_text(ctx, text, font, GRect(left, top, width, size.h), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
}

static void weather_draw_temperature(const WeatherSlot *slot, GRect bounds, GContext *ctx)
{
    char temperature[12];

    weather_format_temperature(slot, temperature, sizeof(temperature));
    weather_draw_centered_text(temperature, fonts_get_system_font(WEATHER_FONT), bounds, ctx);
}

static void weather_draw_text(const WeatherSlot *slot, GRect bounds, GContext *ctx)
{
    char value[40];

    snprintf(value, sizeof(value),
             "%d\xC2\xB0"
             "C  %s",
             (int)slot->temperature_c, weather_condition_text(slot->condition_id));

    weather_draw_centered_text(value, fonts_get_system_font(WEATHER_FONT), bounds, ctx);
}

static bool weather_get_icon_layout(Weather *self, const WeatherSlot *slot, int max_width, WeatherIconLayout *layout)
{
    if (!self || !slot || !layout || max_width <= 0)
        return false;

    memset(layout, 0, sizeof(*layout));
    layout->icon_font = weather_get_icon_font(self);

    if (!layout->icon_font)
        return false;

    layout->temperature_font = fonts_get_system_font(WEATHER_FONT);
    layout->glyph            = weather_condition_glyph(slot->condition_id);
    weather_format_temperature(slot, layout->temperature, sizeof(layout->temperature));

    layout->temperature_size  = weather_measure_text(layout->temperature, layout->temperature_font, max_width);
    layout->glyph_size        = weather_measure_text(layout->glyph, layout->icon_font, max_width);
    layout->temperature_width = weather_content_width(layout->temperature_size, max_width);
    layout->glyph_width       = weather_content_width(layout->glyph_size, max_width);
    layout->total_width       = layout->temperature_width + WEATHER_ICON_GAP + layout->glyph_width;

    return true;
}

static void weather_draw_icon_layout(const WeatherIconLayout *layout, int left, GRect bounds, GContext *ctx)
{
    int temperature_y = bounds.origin.y + (bounds.size.h - layout->temperature_size.h) / 2;
    int glyph_y       = bounds.origin.y + (bounds.size.h - layout->glyph_size.h) / 2;

    graphics_context_set_text_color(ctx, WEATHER_TEXT_COLOR);
    graphics_draw_text(ctx, layout->temperature, layout->temperature_font,
                       GRect(left, temperature_y, layout->temperature_width, layout->temperature_size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(
        ctx, layout->glyph, layout->icon_font,
        GRect(left + layout->temperature_width + WEATHER_ICON_GAP, glyph_y, layout->glyph_width, layout->glyph_size.h),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void weather_draw_icon(Weather *self, const WeatherSlot *slot, GRect bounds, GContext *ctx)
{
    WeatherIconLayout layout;
    int               max_width = bounds.size.w - 2 * WEATHER_HORIZONTAL_MARGIN;

    if (!weather_get_icon_layout(self, slot, max_width, &layout)) {
        weather_draw_text(slot, bounds, ctx);
        return;
    }

    int left = bounds.origin.x + (bounds.size.w - layout.total_width) / 2;

    weather_draw_icon_layout(&layout, left, bounds, ctx);
}

static int weather_measure_condition_width(const char *text, GFont font)
{
    GSize size = weather_measure_text(text, font, WEATHER_TEXT_MEASURE_WIDTH);

    return size.w + 2;
}

static void weather_copy_condition_part(char *destination, size_t destination_size, const char *source, size_t length)
{
    if (!destination || destination_size == 0)
        return;

    if (length >= destination_size)
        length = destination_size - 1;

    memcpy(destination, source, length);
    destination[length] = '\0';
}

static void weather_layout_condition(const char *text, GFont font, int max_width, WeatherConditionLayout *layout)
{
    int         full_width;
    int         best_width = 0;
    const char *best_split = NULL;

    memset(layout, 0, sizeof(*layout));
    full_width = weather_measure_condition_width(text, font);

    if (full_width <= max_width) {
        snprintf(layout->first, sizeof(layout->first), "%s", text);
        layout->line_count = 1;
        layout->width      = full_width;
        return;
    }

    for (const char *split = strchr(text, ' '); split; split = strchr(split + 1, ' ')) {
        char first[WEATHER_CONDITION_BUFFER_SIZE];
        char second[WEATHER_CONDITION_BUFFER_SIZE];
        int  first_width;
        int  second_width;
        int  candidate_width;

        weather_copy_condition_part(first, sizeof(first), text, (size_t)(split - text));
        snprintf(second, sizeof(second), "%s", split + 1);
        first_width     = weather_measure_condition_width(first, font);
        second_width    = weather_measure_condition_width(second, font);
        candidate_width = first_width > second_width ? first_width : second_width;

        if (!best_split || candidate_width < best_width) {
            best_split = split;
            best_width = candidate_width;
        }
    }

    if (!best_split) {
        snprintf(layout->first, sizeof(layout->first), "%s", text);
        layout->line_count = 1;
        layout->width      = max_width;
        return;
    }

    weather_copy_condition_part(layout->first, sizeof(layout->first), text, (size_t)(best_split - text));
    snprintf(layout->second, sizeof(layout->second), "%s", best_split + 1);
    layout->line_count = 2;
    layout->width      = best_width < max_width ? best_width : max_width;
}

static void weather_draw_condition(const WeatherConditionLayout *layout, GFont font, GRect bounds, GContext *ctx)
{
    graphics_context_set_text_color(ctx, WEATHER_TEXT_COLOR);

    if (layout->line_count == 1) {
        GSize size = weather_measure_text(layout->first, font, layout->width);
        int   top  = bounds.origin.y + (bounds.size.h - size.h) / 2;

        graphics_draw_text(ctx, layout->first, font, GRect(bounds.origin.x, top, bounds.size.w, size.h),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        return;
    }

    int total_height = WEATHER_CONDITION_LINE_HEIGHT + WEATHER_CONDITION_LINE_STEP;
    int top          = bounds.origin.y + (bounds.size.h - total_height) / 2;

    graphics_draw_text(ctx, layout->first, font,
                       GRect(bounds.origin.x, top, bounds.size.w, WEATHER_CONDITION_LINE_HEIGHT),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(
        ctx, layout->second, font,
        GRect(bounds.origin.x, top + WEATHER_CONDITION_LINE_STEP, bounds.size.w, WEATHER_CONDITION_LINE_HEIGHT),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void weather_draw_icon_text(Weather *self, const WeatherSlot *slot, GRect bounds, GContext *ctx)
{
    WeatherIconLayout      icon_layout;
    WeatherConditionLayout condition_layout;
    int                    max_row_width = bounds.size.w - 2 * WEATHER_HORIZONTAL_MARGIN;

    if (!weather_get_icon_layout(self, slot, max_row_width, &icon_layout)) {
        weather_draw_text(slot, bounds, ctx);
        return;
    }

    int condition_max_width = max_row_width - icon_layout.total_width - WEATHER_COMBINED_GAP;

    if (condition_max_width <= 0) {
        weather_draw_icon(self, slot, bounds, ctx);
        return;
    }

    GFont condition_font = fonts_get_system_font(WEATHER_CONDITION_FONT);

    weather_layout_condition(weather_condition_text(slot->condition_id), condition_font, condition_max_width,
                             &condition_layout);

    int   total_width      = icon_layout.total_width + WEATHER_COMBINED_GAP + condition_layout.width;
    int   left             = bounds.origin.x + (bounds.size.w - total_width) / 2;
    GRect condition_bounds = GRect(left + icon_layout.total_width + WEATHER_COMBINED_GAP, bounds.origin.y,
                                   condition_layout.width, bounds.size.h);

    weather_draw_icon_layout(&icon_layout, left, bounds, ctx);
    weather_draw_condition(&condition_layout, condition_font, condition_bounds, ctx);
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
    else if (self->settings && self->settings->weather.display_mode == WEATHER_DISPLAY_ICON_TEXT)
        weather_draw_icon_text(self, slot, bounds, ctx);
    else if (self->settings && self->settings->weather.display_mode == WEATHER_DISPLAY_TEMPERATURE)
        weather_draw_temperature(slot, bounds, ctx);
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

    time_t now = time(NULL);

    // A queued refresh is already due and will run on the next minute tick.
    if (self->refresh_pending)
        self->next_update_at = now;
    else
        self->next_update_at = now + (time_t)weather_retry_minutes(settings) * SECONDS_PER_MINUTE;
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

    time_t now = time(NULL);

    self->count            = count;
    self->interval_minutes = interval_minutes;
    self->start_at         = (time_t)start_at;

    if (self->refresh_pending)
        self->next_update_at = now;
    else
        self->next_update_at = now + (time_t)weather_refresh_hours(settings) * SECONDS_PER_HOUR;

    self->in_progress     = false;
    self->refresh_pending = false;

    weather_refresh_display(self);
    return true;
}

bool weather_init(Weather *self, Layer *details, const ScreenGeometry *geometry, const Settings *settings)
{
    if (!self || !details || !geometry || !settings)
        return false;

    GRect layer_frame = geometry->weather;

    memset(self, 0, sizeof(*self));
    self->settings = settings;
    layer_frame.origin.y -= WEATHER_LAYER_VERTICAL_PADDING;
    layer_frame.size.h += 2 * WEATHER_LAYER_VERTICAL_PADDING;
    self->layer = layer_create_with_data(layer_frame, sizeof(Weather *));

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

    if (self->settings->weather.display_mode != WEATHER_DISPLAY_ICON &&
        self->settings->weather.display_mode != WEATHER_DISPLAY_ICON_TEXT)
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

    // weather_tick() runs once per minute, so a due timestamp targets the next tick.
    self->next_update_at = time(NULL);
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
