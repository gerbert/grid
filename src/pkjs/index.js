var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var messageKeys = require('message_keys');
var weatherProviders = require('./providers');
var weatherAdapter = require('./adapters');
var alarms = require('./alarms');
function customClay() {
    var clayConfig = this;
    var dndTimeRow;
    var MAX_ALARMS = 5;
    var REPEAT_ONCE = 0;
    var REPEAT_SELECTED = 4;
    var DAYS_WEEKDAYS = 0x3e;
    var DAY_BITS = [
        1 << 1,
        1 << 2,
        1 << 3,
        1 << 4,
        1 << 5,
        1 << 6,
        1 << 0
    ];

    function updateGroup(group) {
        var items = clayConfig.getItemsByGroup(group);
        var visible = this.get();
        var i;

        for (i = 0; i < items.length; i += 1) {
            if (visible) {
                items[i].show();
            } else {
                items[i].hide();
            }
        }
    }

    function bindGroupToggle(messageKey, group) {
        var toggle = clayConfig.getItemByMessageKey(messageKey);

        if (!toggle) {
            return;
        }

        updateGroup.call(toggle, group);
        toggle.on('change', function() {
            updateGroup.call(toggle, group);
        });
    }

    function layoutDndTimeRange() {
        var start = clayConfig.getItemByMessageKey('DND_START_TIME');
        var end = clayConfig.getItemByMessageKey('DND_END_TIME');
        var startElement;
        var endElement;
        var parent;
        var startControl;
        var endControl;
        var startLabel;
        var endLabel;
        var startInputContainer;
        var endInputContainer;
        var startInput;
        var endInput;
        var dndLabel;
        var timeGroup;
        var separator;

        if (!start || !end) {
            return;
        }

        startElement = start.$element[0];
        endElement = end.$element[0];
        if (!startElement || !endElement || startElement.parentNode !== endElement.parentNode) {
            return;
        }

        startControl = startElement.querySelector('label');
        endControl = endElement.querySelector('label');
        startLabel = startElement.querySelector('.label');
        endLabel = endElement.querySelector('.label');
        startInputContainer = startElement.querySelector('.input');
        endInputContainer = endElement.querySelector('.input');
        startInput = startElement.querySelector('input[type="time"]');
        endInput = endElement.querySelector('input[type="time"]');
        if (!startControl || !endControl || !startInputContainer || !endInputContainer ||
            !startInput || !endInput) {
            return;
        }

        parent = startElement.parentNode;
        dndTimeRow = document.createElement('div');
        dndTimeRow.className = 'component dnd-time-row';
        dndTimeRow.style.display = 'flex';
        dndTimeRow.style.alignItems = 'center';
        dndTimeRow.style.whiteSpace = 'nowrap';

        dndLabel = document.createElement('span');
        dndLabel.textContent = 'DnD:';
        dndLabel.style.flex = '0 0 auto';
        dndLabel.style.marginRight = '6px';

        timeGroup = document.createElement('div');
        timeGroup.className = 'dnd-time-group';
        timeGroup.style.display = 'flex';
        timeGroup.style.alignItems = 'center';
        timeGroup.style.justifyContent = 'flex-end';
        timeGroup.style.marginLeft = 'auto';
        timeGroup.style.minWidth = '0';

        separator = document.createElement('span');
        separator.textContent = '\u2192';
        separator.style.alignSelf = 'stretch';
        separator.style.display = 'flex';
        separator.style.alignItems = 'center';
        separator.style.justifyContent = 'center';
        separator.style.flex = '0 0 auto';
        separator.style.margin = '0 6px';

        parent.insertBefore(dndTimeRow, startElement);
        dndTimeRow.appendChild(dndLabel);
        dndTimeRow.appendChild(timeGroup);
        timeGroup.appendChild(startElement);
        timeGroup.appendChild(separator);
        timeGroup.appendChild(endElement);

        startElement.style.display = 'flex';
        startElement.style.alignItems = 'center';
        startElement.style.flex = '0 1 6.25em';
        startElement.style.width = '6.25em';
        startElement.style.maxWidth = '6.25em';
        startElement.style.minWidth = '0';
        startElement.style.margin = '0';
        startElement.style.padding = '0';
        startElement.style.border = '0';
        startElement.style.boxSizing = 'border-box';

        endElement.style.display = 'flex';
        endElement.style.alignItems = 'center';
        endElement.style.flex = '0 1 6.25em';
        endElement.style.width = '6.25em';
        endElement.style.maxWidth = '6.25em';
        endElement.style.minWidth = '0';
        endElement.style.margin = '0';
        endElement.style.padding = '0';
        endElement.style.border = '0';
        endElement.style.boxSizing = 'border-box';

        startControl.style.display = 'flex';
        startControl.style.alignItems = 'center';
        startControl.style.width = '100%';
        startControl.style.margin = '0';
        startControl.style.padding = '0';

        endControl.style.display = 'flex';
        endControl.style.alignItems = 'center';
        endControl.style.width = '100%';
        endControl.style.margin = '0';
        endControl.style.padding = '0';

        if (startLabel) {
            startLabel.style.display = 'none';
        }
        if (endLabel) {
            endLabel.style.display = 'none';
        }

        startInputContainer.style.display = 'flex';
        startInputContainer.style.alignItems = 'center';
        startInputContainer.style.width = '100%';
        startInputContainer.style.minWidth = '0';
        startInputContainer.style.maxWidth = 'none';
        startInputContainer.style.margin = '0';

        endInputContainer.style.display = 'flex';
        endInputContainer.style.alignItems = 'center';
        endInputContainer.style.width = '100%';
        endInputContainer.style.minWidth = '0';
        endInputContainer.style.maxWidth = 'none';
        endInputContainer.style.margin = '0';

        startInput.style.display = 'block';
        startInput.style.width = '100%';
        startInput.style.maxWidth = '100%';
        startInput.style.minWidth = '0';
        startInput.style.margin = '0';
        startInput.style.boxSizing = 'border-box';
        startInput.setAttribute('aria-label', 'Do not disturb start time');

        endInput.style.display = 'block';
        endInput.style.width = '100%';
        endInput.style.maxWidth = '100%';
        endInput.style.minWidth = '0';
        endInput.style.margin = '0';
        endInput.style.boxSizing = 'border-box';
        endInput.setAttribute('aria-label', 'Do not disturb end time');
    }

    function updateDndTimeRow() {
        if (!dndTimeRow) {
            return;
        }

        dndTimeRow.className = this.get() ?
            'component dnd-time-row' : 'component dnd-time-row hide';
    }

    function bindDndToggle() {
        var toggle = clayConfig.getItemByMessageKey('DND_ENABLE');

        if (!toggle) {
            return;
        }

        updateDndTimeRow.call(toggle);
        toggle.on('change', function() {
            updateDndTimeRow.call(toggle);
        });
    }

    function parseAlarmState(value) {
        var state = value;

        if (typeof state === 'string') {
            try {
                state = JSON.parse(state);
            } catch (error) {
                state = null;
            }
        }
        if (!state || typeof state !== 'object') {
            state = {};
        }
        if (!(state.a instanceof Array)) {
            state.a = [];
        }
        state.a = state.a.slice(0, MAX_ALARMS);
        return state;
    }

    function repeatValue(value) {
        var mode = value;
        if (typeof mode === 'string' && /^\d+$/.test(mode)) {
            mode = parseInt(mode, 10);
        }
        return mode >= REPEAT_ONCE && mode <= REPEAT_SELECTED ? mode : REPEAT_ONCE;
    }

    function maskToChecks(mask) {
        var values = [];
        var i;
        mask = typeof mask === 'number' ? mask : DAYS_WEEKDAYS;
        for (i = 0; i < DAY_BITS.length; i += 1) {
            values.push((mask & DAY_BITS[i]) !== 0);
        }
        return values;
    }

    function checksToMask(values) {
        var mask = 0;
        var i;
        values = values instanceof Array ? values : [];
        for (i = 0; i < DAY_BITS.length; i += 1) {
            if (values[i]) {
                mask |= DAY_BITS[i];
            }
        }
        return mask;
    }

    function bindAlarmRows() {
        var stateItem = clayConfig.getItemByMessageKey('ALARM_STATE');
        var addButton = clayConfig.getItemById('ALARM_ADD_BUTTON');
        var addButtonTarget;
        var addButtonElement;
        var addButtonSlot;
        var state;
        var table;
        var body;
        var parent;
        var rows = [];
        var syncing = false;
        var i;

        if (!stateItem || !addButton) {
            return;
        }

        state = parseAlarmState(stateItem.get());
        stateItem.hide();

        function saveState() {
            if (!syncing) {
                stateItem.set(JSON.stringify(state));
            }
        }

        function setRowVisible(row, visible) {
            if (visible) {
                row.repeat.show();
                row.time.show();
                row.tr.removeAttribute('hidden');
            } else {
                row.repeat.hide();
                row.time.hide();
                row.days.hide();
                row.tr.setAttribute('hidden', 'hidden');
                row.daysTr.setAttribute('hidden', 'hidden');
                return;
            }

            if (repeatValue(row.repeat.get()) === REPEAT_SELECTED) {
                row.days.show();
                row.daysTr.removeAttribute('hidden');
            } else {
                row.days.hide();
                row.daysTr.setAttribute('hidden', 'hidden');
            }
        }

        function refreshRows() {
            var alarm;

            syncing = true;
            for (i = 0; i < MAX_ALARMS; i += 1) {
                if (i < state.a.length) {
                    alarm = state.a[i] || {};
                    rows[i].repeat.set(repeatValue(alarm.r));
                    rows[i].time.set(typeof alarm.t === 'string' ? alarm.t : '07:00');
                    rows[i].days.set(maskToChecks(alarm.m));
                    setRowVisible(rows[i], true);
                } else {
                    setRowVisible(rows[i], false);
                }
            }
            syncing = false;

            if (state.a.length >= MAX_ALARMS) {
                if (addButtonTarget.parentNode === addButtonSlot) {
                    addButtonSlot.removeChild(addButtonTarget);
                }
            } else if (addButtonTarget.parentNode !== addButtonSlot) {
                addButtonSlot.appendChild(addButtonTarget);
            }
        }

        function updateAlarm(index) {
            if (syncing || index >= state.a.length) {
                return;
            }

            state.a[index].e = 1;
            state.a[index].r = repeatValue(rows[index].repeat.get());
            state.a[index].t = rows[index].time.get();
            state.a[index].m = checksToMask(rows[index].days.get());
            delete state.a[index].d;
            saveState();
            setRowVisible(rows[index], true);
        }

        // Hide every predeclared alarm item before any layout work. If the
        // layout cannot be assembled, unused alarms must never leak into the
        // configuration page as ordinary Clay controls.
        for (i = 0; i < MAX_ALARMS; i += 1) {
            var initialRepeat = clayConfig.getItemById('ALARM_REPEAT_' + i);
            var initialTime = clayConfig.getItemById('ALARM_TIME_' + i);
            var initialDays = clayConfig.getItemById('ALARM_DAYS_' + i);
            var initialRemove = clayConfig.getItemById('ALARM_DELETE_' + i);

            if (!initialRepeat || !initialTime || !initialDays || !initialRemove) {
                return;
            }

            initialRepeat.hide();
            initialTime.hide();
            initialDays.hide();
            initialRemove.hide();
        }

        for (i = 0; i < MAX_ALARMS; i += 1) {
            var repeat = clayConfig.getItemById('ALARM_REPEAT_' + i);
            var time = clayConfig.getItemById('ALARM_TIME_' + i);
            var days = clayConfig.getItemById('ALARM_DAYS_' + i);
            var remove = clayConfig.getItemById('ALARM_DELETE_' + i);
            var repeatElement = repeat.$element[0];
            var timeElement = time.$element[0];
            var daysElement = days.$element[0];
            var removeElement = remove.$element[0];
            var tr;
            var daysTr;
            var repeatCell;
            var spacerTwo;
            var timeCell;
            var spacerThree;
            var removeCell;
            var daysCell;

            if (!repeatElement || !timeElement || !daysElement || !removeElement) {
                return;
            }

            if (!parent) {
                parent = repeatElement.parentNode;
                if (!parent) {
                    return;
                }

                table = document.createElement('table');
                // Reuse Clay's own section component spacing for the complete
                // alarm row. This gives the row the same left/right gutters as
                // the rest of the configuration page without adding CSS.
                table.className = 'component';
                table.setAttribute('width', '100%');
                table.setAttribute('cellpadding', '0');
                table.setAttribute('cellspacing', '0');
                body = document.createElement('tbody');
                table.appendChild(body);
                parent.insertBefore(table, repeatElement);
            }

            tr = document.createElement('tr');
            tr.setAttribute('valign', 'middle');
            daysTr = document.createElement('tr');
            repeatCell = document.createElement('td');
            spacerTwo = document.createElement('td');
            timeCell = document.createElement('td');
            spacerThree = document.createElement('td');
            removeCell = document.createElement('td');
            daysCell = document.createElement('td');

            spacerTwo.innerHTML = '&nbsp;';
            spacerThree.innerHTML = '&nbsp;';

            repeatCell.setAttribute('align', 'left');
            repeatCell.setAttribute('valign', 'middle');
            spacerTwo.setAttribute('width', '1%');
            spacerTwo.setAttribute('nowrap', 'nowrap');
            spacerTwo.setAttribute('valign', 'middle');
            timeCell.setAttribute('width', '1%');
            timeCell.setAttribute('nowrap', 'nowrap');
            timeCell.setAttribute('valign', 'middle');
            spacerThree.setAttribute('width', '1%');
            spacerThree.setAttribute('nowrap', 'nowrap');
            spacerThree.setAttribute('valign', 'middle');
            removeCell.setAttribute('width', '1%');
            removeCell.setAttribute('nowrap', 'nowrap');
            removeCell.setAttribute('valign', 'middle');
            daysCell.setAttribute('colspan', '5');

            // Keep Repeat as Clay's native component-select so it has exactly
            // the same appearance as Weather Provider. The alarm select has no
            // visible label, so remove only the empty label span: with .value
            // as the sole flex item Clay's own layout places it at the left.
            var repeatLabel = repeatElement.querySelector('label');
            var repeatLabelText = repeatElement.querySelector('.label');
            var repeatValueElement = repeatElement.querySelector('.value');
            var repeatSelect = repeatElement.querySelector('select');

            // Match the existing DnD time field exactly: keep the complete
            // Clay component-input and apply the same compact time geometry.
            var timeControl = timeElement.querySelector('label');
            var timeLabel = timeElement.querySelector('.label');
            var timeInputContainer = timeElement.querySelector('.input');
            var timeInput = timeElement.querySelector('input[type="time"]');
            var removeButton = remove.$manipulatorTarget[0];

            if (!repeatLabel || !repeatValueElement || !repeatSelect ||
                !timeControl || !timeInputContainer || !timeInput || !removeButton) {
                return;
            }

            if (repeatLabelText && repeatLabelText.parentNode === repeatLabel) {
                repeatLabel.removeChild(repeatLabelText);
            }

            // The alarm row already provides the standard section gutter and
            // explicit inter-column spacing. Remove only Select's horizontal
            // label padding so Repeat starts at the row's true left edge and
            // does not add a second gap before Time.
            repeatLabel.style.paddingLeft = '0';
            repeatLabel.style.paddingRight = '0';

            // The row itself already carries Clay's standard component gutters.
            // Keep the native component-select styling, but remove the nested
            // generic component token so Repeat does not receive a second left
            // inset inside the alarm row.
            repeatElement.className = repeatElement.className.replace(
                /(^|\s)component(?=\s|$)/g, '$1'
            ).replace(/\s+/g, ' ').replace(/^\s+|\s+$/g, '');

            timeElement.style.display = 'flex';
            timeElement.style.alignItems = 'center';
            timeElement.style.flex = '0 1 6.25em';
            timeElement.style.width = '6.25em';
            timeElement.style.maxWidth = '6.25em';
            timeElement.style.minWidth = '0';
            timeElement.style.margin = '0';
            timeElement.style.padding = '0';
            timeElement.style.border = '0';
            timeElement.style.boxSizing = 'border-box';

            timeControl.style.display = 'flex';
            timeControl.style.alignItems = 'center';
            timeControl.style.width = '100%';
            timeControl.style.margin = '0';
            timeControl.style.padding = '0';

            if (timeLabel) {
                timeLabel.style.display = 'none';
            }

            timeInputContainer.style.display = 'flex';
            timeInputContainer.style.alignItems = 'center';
            timeInputContainer.style.width = '100%';
            timeInputContainer.style.minWidth = '0';
            timeInputContainer.style.maxWidth = 'none';
            timeInputContainer.style.margin = '0';

            timeInput.style.display = 'block';
            timeInput.style.width = '100%';
            timeInput.style.maxWidth = '100%';
            timeInput.style.minWidth = '0';
            timeInput.style.margin = '0';
            timeInput.style.boxSizing = 'border-box';

            // Clay's standard button has min-width: 12rem. Remove only that
            // geometry constraint so the primary delete button stays compact.
            removeButton.style.minWidth = '0';

            repeatCell.appendChild(repeatElement);
            timeCell.appendChild(timeElement);
            removeCell.appendChild(removeButton);
            tr.appendChild(repeatCell);
            tr.appendChild(spacerTwo);
            tr.appendChild(timeCell);
            tr.appendChild(spacerThree);
            tr.appendChild(removeCell);
            daysCell.appendChild(daysElement);
            daysTr.appendChild(daysCell);
            body.appendChild(tr);
            body.appendChild(daysTr);

            rows.push({
                repeat: repeat,
                time: time,
                days: days,
                remove: remove,
                tr: tr,
                daysTr: daysTr
            });
        }

        rows.forEach(function(row, index) {
            row.repeat.on('change', function() {
                updateAlarm(index);
            });
            row.time.on('change', function() {
                updateAlarm(index);
            });
            row.days.on('change', function() {
                updateAlarm(index);
            });
            row.remove.on('click', function() {
                if (index < state.a.length) {
                    state.a.splice(index, 1);
                    saveState();
                    refreshRows();
                }
            });
        });

        // Keep the existing Clay primary button, but place it in the section
        // heading so alarm creation stays on the same line as "Alarms".
        addButtonTarget = addButton.$manipulatorTarget[0];
        addButtonElement = addButton.$element[0];
        addButtonSlot = document.getElementById('alarm-add-slot');
        if (!addButtonTarget || !addButtonElement || !addButtonSlot) {
            return;
        }
        addButtonTarget.setAttribute('type', 'button');
        addButtonTarget.style.minWidth = '0';
        addButtonElement.setAttribute('hidden', 'hidden');
        addButtonSlot.appendChild(addButtonTarget);
        addButtonTarget.addEventListener('click', function(event) {
            if (event) {
                event.preventDefault();
            }
            if (state.a.length >= MAX_ALARMS) {
                return;
            }

            state.a.push({ e: 1, r: REPEAT_ONCE, t: '07:00', m: DAYS_WEEKDAYS });
            saveState();
            refreshRows();
        }, false);

        refreshRows();
    }

    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
        layoutDndTimeRange();
        bindGroupToggle('WEATHER_ENABLE', 'weather-details');
        bindDndToggle();
        bindAlarmRows();
    });
}

var clay = new Clay(clayConfig, customClay, { autoHandleEvents: false });

var WEATHER_REQUEST_TIMEOUT_MS = 15000;
var DND_DEFAULT_START_MINUTE = 22 * 60;
var DND_DEFAULT_END_MINUTE = 7 * 60;
var LOCATION_CACHE_MAX_AGE_MS = 6 * 60 * 60 * 1000;
var WEATHER_DISPLAY_TEXT = 0;
var WEATHER_DISPLAY_ICON = 1;
var WEATHER_DISPLAY_ICON_TEXT = 2;
var WEATHER_DISPLAY_TEMPERATURE = 3;
var weatherInProgress = false;

function integerNumber(value) {
    return typeof value === 'number' && isFinite(value) && Math.floor(value) === value;
}

function providerIdFromClay(value) {
    var providerId = value;

    if (typeof providerId === 'string' && /^\d+$/.test(providerId)) {
        providerId = parseInt(providerId, 10);
    }

    if (!weatherProviders.isValidId(providerId)) {
        return weatherProviders.defaultId;
    }

    return providerId;
}

function timeMinutesFromClay(value, defaultValue) {
    if (typeof value !== 'string') {
        return defaultValue;
    }

    var match = /^([01][0-9]|2[0-3]):([0-5][0-9])$/.exec(value);

    if (!match) {
        return defaultValue;
    }

    return parseInt(match[1], 10) * 60 + parseInt(match[2], 10);
}

function timeMinutesToClay(value) {
    var hour = Math.floor(value / 60);
    var minute = value % 60;

    return (hour < 10 ? '0' : '') + hour + ':' +
        (minute < 10 ? '0' : '') + minute;
}

function displayModeFromClay(value) {
    var displayMode = value;

    if (typeof displayMode === 'string' && /^\d+$/.test(displayMode)) {
        displayMode = parseInt(displayMode, 10);
    }

    if (displayMode !== WEATHER_DISPLAY_TEXT &&
        displayMode !== WEATHER_DISPLAY_ICON &&
        displayMode !== WEATHER_DISPLAY_ICON_TEXT &&
        displayMode !== WEATHER_DISPLAY_TEMPERATURE) {
        return WEATHER_DISPLAY_TEXT;
    }

    return displayMode;
}

function hasMessageKey(payload, key) {
    return Object.prototype.hasOwnProperty.call(payload, key);
}

function finishWeatherUpdate() {
    weatherInProgress = false;
}

function sendWeatherFailure(reason) {
    console.log('Weather update failed: ' + reason);
    finishWeatherUpdate();

    Pebble.sendAppMessage(
        { 'WEATHER_UPDATE_FAILED': 1 },
        function() { },
        function() {
            console.log('Unable to send weather failure to the watch');
        }
    );
}

function sendForecast(providerId, responseText) {
    var payload;

    try {
        payload = weatherAdapter.encode(providerId, responseText);
    } catch (error) {
        sendWeatherFailure('unable to process weather response');
        return;
    }

    if (!payload) {
        sendWeatherFailure('invalid weather forecast');
        return;
    }

    finishWeatherUpdate();

    Pebble.sendAppMessage(
        { 'WEATHER_FORECAST': payload },
        function() {
            console.log('Weather forecast sent to the watch');
        },
        function() {
            console.log('Unable to send weather forecast to the watch');
        }
    );
}

function requestForecast(providerId, position) {
    weatherProviders.request(
        providerId,
        position,
        weatherAdapter.slotCount,
        WEATHER_REQUEST_TIMEOUT_MS,
        function(error, responseText) {
            if (error) {
                sendWeatherFailure(error);
                return;
            }

            sendForecast(providerId, responseText);
        }
    );
}

function getWeather(providerId) {
    if (weatherInProgress) {
        return;
    }

    if (!integerNumber(providerId) || !weatherProviders.isValidId(providerId)) {
        sendWeatherFailure('unsupported weather provider');
        return;
    }

    weatherInProgress = true;

    try {
        navigator.geolocation.getCurrentPosition(
            function(position) {
                requestForecast(providerId, position);
            },
            function() {
                sendWeatherFailure('location unavailable');
            },
            {
                enableHighAccuracy: false,
                timeout: WEATHER_REQUEST_TIMEOUT_MS,
                maximumAge: LOCATION_CACHE_MAX_AGE_MS
            }
        );
    } catch (error) {
        sendWeatherFailure('unable to request location');
    }
}

Pebble.addEventListener('showConfiguration', function() {
    Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(event) {
    if (!event || !event.response) {
        return;
    }

    var settings;

    try {
        settings = clay.getSettings(event.response);
    } catch (error) {
        console.log('Unable to read settings');
        return;
    }

    var providerId = providerIdFromClay(settings[messageKeys.WEATHER_PROVIDER_ID]);
    var displayMode = displayModeFromClay(settings[messageKeys.WEATHER_DISPLAY_MODE]);
    var dndStartMinute = timeMinutesFromClay(
        settings[messageKeys.DND_START_TIME],
        DND_DEFAULT_START_MINUTE
    );
    var dndEndMinute = timeMinutesFromClay(
        settings[messageKeys.DND_END_TIME],
        DND_DEFAULT_END_MINUTE
    );

    var alarmState = alarms.normalizeState(settings[messageKeys.ALARM_STATE], true);
    settings[messageKeys.WEATHER_PROVIDER_ID] = providerId;
    settings[messageKeys.WEATHER_DISPLAY_MODE] = displayMode;
    settings[messageKeys.DND_START_TIME] = dndStartMinute;
    settings[messageKeys.DND_END_TIME] = dndEndMinute;
    settings[messageKeys.ALARM_CONFIG] = alarms.encode(alarmState);
    delete settings[messageKeys.ALARM_STATE];
    clay.setSettings('WEATHER_PROVIDER_ID', providerId);
    clay.setSettings('WEATHER_DISPLAY_MODE', displayMode);
    clay.setSettings('DND_START_TIME', timeMinutesToClay(dndStartMinute));
    clay.setSettings('DND_END_TIME', timeMinutesToClay(dndEndMinute));
    clay.setSettings('ALARM_STATE', JSON.stringify(alarmState));

    Pebble.sendAppMessage(
        settings,
        function() {
            console.log('Sent config data to Pebble');
        },
        function(error) {
            console.log('Failed to send config data');
            console.log(JSON.stringify(error));
        }
    );
});

Pebble.addEventListener('ready', function() {
    console.log('//GRID PebbleKit JS ready');
});

Pebble.addEventListener('appmessage', function(event) {
    if (!event || !event.payload || !hasMessageKey(event.payload, 'WEATHER_REQUEST')) {
        return;
    }

    var providerId = event.payload['WEATHER_REQUEST'];

    console.log('Weather update requested by watch, provider ' + providerId);
    getWeather(providerId);
});
