/**
 * @file settings.c
 * @brief Persistent //GRID settings and Clay/AppMessage integration.
 */
#include "grid.h"

#define SETTINGS_PERSIST_KEY 1
#define SETTINGS_INBOX_SIZE  32
#define SETTINGS_OUTBOX_SIZE 16

static void settings_set_defaults(Settings *settings)
{
    settings->version             = GRID_SETTINGS_VERSION;
    settings->glance_duration_sec = GRID_GLANCE_DEFAULT_DURATION_SEC;
}

static void settings_validate(Settings *settings)
{
    settings->version = GRID_SETTINGS_VERSION;

    if (settings->glance_duration_sec < GRID_GLANCE_MIN_DURATION_SEC ||
        settings->glance_duration_sec > GRID_GLANCE_MAX_DURATION_SEC)
        settings->glance_duration_sec = GRID_GLANCE_DEFAULT_DURATION_SEC;
}

static bool tuple_get_duration(const Tuple *tuple, uint8_t *duration)
{
    if (!tuple || !duration)
        return false;

    int32_t value;

    switch (tuple->type) {
    case TUPLE_INT:
        value = tuple->value->int32;
        break;
    case TUPLE_UINT:
        if (tuple->value->uint32 > UINT8_MAX)
            return false;
        value = (int32_t)tuple->value->uint32;
        break;
    default:
        return false;
    }

    if (value < 0 || value > UINT8_MAX)
        return false;

    *duration = (uint8_t)value;
    return true;
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

static void inbox_received_handler(DictionaryIterator *iterator, void *context)
{
    SettingsStore *self  = context;
    Tuple         *tuple = dict_find(iterator, MESSAGE_KEY_GLANCE_DURATION_SEC);

    if (!self || !tuple)
        return;

    uint8_t duration;

    if (!tuple_get_duration(tuple, &duration))
        return;

    Settings updated            = self->value;
    updated.glance_duration_sec = duration;
    settings_validate(&updated);

    if (updated.glance_duration_sec == self->value.glance_duration_sec)
        return;

    self->value = updated;
    settings_save(self);
}

bool settings_init(SettingsStore *self)
{
    if (!self)
        return false;

    settings_load(self);

    app_message_set_context(self);
    app_message_register_inbox_received(inbox_received_handler);

    AppMessageResult result = app_message_open(SETTINGS_INBOX_SIZE, SETTINGS_OUTBOX_SIZE);

    if (result != APP_MSG_OK) {
        app_message_deregister_callbacks();
        app_message_set_context(NULL);
        return false;
    }

    return true;
}

void settings_deinit(__attribute__((__unused__)) SettingsStore *self)
{
    app_message_deregister_callbacks();
    app_message_set_context(NULL);
}
