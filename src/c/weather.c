/**
 * @file weather.c
 * @brief In-memory weather forecast synchronized through PebbleKit JS.
 */
#include "grid.h"

#ifdef PBL_PLATFORM_EMERY
#define WEATHER_FONT       FONT_KEY_GOTHIC_18_BOLD
#define WEATHER_TEXT_COLOR GColorCyan
#else
#define WEATHER_FONT       FONT_KEY_GOTHIC_14_BOLD
#define WEATHER_TEXT_COLOR GColorWhite
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

static void weather_layer_update_proc(Layer *layer, GContext *ctx)
{
    Weather *self = *(Weather **)layer_get_data(layer);

    if (!self)
        return;

    const WeatherSlot *slot = weather_current_slot(self, time(NULL));

    if (!slot)
        return;

    char value[40];

    if (slot->condition[0])
        snprintf(value, sizeof(value),
                 "%d\xC2\xB0"
                 "C  %s",
                 (int)slot->temperature_c, slot->condition);
    else
        snprintf(value, sizeof(value),
                 "%d\xC2\xB0"
                 "C",
                 (int)slot->temperature_c);

    graphics_context_set_text_color(ctx, WEATHER_TEXT_COLOR);
    graphics_draw_text(ctx, value, fonts_get_system_font(WEATHER_FONT), layer_get_bounds(layer),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void weather_clear(Weather *self)
{
    if (!self)
        return;

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

    memset(self->slots, 0, sizeof(self->slots));

    for (uint8_t i = 0; i < count; i++) {
        const uint8_t *slot_payload = payload + WEATHER_FORECAST_HEADER_SIZE + i * WEATHER_FORECAST_SLOT_SIZE;

        self->slots[i].temperature_c = (int8_t)slot_payload[0];
        memcpy(self->slots[i].condition, slot_payload + 1, WEATHER_CONDITION_LENGTH);
        self->slots[i].condition[WEATHER_CONDITION_LENGTH - 1] = '\0';
    }

    time_t delay =
        self->refresh_pending ? SECONDS_PER_MINUTE : (time_t)weather_refresh_hours(settings) * SECONDS_PER_HOUR;

    self->count            = count;
    self->interval_minutes = interval_minutes;
    self->start_at         = (time_t)start_at;
    self->next_update_at   = time(NULL) + delay;
    self->in_progress      = false;
    self->refresh_pending  = false;

    if (self->layer) {
        layer_set_hidden(self->layer, false);
        layer_mark_dirty(self->layer);
    }

    return true;
}

bool weather_init(Weather *self, Layer *details, const ScreenGeometry *geometry)
{
    if (!self || !details || !geometry)
        return false;

    memset(self, 0, sizeof(*self));
    self->layer = layer_create_with_data(geometry->weather, sizeof(Weather *));

    if (!self->layer)
        return false;

    *(Weather **)layer_get_data(self->layer) = self;
    layer_set_update_proc(self->layer, weather_layer_update_proc);
    layer_set_hidden(self->layer, true);
    layer_add_child(details, self->layer);

    return true;
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

    self->layer = NULL;
    weather_clear(self);

    if (layer)
        layer_destroy(layer);
}
