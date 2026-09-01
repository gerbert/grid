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
    memset(settings, 0, sizeof(*settings));

    settings->version                     = GRID_SETTINGS_VERSION;
    settings->glance_duration_sec         = GRID_GLANCE_DEFAULT_DURATION_SEC;
    settings->do_not_disturb.enabled      = GRID_DND_DEFAULT_ENABLED;
    settings->do_not_disturb.start_minute = GRID_DND_DEFAULT_START_MINUTE;
    settings->do_not_disturb.end_minute   = GRID_DND_DEFAULT_END_MINUTE;
    settings->weather.enabled             = GRID_WEATHER_DEFAULT_ENABLED;
    settings->weather.provider_id         = GRID_WEATHER_DEFAULT_PROVIDER_ID;
    settings->weather.refresh_hrs         = GRID_WEATHER_DEFAULT_UPDATE_INTERVAL_HOURS;
    settings->weather.retry_min           = GRID_WEATHER_DEFAULT_RETRY_INTERVAL_MINUTES;
    settings->weather.display_mode        = GRID_WEATHER_DEFAULT_DISPLAY_MODE;
}

static bool settings_alarm_date_valid(const AlarmSettings *alarm)
{
    if (!alarm || alarm->year < GRID_ALARM_MIN_YEAR || alarm->year > GRID_ALARM_MAX_YEAR || alarm->month < 1 ||
        alarm->month > 12 || alarm->day < 1 || alarm->day > 31)
        return false;

    struct tm candidate = {0};

    candidate.tm_year  = alarm->year - 1900;
    candidate.tm_mon   = alarm->month - 1;
    candidate.tm_mday  = alarm->day;
    candidate.tm_hour  = 12;
    candidate.tm_isdst = -1;

    if (mktime(&candidate) == (time_t)-1)
        return false;

    return candidate.tm_year == (int)alarm->year - 1900 && candidate.tm_mon == alarm->month - 1 &&
           candidate.tm_mday == alarm->day;
}

static void settings_validate_alarms(AlarmCollectionSettings *alarms)
{
    if (alarms->count > GRID_ALARM_MAX_COUNT)
        alarms->count = GRID_ALARM_MAX_COUNT;

    for (uint8_t i = 0; i < alarms->count; i++) {
        AlarmSettings *alarm = &alarms->items[i];

        alarm->enabled = alarm->enabled ? 1 : 0;

        if (alarm->minute_of_day >= GRID_MINUTES_PER_DAY) {
            alarm->minute_of_day = 7 * 60;
            alarm->enabled       = 0;
        }

        if (alarm->repeat_mode != ALARM_REPEAT_ONCE && alarm->repeat_mode != ALARM_REPEAT_DAYS) {
            alarm->repeat_mode = ALARM_REPEAT_ONCE;
            alarm->enabled     = 0;
        }

        alarm->days_mask &= GRID_ALARM_ALL_DAYS_MASK;

        alarm->reserved = 0;

        if (alarm->repeat_mode == ALARM_REPEAT_DAYS) {
            if (!alarm->days_mask)
                alarm->enabled = 0;

            alarm->year  = 0;
            alarm->month = 0;
            alarm->day   = 0;
            continue;
        }

        alarm->days_mask = 0;

        if (!settings_alarm_date_valid(alarm))
            alarm->enabled = 0;
    }

    if (alarms->count < GRID_ALARM_MAX_COUNT) {
        memset(&alarms->items[alarms->count], 0, sizeof(alarms->items[0]) * (GRID_ALARM_MAX_COUNT - alarms->count));
    }
}

static void settings_validate(Settings *settings)
{
    settings->version = GRID_SETTINGS_VERSION;

    if (settings->glance_duration_sec < GRID_GLANCE_MIN_DURATION_SEC ||
        settings->glance_duration_sec > GRID_GLANCE_MAX_DURATION_SEC)
        settings->glance_duration_sec = GRID_GLANCE_DEFAULT_DURATION_SEC;

    settings->do_not_disturb.enabled = settings->do_not_disturb.enabled ? 1 : 0;

    if (settings->do_not_disturb.start_minute >= GRID_MINUTES_PER_DAY)
        settings->do_not_disturb.start_minute = GRID_DND_DEFAULT_START_MINUTE;

    if (settings->do_not_disturb.end_minute >= GRID_MINUTES_PER_DAY)
        settings->do_not_disturb.end_minute = GRID_DND_DEFAULT_END_MINUTE;

    settings->weather.enabled = settings->weather.enabled ? 1 : 0;

    if (settings->weather.refresh_hrs < GRID_WEATHER_MIN_UPDATE_INTERVAL_HOURS ||
        settings->weather.refresh_hrs > GRID_WEATHER_MAX_UPDATE_INTERVAL_HOURS)
        settings->weather.refresh_hrs = GRID_WEATHER_DEFAULT_UPDATE_INTERVAL_HOURS;

    if (settings->weather.retry_min < GRID_WEATHER_MIN_RETRY_INTERVAL_MINUTES ||
        settings->weather.retry_min > GRID_WEATHER_MAX_RETRY_INTERVAL_MINUTES)
        settings->weather.retry_min = GRID_WEATHER_DEFAULT_RETRY_INTERVAL_MINUTES;

    if (settings->weather.display_mode > WEATHER_DISPLAY_TEMPERATURE)
        settings->weather.display_mode = GRID_WEATHER_DEFAULT_DISPLAY_MODE;

    settings_validate_alarms(&settings->alarms);
}

static bool settings_load(SettingsStore *self)
{
    settings_set_defaults(&self->value);

    if (!persist_exists(SETTINGS_PERSIST_KEY))
        return false;

    int stored_size = persist_get_size(SETTINGS_PERSIST_KEY);

    if (stored_size == (int)sizeof(self->value)) {
        Settings stored = {0};

        if (persist_read_data(SETTINGS_PERSIST_KEY, &stored, sizeof(stored)) != (int)sizeof(stored) ||
            stored.version != GRID_SETTINGS_VERSION)
            return false;

        settings_validate(&stored);
        self->value = stored;
        return true;
    }

    return false;
}

bool settings_commit(SettingsStore *self, const Settings *updated)
{
    if (!self || !updated)
        return false;

    Settings validated = *updated;

    settings_validate(&validated);

    if (persist_write_data(SETTINGS_PERSIST_KEY, &validated, sizeof(validated)) != (int)sizeof(validated))
        return false;

    self->value = validated;
    return true;
}

static bool settings_decode_alarms(const Tuple *tuple, AlarmCollectionSettings *alarms)
{
    if (!tuple || !alarms || tuple->type != TUPLE_BYTE_ARRAY || tuple->length < GRID_ALARM_CONFIG_HEADER_SIZE)
        return false;

    const uint8_t *payload         = tuple->value->data;
    uint8_t        count           = payload[0];
    uint16_t       expected_length = GRID_ALARM_CONFIG_HEADER_SIZE + count * GRID_ALARM_CONFIG_ITEM_SIZE;

    if (count > GRID_ALARM_MAX_COUNT || tuple->length != expected_length)
        return false;

    memset(alarms, 0, sizeof(*alarms));
    alarms->count = count;

    for (uint8_t i = 0; i < count; i++) {
        const uint8_t *item  = payload + GRID_ALARM_CONFIG_HEADER_SIZE + i * GRID_ALARM_CONFIG_ITEM_SIZE;
        AlarmSettings *alarm = &alarms->items[i];

        alarm->enabled       = item[0];
        alarm->repeat_mode   = item[1];
        alarm->minute_of_day = (uint16_t)(item[2] | ((uint16_t)item[3] << 8));
        alarm->days_mask     = item[4];
        alarm->reserved      = item[5];
        alarm->year          = (uint16_t)(item[6] | ((uint16_t)item[7] << 8));
        alarm->month         = item[8];
        alarm->day           = item[9];
    }

    return true;
}

static void inbox_received_handler(DictionaryIterator *iterator, __attribute__((unused)) void *context)
{
    SettingsStore *self = s_settings_store;
    App           *app  = self ? self->app : NULL;

    if (!self || !iterator)
        return;

    Tuple *forecast        = dict_find(iterator, MESSAGE_KEY_WEATHER_FORECAST);
    Tuple *weather_failure = dict_find(iterator, MESSAGE_KEY_WEATHER_UPDATE_FAILED);

    if (app && app->mounted && (forecast || weather_failure))
        weather_handle_message(&app->weather, &self->value, iterator);

    Settings updated        = self->value;
    bool     changed        = false;
    bool     alarms_changed = false;

    Tuple *glance_duration = dict_find(iterator, MESSAGE_KEY_GLANCE_DURATION_SEC);
    if (glance_duration) {
        updated.glance_duration_sec = (uint8_t)glance_duration->value->int32;
        changed                     = true;
    }

    Tuple *dnd_enable = dict_find(iterator, MESSAGE_KEY_DND_ENABLE);
    if (dnd_enable) {
        updated.do_not_disturb.enabled = dnd_enable->value->int32 == 1;
        changed                        = true;
    }

    Tuple *dnd_start = dict_find(iterator, MESSAGE_KEY_DND_START_TIME);
    if (dnd_start && (dnd_start->type == TUPLE_INT || dnd_start->type == TUPLE_UINT)) {
        updated.do_not_disturb.start_minute = (uint16_t)dnd_start->value->int32;
        changed                             = true;
    }

    Tuple *dnd_end = dict_find(iterator, MESSAGE_KEY_DND_END_TIME);
    if (dnd_end && (dnd_end->type == TUPLE_INT || dnd_end->type == TUPLE_UINT)) {
        updated.do_not_disturb.end_minute = (uint16_t)dnd_end->value->int32;
        changed                           = true;
    }

    Tuple *weather_enable = dict_find(iterator, MESSAGE_KEY_WEATHER_ENABLE);
    if (weather_enable) {
        updated.weather.enabled = weather_enable->value->int32 == 1;
        changed                 = true;
    }

    Tuple *provider_id = dict_find(iterator, MESSAGE_KEY_WEATHER_PROVIDER_ID);
    if (provider_id && (provider_id->type == TUPLE_INT || provider_id->type == TUPLE_UINT)) {
        updated.weather.provider_id = (uint8_t)provider_id->value->int32;
        changed                     = true;
    }

    Tuple *update_interval = dict_find(iterator, MESSAGE_KEY_WEATHER_UPDATE_INTERVAL_HOURS);
    if (update_interval) {
        updated.weather.refresh_hrs = (uint8_t)update_interval->value->int32;
        changed                     = true;
    }

    Tuple *retry_interval = dict_find(iterator, MESSAGE_KEY_WEATHER_RETRY_INTERVAL_MINUTES);
    if (retry_interval) {
        updated.weather.retry_min = (uint8_t)retry_interval->value->int32;
        changed                   = true;
    }

    Tuple *display_mode = dict_find(iterator, MESSAGE_KEY_WEATHER_DISPLAY_MODE);
    if (display_mode) {
        updated.weather.display_mode = (uint8_t)display_mode->value->int32;
        changed                      = true;
    }

    Tuple *alarm_config = dict_find(iterator, MESSAGE_KEY_ALARM_CONFIG);
    if (alarm_config) {
        if (!settings_decode_alarms(alarm_config, &updated.alarms))
            return;

        changed        = true;
        alarms_changed = true;
    }

    if (!changed || !settings_commit(self, &updated))
        return;

    if (app && app->mounted) {
        weather_refresh_display(&app->weather);

        // Apply weather settings on the next minute tick.
        // Only the scheduler timestamp is changed here; the tick performs I/O.
        if (self->value.weather.enabled)
            weather_schedule_refresh(&app->weather);

        if (alarms_changed)
            alarm_settings_changed(&app->alarm);
    }
}

bool settings_init(SettingsStore *self, App *app)
{
    if (!self || !app || s_settings_store)
        return false;

    settings_load(self);
    self->app        = app;
    s_settings_store = self;

    app_message_register_inbox_received(inbox_received_handler);
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
