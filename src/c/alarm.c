/**
 * @file alarm.c
 * @brief Alarm matching, vibration, idle presentation, and flick-to-stop handling.
 */
#include "grid.h"

#ifdef PBL_PLATFORM_EMERY
#define ALARM_FONT  FONT_KEY_GOTHIC_18_BOLD
#define ALARM_COLOR GColorCyan
#else
#define ALARM_FONT  FONT_KEY_GOTHIC_14_BOLD
#define ALARM_COLOR GColorWhite
#endif

#define ALARM_VIBE_REPEAT_MS      1800
#define ALARM_FLICK_VIBE_GUARD_MS 650

static uint32_t current_millis(void)
{
    time_t   seconds = 0;
    uint16_t millis  = 0;

    time_ms(&seconds, &millis);
    return (uint32_t)seconds * 1000u + millis;
}

static bool alarm_once_time(const AlarmSettings *alarm, time_t *result)
{
    if (!alarm || alarm->year < GRID_ALARM_MIN_YEAR || alarm->year > GRID_ALARM_MAX_YEAR || alarm->month < 1 ||
        alarm->month > 12 || alarm->day < 1 || alarm->day > 31 || alarm->minute_of_day >= GRID_MINUTES_PER_DAY)
        return false;

    struct tm candidate = {0};

    candidate.tm_year  = alarm->year - 1900;
    candidate.tm_mon   = alarm->month - 1;
    candidate.tm_mday  = alarm->day;
    candidate.tm_hour  = alarm->minute_of_day / 60;
    candidate.tm_min   = alarm->minute_of_day % 60;
    candidate.tm_isdst = -1;

    time_t value = mktime(&candidate);

    if (value == (time_t)-1 || candidate.tm_year != (int)alarm->year - 1900 || candidate.tm_mon != alarm->month - 1 ||
        candidate.tm_mday != alarm->day || candidate.tm_hour != alarm->minute_of_day / 60 ||
        candidate.tm_min != alarm->minute_of_day % 60)
        return false;

    if (result)
        *result = value;

    return true;
}

static bool alarm_recurring_next(const AlarmSettings *alarm, time_t now, time_t *result)
{
    struct tm *current = localtime(&now);

    if (!alarm || !current || !alarm->days_mask || alarm->minute_of_day >= GRID_MINUTES_PER_DAY)
        return false;

    struct tm base = *current;

    for (int day_offset = 0; day_offset <= 7; day_offset++) {
        struct tm candidate = base;

        candidate.tm_mday += day_offset;
        candidate.tm_hour  = alarm->minute_of_day / 60;
        candidate.tm_min   = alarm->minute_of_day % 60;
        candidate.tm_sec   = 0;
        candidate.tm_isdst = -1;

        time_t value = mktime(&candidate);

        if (value == (time_t)-1 || value < now)
            continue;
        if (!(alarm->days_mask & (1u << candidate.tm_wday)))
            continue;

        if (result)
            *result = value;

        return true;
    }

    return false;
}

static bool alarm_matches_now(const AlarmSettings *alarm, const struct tm *current)
{
    if (!alarm || !current || !alarm->enabled || alarm->minute_of_day >= GRID_MINUTES_PER_DAY)
        return false;

    uint16_t current_minute = (uint16_t)(current->tm_hour * 60 + current->tm_min);

    if (current_minute != alarm->minute_of_day)
        return false;

    if (alarm->repeat_mode == ALARM_REPEAT_DAYS)
        return (alarm->days_mask & (1u << current->tm_wday)) != 0;

    if (alarm->repeat_mode != ALARM_REPEAT_ONCE)
        return false;

    return current->tm_year + 1900 == alarm->year && current->tm_mon + 1 == alarm->month &&
           current->tm_mday == alarm->day;
}

static bool alarm_find_next(const Alarm *self, time_t now, uint16_t *minute_of_day)
{
    if (!self || !self->settings_store)
        return false;

    const AlarmCollectionSettings *alarms         = &self->settings_store->value.alarms;
    bool                           found          = false;
    time_t                         nearest        = 0;
    uint16_t                       nearest_minute = 0;

    for (uint8_t i = 0; i < alarms->count; i++) {
        const AlarmSettings *alarm = &alarms->items[i];
        time_t               candidate;

        if (!alarm->enabled)
            continue;

        bool valid = alarm->repeat_mode == ALARM_REPEAT_ONCE ? alarm_once_time(alarm, &candidate)
                                                             : alarm_recurring_next(alarm, now, &candidate);

        if (!valid || candidate < now)
            continue;
        if (found && candidate >= nearest)
            continue;

        found          = true;
        nearest        = candidate;
        nearest_minute = alarm->minute_of_day;
    }

    if (found && minute_of_day)
        *minute_of_day = nearest_minute;

    return found;
}

static void alarm_refresh_display(Alarm *self)
{
    if (!self || !self->layer)
        return;

    if (self->ringing) {
        self->display_valid  = true;
        self->display_minute = self->active_minute;
    } else {
        self->display_valid = alarm_find_next(self, time(NULL), &self->display_minute);
    }

    layer_mark_dirty(self->layer);
}

static void format_alarm_time(uint16_t minute_of_day, char *buffer, size_t buffer_size)
{
    uint8_t hour   = minute_of_day / 60;
    uint8_t minute = minute_of_day % 60;

    if (clock_is_24h_style()) {
        snprintf(buffer, buffer_size, "%02u:%02u", hour, minute);
        return;
    }

    const char *period = hour >= 12 ? "PM" : "AM";
    uint8_t     hour12 = hour % 12;

    if (hour12 == 0)
        hour12 = 12;

    snprintf(buffer, buffer_size, "%u:%02u %s", hour12, minute, period);
}

static void alarm_layer_update_proc(Layer *layer, GContext *ctx)
{
    Alarm *self = *(Alarm **)layer_get_data(layer);

    if (!self || !self->display_valid || !self->details_layer || !layer_get_hidden(self->details_layer))
        return;

    char  time_buf[12]    = {0};
    char  display_buf[32] = {0};
    GRect bounds          = layer_get_bounds(layer);
    GFont font            = fonts_get_system_font(ALARM_FONT);

    format_alarm_time(self->display_minute, time_buf, sizeof(time_buf));
    snprintf(display_buf, sizeof(display_buf), "Next alarm %s", time_buf);

    graphics_context_set_text_color(ctx, ALARM_COLOR);
    graphics_draw_text(ctx, display_buf, font, bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void alarm_vibe_timer_cb(void *context);

static void alarm_vibe_schedule(Alarm *self)
{
    if (!self || !self->ringing || self->vibe_timer)
        return;

    self->vibe_timer = app_timer_register(ALARM_VIBE_REPEAT_MS, alarm_vibe_timer_cb, self);
}

static void alarm_vibe_pulse(Alarm *self)
{
    if (!self || !self->ringing)
        return;

    // Use only the firmware-provided long pulse. The timer repeats the pulse
    // while an already-triggered alarm is ringing; it never schedules alarms.
    self->last_vibe_ms = current_millis();
    vibes_long_pulse();
    alarm_vibe_schedule(self);
}

static void alarm_vibe_timer_cb(void *context)
{
    Alarm *self = context;

    if (!self)
        return;

    self->vibe_timer = NULL;

    if (self->ringing)
        alarm_vibe_pulse(self);
}

static void alarm_vibe_stop(Alarm *self)
{
    if (!self)
        return;

    bool had_alarm_vibe = self->ringing || self->vibe_timer;

    if (self->vibe_timer) {
        app_timer_cancel(self->vibe_timer);
        self->vibe_timer = NULL;
    }

    if (had_alarm_vibe)
        vibes_cancel();
}

static bool alarm_start_next(Alarm *self)
{
    if (!self || !self->settings_store || self->ringing)
        return false;

    for (uint8_t i = 0; i < GRID_ALARM_MAX_COUNT; i++) {
        uint16_t bit = (uint16_t)(1u << i);

        if (!(self->pending_mask & bit))
            continue;

        const AlarmSettings *alarm = &self->settings_store->value.alarms.items[i];

        self->pending_mask &= (uint16_t)~bit;
        self->active_minute = alarm->minute_of_day;
        self->last_vibe_ms  = 0;
        self->ringing       = true;

        alarm_vibe_pulse(self);
        alarm_refresh_display(self);
        return true;
    }

    return false;
}

static void alarm_stop_current(Alarm *self)
{
    if (!self || !self->ringing)
        return;

    alarm_vibe_stop(self);
    self->ringing = false;

    if (!alarm_start_next(self))
        alarm_refresh_display(self);
}

bool alarm_init(Alarm *self, Layer *root, Layer *details, SettingsStore *settings_store, const ScreenGeometry *geometry)
{
    if (!self || !root || !details || !settings_store || !geometry)
        return false;

    self->settings_store      = settings_store;
    self->details_layer       = details;
    self->last_checked_minute = (time_t)-1;
    self->layer               = layer_create_with_data(geometry->weather, sizeof(Alarm *));

    if (!self->layer)
        goto fail;

    *(Alarm **)layer_get_data(self->layer) = self;
    layer_set_update_proc(self->layer, alarm_layer_update_proc);
    layer_add_child(root, self->layer);
    alarm_refresh_display(self);
    return true;

fail:
    alarm_deinit(self);
    return false;
}

void alarm_settings_changed(Alarm *self)
{
    if (!self)
        return;

    // Pending bits refer to indices in the previous configuration. A ringing alarm has copied what it needs.
    self->pending_mask = 0;
    alarm_refresh_display(self);
}

void alarm_tick(Alarm *self, time_t now)
{
    if (!self || !self->settings_store)
        return;

    time_t epoch_minute = now / 60;

    if (self->last_checked_minute == epoch_minute)
        return;

    self->last_checked_minute = epoch_minute;

    struct tm *current = localtime(&now);

    if (!current)
        return;

    Settings updated          = self->settings_store->value;
    time_t   minute_start     = now - current->tm_sec;
    bool     settings_changed = false;

    for (uint8_t i = 0; i < updated.alarms.count; i++) {
        AlarmSettings *alarm = &updated.alarms.items[i];

        if (!alarm->enabled)
            continue;

        if (alarm->repeat_mode == ALARM_REPEAT_ONCE) {
            time_t alarm_time;

            if (!alarm_once_time(alarm, &alarm_time) || alarm_time < minute_start) {
                alarm->enabled   = 0;
                settings_changed = true;
                continue;
            }
        }

        if (!alarm_matches_now(alarm, current))
            continue;

        self->pending_mask |= (uint16_t)(1u << i);

        if (alarm->repeat_mode == ALARM_REPEAT_ONCE) {
            alarm->enabled   = 0;
            settings_changed = true;
        }
    }

    if (settings_changed)
        settings_commit(self->settings_store, &updated);

    alarm_start_next(self);
    alarm_refresh_display(self);
}

bool alarm_is_ringing(const Alarm *self) { return self && self->ringing; }

void alarm_handle_flick(Alarm *self, __attribute__((__unused__)) AccelAxisType axis)
{
    if (!self || !self->ringing || !self->settings_store)
        return;

    uint32_t now_ms = current_millis();

    // AccelTap does not expose the raw did_vibrate flag. Ignore tap events
    // immediately after our own system pulse so the alarm cannot reset itself.
    if (now_ms - self->last_vibe_ms < ALARM_FLICK_VIBE_GUARD_MS)
        return;

    alarm_stop_current(self);
}

void alarm_deinit(Alarm *self)
{
    if (!self)
        return;

    Layer *layer = self->layer;

    alarm_vibe_stop(self);

    self->layer               = NULL;
    self->details_layer       = NULL;
    self->settings_store      = NULL;
    self->pending_mask        = 0;
    self->active_minute       = 0;
    self->display_minute      = 0;
    self->last_vibe_ms        = 0;
    self->vibe_timer          = NULL;
    self->last_checked_minute = (time_t)-1;
    self->display_valid       = false;
    self->ringing             = false;

    if (layer)
        layer_destroy(layer);
}
