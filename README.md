# //GRID

//GRID is a small, minimalist watchface for Pebble Time 2 and Pebble 2 Duo.
It takes visual cues from TRON: a black background, cyan geometry, a simple
screen grid, and large LCD-style digits.

The idea is simple. Most of the time, the watchface shows only the current
time. Flick your wrist and the screen plays a short Doppler animation, then
opens the full view with the date, battery status, health data, and sleep time.

## Highlights

- Clean idle screen with time only
- Doppler animation triggered by a wrist flick
- Configurable glance duration from 3 to 30 seconds
- 12-hour and 24-hour time formats
- Date shown in bracket notation
- Segmented battery bar with percentage
- Steps, calories, distance, sleep, and heart rate where supported
- Separate layouts for Pebble Time 2 and Pebble 2 Duo
- Persistent settings stored on the watch
- No background network requests or JavaScript timers

## Supported watches

### Pebble Time 2

Platform: `emery`

The full view shows:

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

During normal use, only the current time is visible. The watchface wakes once per
minute to update it.

### Glance screen

A wrist flick starts the Doppler animation. When the animation ends, the full
view stays on screen for the configured number of seconds.

While the animation or glance screen is already active, extra flick events are
ignored. This avoids restarting the animation, extending the glance timer, or
repeatedly requesting the backlight.

The backlight is enabled only when it is currently off.

## Health data

Health values are kept in a small in-memory cache.

The cache is filled when the watchface starts and updated when Pebble reports a
movement, heart-rate, sleep, or significant health event. Opening the glance
screen only draws the cached values; it does not read every health metric again.

If a value is not available, the watchface shows `--`.

## Battery display

The battery bar has ten segments.

Above 10 percent, the segments show the approximate charge in 10-percent steps.
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

Available option:

- `Glance duration`: 3 to 30 seconds, default 7 seconds

A saved value is used the next time the animation starts. It does not change a
glance that is already running.

## Power use

The watchface tries to do as little work as possible while idle:

- The time is updated once per minute.
- Date and battery values are refreshed only before a glance starts.
- Health values are updated through HealthService events.
- Hidden health layers are not redrawn after every health event.
- Doppler timers exist only while the animation or glance is active.
- Repeated flicks are ignored until the current glance finishes.
- The backlight is requested only when it is off.
- The JavaScript side has no polling, background timers, or network traffic.

## Building

For current SDK installation, setup, build, emulator, and device installation
instructions, see the official Pebble SDK documentation:

[Installing the Pebble SDK](https://developer.repebble.com/sdk/)

For a practical C watchface example, see:

[C Watchface Tutorial](https://github.com/coredevices/c-watchface-tutorial)

The project targets `emery` (Pebble Time 2) and `flint` (Pebble 2 Duo).
