/**
 * @file alarm.c
 * @brief Alarm matching, Wakeup scheduling, vibration, idle presentation, and flick-to-stop handling.
 */
#include "include/grid.h"

#ifdef PBL_PLATFORM_EMERY
#define ALARM_FONT  FONT_KEY_GOTHIC_18_BOLD
#define ALARM_COLOR GColorCyan
#else
#define ALARM_FONT  FONT_KEY_GOTHIC_14_BOLD
#define ALARM_COLOR GColorWhite
#endif

#define ALARM_ROW_GAP             2
#define ALARM_VIBE_REPEAT_MS      1800
#define ALARM_FLICK_VIBE_GUARD_MS 650
#define ALARM_WAKEUP_PERSIST_KEY  2
#define ALARM_WAKEUP_COOKIE       0x47524944 // GRID
#define ALARM_INVALID_WAKEUP_ID   ((WakeupId) - 1)

typedef struct {
    time_t   timestamp;
    uint16_t minute_of_day;
    bool     timer;
    bool     valid;
} AlarmNext;

static uint32_t current_millis(void)
{
    time_t   seconds = 0;
    uint16_t millis  = 0;

    time_ms(&seconds, &millis);
    return (uint32_t)seconds * 1000u + millis;
}

static bool alarm_once_time(const AlarmSettings *alarm, time_t *result)
{
    if (!alarm)
        return false;

    if (alarm->repeat_mode == ALARM_REPEAT_TIMER) {
        if (!alarm->minute_of_day || alarm->minute_of_day >= GRID_MINUTES_PER_DAY)
            return false;

        uint32_t epoch_minute = (uint32_t)alarm->year | ((uint32_t)alarm->month << 16) | ((uint32_t)alarm->day << 24);
        uint64_t seconds      = (uint64_t)epoch_minute * 60u + alarm->reserved;
        time_t   timestamp    = (time_t)seconds;

        if (timestamp <= 0 || (uint64_t)timestamp != seconds)
            return false;

        if (result)
            *result = timestamp;

        return true;
    }

    if (alarm->year < GRID_ALARM_MIN_YEAR || alarm->year > GRID_ALARM_MAX_YEAR || alarm->month < 1 ||
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

static void alarm_find_next(const Alarm *self, time_t now, AlarmNext *next, AlarmNext *next_timer,
                            AlarmNext *next_alarm)
{
    if (!next)
        return;

    *next = (AlarmNext){0};
    if (next_timer)
        *next_timer = (AlarmNext){0};
    if (next_alarm)
        *next_alarm = (AlarmNext){0};

    if (!self || !self->settings_store)
        return;

    const AlarmCollectionSettings *alarms = &self->settings_store->value.alarms;

    for (uint8_t i = 0; i < alarms->count; i++) {
        const AlarmSettings *alarm = &alarms->items[i];
        time_t               candidate;

        if (!alarm->enabled)
            continue;

        bool valid = alarm->repeat_mode == ALARM_REPEAT_DAYS ? alarm_recurring_next(alarm, now, &candidate)
                                                             : alarm_once_time(alarm, &candidate);

        if (!valid || candidate < now)
            continue;

        AlarmNext occurrence = {
            .valid         = true,
            .timestamp     = candidate,
            .minute_of_day = alarm->minute_of_day,
            .timer         = alarm->repeat_mode == ALARM_REPEAT_TIMER,
        };

        if (!next->valid || candidate < next->timestamp)
            *next = occurrence;
        if (next_timer && occurrence.timer && (!next_timer->valid || candidate < next_timer->timestamp))
            *next_timer = occurrence;
        if (next_alarm && !occurrence.timer && (!next_alarm->valid || candidate < next_alarm->timestamp))
            *next_alarm = occurrence;
    }
}

static void alarm_refresh_display(Alarm *self, const AlarmNext *next_alarm, const AlarmNext *next_timer, time_t now)
{
    if (!self || !self->layer)
        return;

    AlarmNext current_next;
    AlarmNext current_alarm;
    AlarmNext current_timer;

    if (!next_alarm || !next_timer) {
        alarm_find_next(self, now + 1, &current_next, &current_timer, &current_alarm);
        next_alarm = &current_alarm;
        next_timer = &current_timer;
    }

    time_t timer_timestamp = next_timer->valid ? next_timer->timestamp : 0;

    if (self->ringing) {
        self->display_valid = true;
        self->display_timer = self->active_timer && !next_alarm->valid;
        self->display_minute =
            self->active_timer && next_alarm->valid ? next_alarm->minute_of_day : self->active_minute;
        self->display_timestamp      = self->active_timer ? 0 : timer_timestamp;
        self->display_timer_interval = self->active_timer ? (next_alarm->valid ? self->active_minute : 0)
                                                          : (next_timer->valid ? next_timer->minute_of_day : 0);
        self->display_now            = now;
    } else {
        bool same_timer = self->display_timestamp && timer_timestamp && self->display_timestamp == timer_timestamp;

        if (!same_timer || now > self->display_now || self->display_now - now > 1)
            self->display_now = now;

        self->display_valid          = next_alarm->valid || next_timer->valid;
        self->display_timer          = !next_alarm->valid && next_timer->valid;
        self->display_minute         = next_alarm->valid ? next_alarm->minute_of_day : next_timer->minute_of_day;
        self->display_timestamp      = timer_timestamp;
        self->display_timer_interval = next_alarm->valid && next_timer->valid ? next_timer->minute_of_day : 0;
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
    char  display_buf[80] = {0};
    GRect bounds          = layer_get_bounds(layer);
    GRect row             = GRect(0, 0, bounds.size.w, (bounds.size.h - ALARM_ROW_GAP) / 2);
    GFont font            = fonts_get_system_font(ALARM_FONT);

    int   cell_width = bounds.size.w / 2;
    GRect label_row  = GRect(0, 0, cell_width, row.size.h);
    GRect value_row  = GRect(cell_width, 0, bounds.size.w - cell_width, row.size.h);

    graphics_context_set_text_color(ctx, ALARM_COLOR);

    if (!self->display_timer) {
        format_alarm_time(self->display_minute, time_buf, sizeof(time_buf));
        snprintf(display_buf, sizeof(display_buf), "%s", grid_i18n(GRID_TEXT_ALARM));
        graphics_draw_text(ctx, display_buf, font, label_row, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                           NULL);
        graphics_draw_text(ctx, time_buf, font, value_row, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                           NULL);

        if (!self->display_timer_interval)
            return;

        row.origin.y += graphics_text_layout_get_content_size(time_buf, font, row, GTextOverflowModeTrailingEllipsis,
                                                              GTextAlignmentCenter)
                            .h +
                        ALARM_ROW_GAP;
        label_row.origin.y = row.origin.y;
        value_row.origin.y = row.origin.y;
    }

    time_t remaining = self->ringing && self->active_timer ? 0 : self->display_timestamp - self->display_now;

    if (remaining < 0)
        remaining = 0;

    if (remaining >= 60) {
        uint16_t minutes  = (uint16_t)((remaining + 59) / 60);
        uint16_t interval = self->display_timer ? self->display_minute : self->display_timer_interval;

        // Ignore small phone/watch clock differences without masking real clock corrections.
        if (minutes > interval && remaining <= (time_t)interval * 60 + 2)
            minutes = interval;

        snprintf(time_buf, sizeof(time_buf), "%02u:%02u", minutes / 60, minutes % 60);
    } else {
        snprintf(time_buf, sizeof(time_buf), "00:%02u", (unsigned int)remaining);
    }

    snprintf(display_buf, sizeof(display_buf), "%s", grid_i18n(GRID_TEXT_TIMER));
    graphics_draw_text(ctx, display_buf, font, label_row, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
    graphics_draw_text(ctx, time_buf, font, value_row, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
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

static bool alarm_cleanup(Settings *settings)
{
    if (!settings)
        return false;

    AlarmCollectionSettings *alarms  = &settings->alarms;
    uint8_t                  write   = 0;
    bool                     changed = false;

    for (uint8_t read = 0; read < alarms->count; read++) {
        AlarmSettings *alarm = &alarms->items[read];

        if (alarm->repeat_mode != ALARM_REPEAT_DAYS && !alarm->enabled) {
            changed = true;
            continue;
        }

        if (write != read)
            alarms->items[write] = *alarm;

        write++;
    }

    if (!changed)
        return false;

    alarms->count = write;
    memset(&alarms->items[write], 0, sizeof(alarms->items[0]) * (GRID_ALARM_MAX_COUNT - write));
    return true;
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
        self->active_timer  = alarm->repeat_mode == ALARM_REPEAT_TIMER;
        self->last_vibe_ms  = 0;
        self->ringing       = true;

        alarm_vibe_pulse(self);
        alarm_refresh_display(self, NULL, NULL, time(NULL));
        return true;
    }

    return false;
}

static void alarm_reconcile_schedule(Alarm *self, time_t now);

static void alarm_stop_current(Alarm *self)
{
    if (!self || !self->ringing)
        return;

    alarm_vibe_stop(self);
    self->ringing = false;

    if (!alarm_start_next(self)) {
        Settings updated = self->settings_store->value;

        if (alarm_cleanup(&updated) && !settings_commit(self->settings_store, &updated))
            APP_LOG(APP_LOG_LEVEL_WARNING, "Alarm settings persist failed");

        alarm_reconcile_schedule(self, time(NULL));
    }
}

static void alarm_forget_wakeup(Alarm *self)
{
    if (!self)
        return;

    self->wakeup_id = ALARM_INVALID_WAKEUP_ID;
}

static void alarm_clear_persisted_wakeup(void)
{
    if (persist_exists(ALARM_WAKEUP_PERSIST_KEY))
        persist_delete(ALARM_WAKEUP_PERSIST_KEY);
}

static void alarm_restore_wakeup(Alarm *self)
{
    if (!self)
        return;

    alarm_forget_wakeup(self);

    if (!persist_exists(ALARM_WAKEUP_PERSIST_KEY))
        return;

    WakeupId wakeup_id = (WakeupId)persist_read_int(ALARM_WAKEUP_PERSIST_KEY);

    if (wakeup_id < 0) {
        alarm_clear_persisted_wakeup();
        return;
    }

    self->wakeup_id = wakeup_id;
}

static void alarm_cancel_wakeup(Alarm *self)
{
    if (!self)
        return;

    if (self->wakeup_id >= 0)
        wakeup_cancel(self->wakeup_id);

    alarm_forget_wakeup(self);
    alarm_clear_persisted_wakeup();
}

static void alarm_schedule_next_wakeup(Alarm *self, const AlarmNext *next)
{
    if (!self || !self->settings_store || !next)
        return;

    if (self->wakeup_id >= 0) {
        time_t scheduled_timestamp = 0;

        if (!wakeup_query(self->wakeup_id, &scheduled_timestamp)) {
            alarm_forget_wakeup(self);
        } else {
            if (next->valid && scheduled_timestamp == next->timestamp)
                return;

            alarm_cancel_wakeup(self);
        }
    }

    if (!next->valid) {
        alarm_clear_persisted_wakeup();
        return;
    }

    // Preserve the existing alarm semantics: a watch powered on after the alarm
    // time must not produce a late notification for a missed event.
    WakeupId wakeup_id = wakeup_schedule(next->timestamp, ALARM_WAKEUP_COOKIE, false);

    if (wakeup_id < 0) {
        alarm_clear_persisted_wakeup();
        APP_LOG(APP_LOG_LEVEL_WARNING, "Alarm wakeup schedule failed: %ld", (long)wakeup_id);
        return;
    }

    self->wakeup_id = wakeup_id;

    status_t persist_result = persist_write_int(ALARM_WAKEUP_PERSIST_KEY, wakeup_id);

    if (persist_result < 0)
        APP_LOG(APP_LOG_LEVEL_WARNING, "Alarm wakeup id persist failed: %ld", (long)persist_result);
}

static void alarm_reconcile_schedule(Alarm *self, time_t now)
{
    if (!self || !self->settings_store)
        return;

    AlarmNext next;
    AlarmNext next_timer;
    AlarmNext next_alarm;

    alarm_find_next(self, now + 1, &next, &next_timer, &next_alarm);
    alarm_schedule_next_wakeup(self, &next);

    self->next_timer_due = next_timer.valid ? next_timer.timestamp : 0;
    time_t remaining     = self->next_timer_due - now;

    // Second ticks also protect nearby timers when an earlier signal is still ringing.
    clock_set_second_ticks(&self->settings_store->app->clock, self->next_timer_due && remaining > 0 && remaining < 120);

    alarm_refresh_display(self, &next_alarm, &next_timer, now);
}

static void alarm_process_due(Alarm *self, time_t now)
{
    if (!self || !self->settings_store)
        return;

    time_t epoch_minute       = now / 60;
    bool   check_clock_alarms = self->last_checked_minute != epoch_minute;

    struct tm *current = localtime(&now);

    if (!current)
        return;

    self->last_checked_minute = epoch_minute;

    Settings updated          = self->settings_store->value;
    time_t   minute_start     = now - current->tm_sec;
    bool     settings_changed = false;

    for (uint8_t i = 0; i < updated.alarms.count; i++) {
        AlarmSettings *alarm = &updated.alarms.items[i];

        if (!alarm->enabled)
            continue;

        if (alarm->repeat_mode != ALARM_REPEAT_DAYS) {
            time_t alarm_time;

            if (!alarm_once_time(alarm, &alarm_time) ||
                (alarm->repeat_mode == ALARM_REPEAT_TIMER ? alarm_time <= now - 60 : alarm_time < minute_start)) {
                alarm->enabled   = 0;
                settings_changed = true;
                continue;
            }

            if (alarm->repeat_mode == ALARM_REPEAT_TIMER && alarm_time > now)
                continue;
        }

        if (alarm->repeat_mode != ALARM_REPEAT_TIMER && (!check_clock_alarms || !alarm_matches_now(alarm, current)))
            continue;

        self->pending_mask |= (uint16_t)(1u << i);

        if (alarm->repeat_mode != ALARM_REPEAT_DAYS) {
            alarm->enabled   = 0;
            settings_changed = true;
        }
    }

    // User-visible reaction comes first. Pending bits refer to the current alarm
    // indices, so one-shot entries stay in place until the pending queue drains.
    alarm_start_next(self);

    if (!self->ringing && !self->pending_mask)
        settings_changed = alarm_cleanup(&updated) || settings_changed;

    if (settings_changed && !settings_commit(self->settings_store, &updated))
        APP_LOG(APP_LOG_LEVEL_WARNING, "Alarm settings persist failed");
}

static void alarm_handle_wakeup_event(Alarm *self, WakeupId wakeup_id, int32_t cookie, time_t now)
{
    if (!self || cookie != ALARM_WAKEUP_COOKIE)
        return;

    if (self->wakeup_id == wakeup_id)
        alarm_forget_wakeup(self);

    // Processing happens before replacing the persisted WakeupId so the first
    // alarm pulse is never delayed by persistent-storage housekeeping.
    alarm_process_due(self, now);
    alarm_reconcile_schedule(self, now);
}

static void alarm_wakeup_handler(WakeupId wakeup_id, int32_t cookie)
{
    App *app = app_from_active_window();

    if (!app || !app->mounted)
        return;

    alarm_handle_wakeup_event(&app->alarm, wakeup_id, cookie, time(NULL));
}

bool alarm_init(Alarm *self, Layer *root, Layer *details, SettingsStore *settings_store, const ScreenGeometry *geometry)
{
    if (!self || !root || !details || !settings_store || !geometry)
        return false;

    self->settings_store      = settings_store;
    self->details_layer       = details;
    self->last_checked_minute = (time_t)-1;
    self->wakeup_id           = ALARM_INVALID_WAKEUP_ID;

    // Finish cleanup if the app previously exited while an alarm was ringing.
    Settings updated = self->settings_store->value;

    if (alarm_cleanup(&updated) && !settings_commit(self->settings_store, &updated))
        APP_LOG(APP_LOG_LEVEL_WARNING, "Alarm settings persist failed");

    GRect display_bounds = geometry->weather;

    display_bounds.size.h = 2 * display_bounds.size.h + ALARM_ROW_GAP;
    self->layer           = layer_create_with_data(display_bounds, sizeof(Alarm *));

    if (!self->layer)
        goto fail;

    *(Alarm **)layer_get_data(self->layer) = self;
    layer_set_update_proc(self->layer, alarm_layer_update_proc);
    layer_add_child(root, self->layer);

    alarm_restore_wakeup(self);
    wakeup_service_subscribe(alarm_wakeup_handler);
    return true;

fail:
    alarm_deinit(self);
    return false;
}

bool alarm_handle_launch(Alarm *self, time_t now)
{
    if (!self || launch_reason() != APP_LAUNCH_WAKEUP)
        return false;

    WakeupId wakeup_id = ALARM_INVALID_WAKEUP_ID;
    int32_t  cookie    = 0;

    if (!wakeup_get_launch_event(&wakeup_id, &cookie) || cookie != ALARM_WAKEUP_COOKIE)
        return false;

    alarm_handle_wakeup_event(self, wakeup_id, cookie, now);
    return true;
}

void alarm_settings_changed(Alarm *self)
{
    if (!self)
        return;

    // Pending bits refer to indices in the previous configuration. A ringing alarm has copied what it needs.
    self->pending_mask = 0;
    alarm_reconcile_schedule(self, time(NULL));
}

void alarm_tick(Alarm *self, time_t now)
{
    if (!self || !self->settings_store)
        return;

    alarm_process_due(self, now);
    alarm_reconcile_schedule(self, now);
}

void alarm_countdown_tick(Alarm *self, time_t now)
{
    if (!self)
        return;

    // Delivery is independent of the countdown layer, including while a prior signal rings.
    if (self->next_timer_due && now >= self->next_timer_due) {
        alarm_tick(self, now);
        return;
    }

    if (!self->display_valid || (!self->display_timer && !self->display_timer_interval) ||
        (self->ringing && self->active_timer))
        return;

    time_t remaining = self->display_timestamp - now;

    if (remaining <= 0) {
        alarm_tick(self, now);
    } else {
        bool clock_adjusted = self->display_now - now > 1;

        if (now > self->display_now || clock_adjusted)
            self->display_now = now;

        if (clock_adjusted)
            alarm_reconcile_schedule(self, now);
        else if (remaining <= 60 && self->layer)
            layer_mark_dirty(self->layer);
    }
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

    self->layer                  = NULL;
    self->details_layer          = NULL;
    self->settings_store         = NULL;
    self->pending_mask           = 0;
    self->active_minute          = 0;
    self->display_minute         = 0;
    self->display_timer_interval = 0;
    self->display_timestamp      = 0;
    self->display_now            = 0;
    self->next_timer_due         = 0;
    self->last_vibe_ms           = 0;
    self->vibe_timer             = NULL;
    self->last_checked_minute    = (time_t)-1;
    self->display_valid          = false;
    self->display_timer          = false;
    self->active_timer           = false;
    self->ringing                = false;
    alarm_forget_wakeup(self);

    // Wakeup has no unsubscribe API. Do not cancel the scheduled system event:
    // it must survive foreground application shutdown and be able to relaunch //GRID.
    if (layer)
        layer_destroy(layer);
}
