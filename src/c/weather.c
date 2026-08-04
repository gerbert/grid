/**
 * @file weather.c
 * @brief In-memory weather forecast synchronized through PebbleKit JS.
 */
#include "grid.h"

#ifdef PBL_PLATFORM_EMERY
#define WEATHER_FONT                FONT_KEY_GOTHIC_18_BOLD
#define WEATHER_TEXT_COLOR          GColorCyan
#define WEATHER_ICON_GAP            4
#define WEATHER_COMBINED_GAP        4
#define WEATHER_STATUS_GAP          4
#define WEATHER_FRESH_STATUS_WIDTH  11
#define WEATHER_FRESH_STATUS_HEIGHT 8
#else
#define WEATHER_FONT                FONT_KEY_GOTHIC_14_BOLD
#define WEATHER_TEXT_COLOR          GColorWhite
#define WEATHER_ICON_GAP            3
#define WEATHER_COMBINED_GAP        3
#define WEATHER_STATUS_GAP          3
#define WEATHER_FRESH_STATUS_WIDTH  8
#define WEATHER_FRESH_STATUS_HEIGHT 6
#endif

#define WEATHER_CONDITION_FONT         FONT_KEY_GOTHIC_14
#define WEATHER_HORIZONTAL_MARGIN      4
#define WEATHER_LAYER_VERTICAL_PADDING 6
#define WEATHER_CONDITION_LINE_HEIGHT  16
#define WEATHER_CONDITION_LINE_STEP    11
#define WEATHER_TEXT_MEASURE_WIDTH     240
#define WEATHER_TEXT_MEASURE_HEIGHT    48
#define WEATHER_CONDITION_BUFFER_SIZE  24
#define WEATHER_STALE_STATUS_GLYPH     "\xEF\x80\xBE" // U+F03E: wi-cloud-refresh

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

static uint8_t weather_slot_index(time_t timestamp)
{
    // Unix time is independent of timezone and DST. Consecutive hourly
    // timestamps therefore advance through the 12 slots cyclically.
    return (uint8_t)(((uint32_t)timestamp / SECONDS_PER_HOUR) % WEATHER_SLOT_COUNT);
}

static const WeatherSlot *weather_current_slot(const Weather *self, time_t now)
{
    if (!self || now < 0)
        return NULL;

    return &self->slots[weather_slot_index(now)];
}

static bool weather_slot_is_valid(const Weather *self, time_t now)
{
    const WeatherSlot *slot = weather_current_slot(self, now);

    return slot && slot->valid;
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

static int weather_centered_top(GRect bounds, int height) { return bounds.origin.y + (bounds.size.h - height) / 2; }

static void weather_format_temperature(const WeatherSlot *slot, char *buffer, size_t buffer_size)
{
    snprintf(buffer, buffer_size,
             "%d\xC2\xB0"
             "C",
             (int)slot->temperature_c);
}

static bool weather_status_is_stale(const Weather *self, time_t now)
{
    return self && self->last_update_at != 0 && now > self->last_update_at &&
           now - self->last_update_at >= (time_t)weather_refresh_hours(self->settings) * SECONDS_PER_HOUR;
}

static int weather_get_status_width(Weather *self, time_t now, GFont *icon_font, GSize *glyph_size)
{
    *icon_font  = NULL;
    *glyph_size = GSize(0, 0);

    if (!self || self->last_update_at == 0)
        return 0;

    if (!weather_status_is_stale(self, now))
        return WEATHER_FRESH_STATUS_WIDTH;

    *icon_font = weather_get_icon_font(self);

    if (!*icon_font)
        return 0;

    *glyph_size = weather_measure_text(WEATHER_STALE_STATUS_GLYPH, *icon_font, WEATHER_TEXT_MEASURE_WIDTH);
    return weather_content_width(*glyph_size, WEATHER_TEXT_MEASURE_WIDTH);
}

static int weather_status_group_width(int status_width)
{
    return status_width > 0 ? status_width + WEATHER_STATUS_GAP : 0;
}

static void weather_draw_status(GFont icon_font, GSize glyph_size, int status_width, int content_right, GRect bounds,
                                GContext *ctx)
{
    if (status_width <= 0)
        return;

    GRect status_bounds = GRect(content_right + WEATHER_STATUS_GAP, bounds.origin.y, status_width, bounds.size.h);

    if (!icon_font) {
        int left = status_bounds.origin.x + (status_bounds.size.w - WEATHER_FRESH_STATUS_WIDTH) / 2;
        int top  = weather_centered_top(status_bounds, WEATHER_FRESH_STATUS_HEIGHT);

        graphics_context_set_stroke_color(ctx, WEATHER_TEXT_COLOR);
        graphics_context_set_stroke_width(ctx, 2);
        graphics_draw_line(ctx, GPoint(left, top + WEATHER_FRESH_STATUS_HEIGHT / 2),
                           GPoint(left + WEATHER_FRESH_STATUS_WIDTH / 3, top + WEATHER_FRESH_STATUS_HEIGHT - 1));
        graphics_draw_line(ctx, GPoint(left + WEATHER_FRESH_STATUS_WIDTH / 3, top + WEATHER_FRESH_STATUS_HEIGHT - 1),
                           GPoint(left + WEATHER_FRESH_STATUS_WIDTH - 1, top));
        return;
    }

    int top = weather_centered_top(status_bounds, glyph_size.h);

    graphics_context_set_text_color(ctx, WEATHER_TEXT_COLOR);
    graphics_draw_text(ctx, WEATHER_STALE_STATUS_GLYPH, icon_font,
                       GRect(status_bounds.origin.x, top, status_bounds.size.w, glyph_size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void weather_draw_centered_text(const char *text, GFont font, GRect bounds, int status_width,
                                       GFont status_icon_font, GSize status_glyph_size, GContext *ctx)
{
    int   status_group_width = weather_status_group_width(status_width);
    int   max_width          = bounds.size.w - 2 * WEATHER_HORIZONTAL_MARGIN - status_group_width;
    GSize size               = weather_measure_text(text, font, max_width);
    int   width              = weather_content_width(size, max_width);
    int   left               = bounds.origin.x + (bounds.size.w - width - status_group_width) / 2;
    int   top                = weather_centered_top(bounds, size.h);

    graphics_context_set_text_color(ctx, WEATHER_TEXT_COLOR);
    graphics_draw_text(ctx, text, font, GRect(left, top, width, size.h), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
    weather_draw_status(status_icon_font, status_glyph_size, status_width, left + width, bounds, ctx);
}

static void weather_draw_temperature(const WeatherSlot *slot, GRect bounds, int status_width, GFont status_icon_font,
                                     GSize status_glyph_size, GContext *ctx)
{
    char temperature[12];

    weather_format_temperature(slot, temperature, sizeof(temperature));
    weather_draw_centered_text(temperature, fonts_get_system_font(WEATHER_FONT), bounds, status_width, status_icon_font,
                               status_glyph_size, ctx);
}

static void weather_draw_text(const WeatherSlot *slot, GRect bounds, int status_width, GFont status_icon_font,
                              GSize status_glyph_size, GContext *ctx)
{
    char value[40];

    snprintf(value, sizeof(value),
             "%d\xC2\xB0"
             "C  %s",
             (int)slot->temperature_c, weather_condition_text(slot->condition_id));

    weather_draw_centered_text(value, fonts_get_system_font(WEATHER_FONT), bounds, status_width, status_icon_font,
                               status_glyph_size, ctx);
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
    int temperature_y = weather_centered_top(bounds, layout->temperature_size.h);
    int glyph_y       = weather_centered_top(bounds, layout->glyph_size.h);

    graphics_context_set_text_color(ctx, WEATHER_TEXT_COLOR);
    graphics_draw_text(ctx, layout->temperature, layout->temperature_font,
                       GRect(left, temperature_y, layout->temperature_width, layout->temperature_size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(
        ctx, layout->glyph, layout->icon_font,
        GRect(left + layout->temperature_width + WEATHER_ICON_GAP, glyph_y, layout->glyph_width, layout->glyph_size.h),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void weather_draw_icon(Weather *self, const WeatherSlot *slot, GRect bounds, int status_width,
                              GFont status_icon_font, GSize status_glyph_size, GContext *ctx)
{
    int               status_group_width = weather_status_group_width(status_width);
    int               max_width          = bounds.size.w - 2 * WEATHER_HORIZONTAL_MARGIN - status_group_width;
    WeatherIconLayout layout;

    if (!weather_get_icon_layout(self, slot, max_width, &layout)) {
        weather_draw_text(slot, bounds, status_width, status_icon_font, status_glyph_size, ctx);
        return;
    }

    int left = bounds.origin.x + (bounds.size.w - layout.total_width - status_group_width) / 2;

    weather_draw_icon_layout(&layout, left, bounds, ctx);
    weather_draw_status(status_icon_font, status_glyph_size, status_width, left + layout.total_width, bounds, ctx);
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
        int   top  = weather_centered_top(bounds, size.h);

        graphics_draw_text(ctx, layout->first, font, GRect(bounds.origin.x, top, bounds.size.w, size.h),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        return;
    }

    int total_height = WEATHER_CONDITION_LINE_HEIGHT + WEATHER_CONDITION_LINE_STEP;
    int top          = weather_centered_top(bounds, total_height);

    graphics_draw_text(ctx, layout->first, font,
                       GRect(bounds.origin.x, top, bounds.size.w, WEATHER_CONDITION_LINE_HEIGHT),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(
        ctx, layout->second, font,
        GRect(bounds.origin.x, top + WEATHER_CONDITION_LINE_STEP, bounds.size.w, WEATHER_CONDITION_LINE_HEIGHT),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void weather_draw_icon_text(Weather *self, const WeatherSlot *slot, GRect bounds, int status_width,
                                   GFont status_icon_font, GSize status_glyph_size, GContext *ctx)
{
    int                    status_group_width = weather_status_group_width(status_width);
    int                    max_row_width      = bounds.size.w - 2 * WEATHER_HORIZONTAL_MARGIN - status_group_width;
    WeatherIconLayout      icon_layout;
    WeatherConditionLayout condition_layout;

    if (!weather_get_icon_layout(self, slot, max_row_width, &icon_layout)) {
        weather_draw_text(slot, bounds, status_width, status_icon_font, status_glyph_size, ctx);
        return;
    }

    int condition_max_width = max_row_width - icon_layout.total_width - WEATHER_COMBINED_GAP;

    if (condition_max_width <= 0) {
        int left = bounds.origin.x + (bounds.size.w - icon_layout.total_width - status_group_width) / 2;

        weather_draw_icon_layout(&icon_layout, left, bounds, ctx);
        weather_draw_status(status_icon_font, status_glyph_size, status_width, left + icon_layout.total_width, bounds,
                            ctx);
        return;
    }

    GFont condition_font = fonts_get_system_font(WEATHER_CONDITION_FONT);

    weather_layout_condition(weather_condition_text(slot->condition_id), condition_font, condition_max_width,
                             &condition_layout);

    int   total_width      = icon_layout.total_width + WEATHER_COMBINED_GAP + condition_layout.width;
    int   left             = bounds.origin.x + (bounds.size.w - total_width - status_group_width) / 2;
    GRect condition_bounds = GRect(left + icon_layout.total_width + WEATHER_COMBINED_GAP, bounds.origin.y,
                                   condition_layout.width, bounds.size.h);

    weather_draw_icon_layout(&icon_layout, left, bounds, ctx);
    weather_draw_condition(&condition_layout, condition_font, condition_bounds, ctx);
    weather_draw_status(status_icon_font, status_glyph_size, status_width, left + total_width, bounds, ctx);
}

static void weather_layer_update_proc(Layer *layer, GContext *ctx)
{
    Weather *self = *(Weather **)layer_get_data(layer);

    if (!self)
        return;

    time_t             now  = time(NULL);
    const WeatherSlot *slot = weather_current_slot(self, now);

    if (!slot || !slot->valid)
        return;

    GRect bounds = layer_get_bounds(layer);

    bounds.origin.y += WEATHER_LAYER_VERTICAL_PADDING;
    bounds.size.h -= 2 * WEATHER_LAYER_VERTICAL_PADDING;

    GFont status_icon_font  = NULL;
    GSize status_glyph_size = GSize(0, 0);
    int   status_width      = weather_get_status_width(self, now, &status_icon_font, &status_glyph_size);

    if (self->settings && self->settings->weather.display_mode == WEATHER_DISPLAY_ICON)
        weather_draw_icon(self, slot, bounds, status_width, status_icon_font, status_glyph_size, ctx);
    else if (self->settings && self->settings->weather.display_mode == WEATHER_DISPLAY_ICON_TEXT)
        weather_draw_icon_text(self, slot, bounds, status_width, status_icon_font, status_glyph_size, ctx);
    else if (self->settings && self->settings->weather.display_mode == WEATHER_DISPLAY_TEMPERATURE)
        weather_draw_temperature(slot, bounds, status_width, status_icon_font, status_glyph_size, ctx);
    else
        weather_draw_text(slot, bounds, status_width, status_icon_font, status_glyph_size, ctx);
}

static void weather_clear(Weather *self)
{
    if (!self)
        return;

    weather_release_icon_font(self);
    memset(self->slots, 0, sizeof(self->slots));
    self->next_update_at = 0;

    if (self->layer)
        layer_set_hidden(self->layer, true);
}

static void weather_finish_update(Weather *self, const Settings *settings, time_t now)
{
    if (!self)
        return;

    if (!settings || !settings->weather.enabled) {
        self->next_update_at = 0;
        return;
    }

    if (weather_slot_is_valid(self, now))
        self->next_update_at = now + (time_t)weather_refresh_hours(settings) * SECONDS_PER_HOUR;
    else
        self->next_update_at = now + (time_t)weather_retry_minutes(settings) * SECONDS_PER_MINUTE;
}

static bool weather_request_update(Weather *self, const Settings *settings, time_t now)
{
    if (!self || !settings || !settings->weather.enabled)
        return false;

    // Every request pre-schedules the next retry. A successful response replaces
    // this timestamp with the regular update interval.
    self->next_update_at = now + (time_t)weather_retry_minutes(settings) * SECONDS_PER_MINUTE;

    DictionaryIterator *iterator = NULL;

    if (app_message_outbox_begin(&iterator) != APP_MSG_OK || !iterator)
        return false;

    if (dict_write_uint8(iterator, MESSAGE_KEY_WEATHER_REQUEST, settings->weather.provider_id) != DICT_OK)
        return false;

    if (app_message_outbox_send() != APP_MSG_OK)
        return false;

    return true;
}

static uint32_t weather_read_uint32(const uint8_t *value)
{
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8) | ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

static bool weather_apply_forecast(Weather *self, const Settings *settings, const Tuple *tuple)
{
    if (!self || !settings || !tuple || tuple->type != TUPLE_BYTE_ARRAY || tuple->length < WEATHER_FORECAST_HEADER_SIZE)
        return false;

    const uint8_t *payload         = tuple->value->data;
    uint8_t        count           = payload[0];
    uint32_t       first_timestamp = weather_read_uint32(payload + 1);
    uint16_t       expected_length = WEATHER_FORECAST_HEADER_SIZE + count * WEATHER_FORECAST_SLOT_SIZE;

    if (count == 0 || count > WEATHER_SLOT_COUNT || tuple->length != expected_length ||
        first_timestamp % SECONDS_PER_HOUR != 0)
        return false;

    for (uint8_t i = 0; i < count; i++) {
        const uint8_t *slot_payload = payload + WEATHER_FORECAST_HEADER_SIZE + i * WEATHER_FORECAST_SLOT_SIZE;

        if (!weather_condition_is_valid(slot_payload[1]))
            return false;
    }

    memset(self->slots, 0, sizeof(self->slots));

    for (uint8_t i = 0; i < count; i++) {
        const uint8_t *slot_payload = payload + WEATHER_FORECAST_HEADER_SIZE + i * WEATHER_FORECAST_SLOT_SIZE;
        time_t         timestamp    = (time_t)first_timestamp + (time_t)i * SECONDS_PER_HOUR;
        WeatherSlot   *slot         = &self->slots[weather_slot_index(timestamp)];

        slot->temperature_c = (int8_t)slot_payload[0];
        slot->condition_id  = slot_payload[1];
        slot->valid         = true;
    }

    time_t now           = time(NULL);
    self->last_update_at = now;

    weather_finish_update(self, settings, now);
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
        goto fail;

    *(Weather **)layer_get_data(self->layer) = self;
    layer_set_update_proc(self->layer, weather_layer_update_proc);
    layer_set_hidden(self->layer, true);
    layer_add_child(details, self->layer);

    return true;

fail:
    weather_deinit(self);
    return false;
}

void weather_refresh_display(Weather *self)
{
    if (!self)
        return;

    bool   enabled = self->settings && self->settings->weather.enabled;
    time_t now     = time(NULL);

    if (!self->layer)
        return;

    if (!enabled || !weather_slot_is_valid(self, now)) {
        weather_release_icon_font(self);
        layer_set_hidden(self->layer, true);
        return;
    }

    if (self->settings->weather.display_mode != WEATHER_DISPLAY_ICON &&
        self->settings->weather.display_mode != WEATHER_DISPLAY_ICON_TEXT && !weather_status_is_stale(self, now))
        weather_release_icon_font(self);

    layer_set_hidden(self->layer, false);
    layer_mark_dirty(self->layer);
}

void weather_schedule_refresh(Weather *self)
{
    if (!self)
        return;

    // weather_tick() runs once per minute, so a due timestamp targets the next tick.
    self->next_update_at = time(NULL);
}

void weather_tick(Weather *self, const Settings *settings, time_t now)
{
    if (!self || !settings)
        return;

    if (!settings->weather.enabled) {
        if (self->next_update_at)
            weather_clear(self);

        return;
    }

    if (self->layer && self->last_update_at != 0)
        layer_mark_dirty(self->layer);

    if (self->next_update_at == 0)
        self->next_update_at = now;

    const WeatherSlot *slot = weather_current_slot(self, now);

    if (!slot || !slot->valid) {
        if (self->layer)
            layer_set_hidden(self->layer, true);

        time_t retry_at = now + (time_t)weather_retry_minutes(settings) * SECONDS_PER_MINUTE;

        // Do not move an already scheduled request further into the future.
        if (self->next_update_at == 0 || self->next_update_at > retry_at)
            self->next_update_at = retry_at;
    } else if (self->layer && now % SECONDS_PER_HOUR < SECONDS_PER_MINUTE) {
        weather_refresh_display(self);
    }

    if (self->next_update_at > now)
        return;

    weather_request_update(self, settings, now);
}

void weather_handle_message(Weather *self, const Settings *settings, DictionaryIterator *iterator)
{
    if (!self || !settings || !iterator || !settings->weather.enabled)
        return;

    Tuple *forecast = dict_find(iterator, MESSAGE_KEY_WEATHER_FORECAST);
    Tuple *failure  = dict_find(iterator, MESSAGE_KEY_WEATHER_UPDATE_FAILED);

    if (forecast && weather_apply_forecast(self, settings, forecast))
        return;

    if (!forecast && !failure)
        return;

    self->next_update_at = time(NULL) + (time_t)weather_retry_minutes(settings) * SECONDS_PER_MINUTE;
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
