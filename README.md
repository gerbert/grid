# //GRID

//GRID is a small, minimalist watchface for Pebble Time 2 and Pebble 2 Duo.
It takes visual cues from TRON: a black background, cyan geometry, a simple
screen grid, and large LCD-style digits.

The idea is simple. Most of the time, the watchface shows only the current
time. Flick your wrist and the screen plays a short Doppler animation, then
opens the full view with the weather, date, battery status, health data, and
sleep time.

## Highlights

- Clean idle screen with time and the next active alarm when configured
- Doppler animation triggered by a wrist flick
- Configurable glance duration from 3 to 30 seconds
- Optional Do not disturb interval that suppresses glance activation
- Up to 5 persistent one-time or recurring alarms
- Optional 12-hour weather forecast with a selectable provider
- Text, icon, combined icon-and-text, or temperature-only weather display
- Configurable weather update interval from 1 to 6 hours
- Configurable failed-update retry interval from 1 to 30 minutes
- 12-hour and 24-hour time formats
- Date shown in bracket notation
- Segmented battery bar with percentage
- Steps, calories, distance, sleep, and heart rate where supported
- Separate layouts for Pebble Time 2 and Pebble 2 Duo
- Persistent settings stored on the watch
- Weather values stored only in watch RAM

## Supported watches

### Pebble Time 2

Platform: `emery`

The full view shows:

- Weather forecast for the current hour when enabled
- Steps (`STP`)
- Heart rate (`BPM`)
- Active calories (`CAL`)
- Distance in kilometers (`KM`)
- Sleep duration
- Date
- Battery level

Pebble Time 2 uses the full color layout, including the lower grid section and
color-coded battery warnings.

### Pebble 2 Duo

Platform: `flint`

The full view shows:

- Weather forecast for the current hour when enabled
- Steps (`STP`)
- Active calories (`CAL`)
- Distance in kilometers (`KM`)
- Sleep duration
- Date
- Battery level

Heart rate is not shown because Pebble 2 Duo does not have an optical heart-rate
sensor. Its smaller display uses a more compact monochrome layout and leaves out
the lower grid section.

## How it works

### Idle screen

During normal use, the current time is visible. When at least one alarm is enabled,
the nearest active alarm is shown directly below it as `Next alarm HH:MM` (or the
corresponding 12-hour representation). Time is updated once per minute. The same tick
performs a cheap alarm reconciliation before weather housekeeping, while the nearest
active alarm is also scheduled through Pebble Wakeup for an independent trigger.

### Glance screen

A wrist flick starts the Doppler animation. When the animation ends, the full
view stays on screen for the configured number of seconds.

While the animation or glance screen is already active, extra flick events are
ignored. This avoids restarting the animation, extending the glance timer, or
repeatedly requesting the backlight.

The backlight is enabled only when it is currently off.

When Do not disturb is enabled, new glance activation is suppressed during the
configured local-time interval. Intervals may cross midnight. The start time is
inclusive and the end time is exclusive; equal start and end times define an
empty interval. An already active glance is not interrupted.

## Alarms

Alarms are configured on the phone and stored together with the persistent watchface
settings. Up to five active alarms can be configured. Alarm vibration uses the
firmware-provided long pulse and repeats continuously until the alarm is reset.

A one-time alarm uses the next occurrence of the selected local time and is disabled after it fires. Recurring
alarms can run every day, on weekdays, on weekends, or on any selected combination
of weekdays. Selecting a single weekday therefore creates a weekly alarm for that
day.

The watch schedules one Pebble Wakeup for the nearest active alarm. When the Wakeup
arrives, all alarms due in that local minute are queued in configuration order and the
first firmware long pulse starts immediately. One-time disable state is persisted only
after that first user-visible reaction, and the next nearest Wakeup is then scheduled.
The minute tick keeps a cheap reconciliation path for startup, clock/timezone changes,
and Wakeup scheduling failures. Deduplication by epoch minute prevents a Wakeup and the
minute tick from triggering the same alarm twice. While an alarm is already ringing, a
short-lived AppTimer only repeats the firmware long pulse; it has no role in deciding
when an alarm fires.

While an alarm is ringing, the normal wrist flick gesture is reserved for stopping it.
A single accepted flick stops the current alarm. Outside the ringing state, flick behavior
is unchanged and a single flick activates the normal glance screen. Snooze is not implemented.

## Weather

Weather is disabled by default. The provider is selected in the settings.
Open-Meteo is currently available.

When enabled, PebbleKit JS obtains the phone location and asks the selected
provider for up to twelve hourly forecast points. Provider-specific values,
including weather codes, are normalized on the phone. The compact payload
contains the number of forecast entries, the timestamp of the first hourly
entry, and temperature/condition pairs. Consecutive entries implicitly
represent one-hour intervals.

The watch does not interpret provider-specific weather codes. Each received
forecast entry is mapped directly into one of twelve in-memory slots using its
Unix hour modulo 12. The current weather is selected using the same mapping for
the current hour. A matching C enum is used only to select the display text, an
icon, both, or only the temperature. Each display is centered using its measured
content width. In the combined mode, a long condition name is wrapped into two
lines to the right of the temperature and icon. Icon modes use selected glyphs
from Weather Icons 2.0.12. The original TTF is used without modification, while
the Pebble resource configuration limits conversion to the 16 condition glyphs
used by the forecast display and the `wi-cloud-refresh` status glyph. The selected
glyphs are rasterized at platform-specific sizes for Pebble Time 2 and Pebble 2 Duo.

No weather values are written to persistent watch storage. Restarting the
watchface clears the forecast until the next successful synchronization.

After weather is enabled, the existing minute tick is the only watch-side
scheduler. It starts an update when `next_update_at` is due and immediately moves
that timestamp forward by the retry interval. This provides unlimited retries
even when no response is received. A successful forecast replaces the in-memory
slots, marks the received slots as valid, records the synchronization time, and
schedules the next regular update. If an update fails or the received forecast
cannot be applied, the next attempt is scheduled using the retry interval.
Existing valid forecast slots are left untouched, so cached weather can remain
available while retries continue.

The wrist gesture never starts a weather request. The weather layer only reads
the current normalized in-memory slot.

## Health data

Health values are kept in a small in-memory cache.

The cache is filled when the watchface starts and updated when Pebble reports a
movement, heart-rate, sleep, or significant health event. Opening the glance
screen only draws the cached values; it does not read every health metric again.

If a value is not available, the watchface shows `--`.

## Battery display

The battery bar has ten segments.

Above 10 percent, the segments show the charge in 10-percent steps. The
current incomplete step remains visible with a dim pattern instead of disappearing.
At 10 percent or below, each segment represents one percent, which makes the
last part of the battery easier to read.

On Pebble Time 2, the color changes with the remaining charge:

- Above 30 percent: cyan
- 21 to 30 percent: green
- 16 to 20 percent: yellow
- 11 to 15 percent: orange
- 10 percent or below: red

Pebble 2 Duo uses the monochrome system palette.

## Settings

The configuration page is built with Rebble Clay.

Available options:

- `Glance duration`: 3 to 30 seconds, default 7 seconds
- `Do not disturb`: disabled by default; native time controls are displayed as
  `DnD: [time] -> [time]`, defaulting to `22:00` through `07:00` (the browser
  renders each picker according to locale)
  DnD suppresses glance activation only; it never suppresses a ringing alarm or its flick-to-reset handling.
- `Alarms`: add, edit, and delete up to 5 active alarms in-place
- A ringing alarm is reset by a single normal wrist flick; this is not configurable
- Per-alarm schedule: one time, every day, weekdays, weekends, or selected days
- `Enable weather`: disabled by default
- `Provider`: Open-Meteo by default
- `Condition display`: text by default, with optional icon, icon-plus-text,
  and temperature-only modes
- `Weather update interval`: 1 to 6 hours, default 3 hours
- `Weather retry interval`: 1 to 30 minutes, default 30 minutes

The watch stores only the numeric provider index. Provider selection and API
handling are implemented in PebbleKit JS.

## Power use

The watchface tries to do as little work as possible while idle:

- The time is updated once per minute.
- The minute tick performs alarm reconciliation before the weather timestamp comparison.
- Only the nearest active alarm has a Pebble Wakeup scheduled; no fast polling is used.
- A runtime AppTimer exists only while an alarm is already ringing to repeat the system long pulse.
- Weather requests occur only when enabled and due.
- Twelve forecast slots allow the displayed value to advance without another
  phone request.
- Date and battery values are refreshed only before a glance starts.
- Health values are updated through HealthService events.
- Hidden health layers are not redrawn after every health event.
- Doppler timers exist only while the animation or glance is active.
- Repeated flicks are ignored until the current glance finishes, except while an alarm is ringing.
- The backlight is requested only when it is off.
- PebbleKit JS has no polling or background interval timer.

## Building

For current SDK installation, setup, build, emulator, and device installation
instructions, see the official Pebble SDK documentation:

[Installing the Pebble SDK](https://developer.repebble.com/sdk/)

For a practical C watchface example, see:

[C Watchface Tutorial](https://github.com/coredevices/c-watchface-tutorial)

The project targets `emery` (Pebble Time 2) and `flint` (Pebble 2 Duo).

### Weather icon font

Icon mode uses the original, unmodified Weather Icons 2.0.12 TTF by Erik
Flowers. The original v1.0 icon designs are credited to Lukas Bischoff; icon art
from v1.1 onward and maintenance of Weather Icons are credited to Erik Flowers.

Project and source:

- https://erikflowers.github.io/weather-icons/
- https://github.com/erikflowers/weather-icons
- https://cdnjs.cloudflare.com/ajax/libs/weather-icons/2.0.12/font/weathericons-regular-webfont.ttf

The font is licensed under the SIL Open Font License 1.1. The complete license
text and third-party notice are stored in:

- `licenses/weather-icons/OFL-1.1.txt`
- `licenses/weather-icons/NOTICE.md`

The original TTF is stored in the project at:

```text
resources/fonts/weathericons-regular-webfont.ttf
```

The bundled file is the unmodified Weather Icons 2.0.12 release asset. Its
SHA-256 digest is:

```text
176bda6661f213dde47c2114d76e476ec8ca9aae07dd54f9550d2d28fe02b4fd
```

The full upstream TTF is about 97 KiB in the source tree. It is not copied whole
into the installed watch application: `package.json` lists the 16 private-use
Unicode condition glyphs used by `//GRID`, together with the `wi-cloud-refresh`
glyph used to indicate stale weather data. The Pebble SDK converts only those glyphs
into the platform-specific font resource at 24 px for `emery` and 18 px for `flint`.

## License

Except for the third-party components listed in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), `//GRID` is licensed
under the GNU General Public License v2.0 only (`GPL-2.0-only`).

See [`LICENSE`](LICENSE) for the complete license text.
