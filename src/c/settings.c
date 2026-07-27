/**
 * @file settings.c
 * @brief Persistent //GRID settings and Clay/AppMessage integration.
 */
#include "grid.h"

#define SETTINGS_PERSIST_KEY 1
#define SETTINGS_INBOX_SIZE  256
#define SETTINGS_OUTBOX_SIZE 256

static SettingsStore *s_settings_store;

static void settings_set_defaults(Settings *settings)
{
    settings->version             = GRID_SETTINGS_VERSION;
    settings->glance_duration_sec = GRID_GLANCE_DEFAULT_DURATION_SEC;
    settings->weather.enabled     = GRID_WEATHER_DEFAULT_ENABLED;
    settings->weather.provider_id = GRID_WEATHER_DEFAULT_PROVIDER_ID;
    settings->weather.refresh_hrs = GRID_WEATHER_DEFAULT_UPDATE_INTERVAL_HOURS;
    settings->weather.retry_min   = GRID_WEATHER_DEFAULT_RETRY_INTERVAL_MINUTES;
}

static void settings_validate(Settings *settings)
{
    settings->version = GRID_SETTINGS_VERSION;

    if (settings->glance_duration_sec < GRID_GLANCE_MIN_DURATION_SEC ||
        settings->glance_duration_sec > GRID_GLANCE_MAX_DURATION_SEC)
        settings->glance_duration_sec = GRID_GLANCE_DEFAULT_DURATION_SEC;

    settings->weather.enabled = settings->weather.enabled ? 1 : 0;

    if (settings->weather.refresh_hrs < GRID_WEATHER_MIN_UPDATE_INTERVAL_HOURS ||
        settings->weather.refresh_hrs > GRID_WEATHER_MAX_UPDATE_INTERVAL_HOURS)
        settings->weather.refresh_hrs = GRID_WEATHER_DEFAULT_UPDATE_INTERVAL_HOURS;

    if (settings->weather.retry_min < GRID_WEATHER_MIN_RETRY_INTERVAL_MINUTES ||
        settings->weather.retry_min > GRID_WEATHER_MAX_RETRY_INTERVAL_MINUTES)
        settings->weather.retry_min = GRID_WEATHER_DEFAULT_RETRY_INTERVAL_MINUTES;
}

static bool settings_load(SettingsStore *self)
{
    settings_set_defaults(&self->value);

    if (!persist_exists(SETTINGS_PERSIST_KEY) || persist_get_size(SETTINGS_PERSIST_KEY) != (int)sizeof(self->value))
        return false;

    Settings stored = {0};

    if (persist_read_data(SETTINGS_PERSIST_KEY, &stored, sizeof(stored)) != (int)sizeof(stored) ||
        stored.version != GRID_SETTINGS_VERSION)
        return false;

    settings_validate(&stored);
    self->value = stored;

    return true;
}

static bool settings_save(const SettingsStore *self)
{
    return persist_write_data(SETTINGS_PERSIST_KEY, &self->value, sizeof(self->value)) == (int)sizeof(self->value);
}

static void inbox_received_handler(DictionaryIterator *iterator, __attribute__((unused)) void *context)
{
    SettingsStore *self = s_settings_store;
    App           *app  = self ? self->app : NULL;

    if (!self || !iterator)
        return;

    Tuple *forecast = dict_find(iterator, MESSAGE_KEY_WEATHER_FORECAST);
    Tuple *failed   = dict_find(iterator, MESSAGE_KEY_WEATHER_UPDATE_FAILED);

    if (app && app->mounted && (forecast || failed))
        weather_handle_message(&app->weather, &self->value, iterator);

    bool changed = false;

    Tuple *glance_duration = dict_find(iterator, MESSAGE_KEY_GLANCE_DURATION_SEC);
    if (glance_duration) {
        self->value.glance_duration_sec = (uint8_t)glance_duration->value->int32;
        changed                         = true;
    }

    Tuple *weather_enable = dict_find(iterator, MESSAGE_KEY_WEATHER_ENABLE);
    if (weather_enable) {
        self->value.weather.enabled = weather_enable->value->int32 == 1;
        changed                     = true;
    }

    Tuple *provider_id = dict_find(iterator, MESSAGE_KEY_WEATHER_PROVIDER_ID);
    if (provider_id && (provider_id->type == TUPLE_INT || provider_id->type == TUPLE_UINT)) {
        self->value.weather.provider_id = (uint8_t)provider_id->value->int32;
        changed                         = true;
    }

    Tuple *update_interval = dict_find(iterator, MESSAGE_KEY_WEATHER_UPDATE_INTERVAL_HOURS);
    if (update_interval) {
        self->value.weather.refresh_hrs = (uint8_t)update_interval->value->int32;
        changed                         = true;
    }

    Tuple *retry_interval = dict_find(iterator, MESSAGE_KEY_WEATHER_RETRY_INTERVAL_MINUTES);
    if (retry_interval) {
        self->value.weather.retry_min = (uint8_t)retry_interval->value->int32;
        changed                       = true;
    }

    if (!changed)
        return;

    settings_validate(&self->value);

    if (!settings_save(self))
        return;

    // Apply weather settings on the next minute tick.
    // Only the scheduler timestamp is changed here; the tick performs I/O.
    if (app && app->mounted && self->value.weather.enabled)
        weather_schedule_refresh(&app->weather);
}

static void outbox_failed_handler(DictionaryIterator *iterator, __attribute__((unused)) AppMessageResult reason,
                                  __attribute__((unused)) void *context)
{
    SettingsStore *self = s_settings_store;
    App           *app  = self ? self->app : NULL;

    if (!app || !app->mounted || !iterator || !dict_find(iterator, MESSAGE_KEY_WEATHER_REQUEST))
        return;

    weather_update_failed(&app->weather, &self->value);
}

bool settings_init(SettingsStore *self, App *app)
{
    if (!self || !app || s_settings_store)
        return false;

    settings_load(self);
    self->app        = app;
    s_settings_store = self;

    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_outbox_failed(outbox_failed_handler);
    self->callbacks_registered = true;

    AppMessageResult result = app_message_open(SETTINGS_INBOX_SIZE, SETTINGS_OUTBOX_SIZE);

    if (result != APP_MSG_OK) {
        app_message_deregister_callbacks();
        self->callbacks_registered = false;
        self->app                  = NULL;
        s_settings_store           = NULL;
        return false;
    }

    return true;
}

void settings_deinit(SettingsStore *self)
{
    if (!self)
        return;

    bool callbacks_registered = self->callbacks_registered;

    self->callbacks_registered = false;
    self->app                  = NULL;

    if (s_settings_store == self)
        s_settings_store = NULL;

    if (callbacks_registered)
        app_message_deregister_callbacks();
}
