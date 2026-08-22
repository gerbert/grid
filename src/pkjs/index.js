var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var messageKeys = require('message_keys');
var weatherProviders = require('./providers');
var weatherAdapter = require('./adapters');

function customClay() {
    var clayConfig = this;
    var dndTimeRow;

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

    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
        layoutDndTimeRange();
        bindGroupToggle('WEATHER_ENABLE', 'weather-details');
        bindDndToggle();
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

    settings[messageKeys.WEATHER_PROVIDER_ID] = providerId;
    settings[messageKeys.WEATHER_DISPLAY_MODE] = displayMode;
    settings[messageKeys.DND_START_TIME] = dndStartMinute;
    settings[messageKeys.DND_END_TIME] = dndEndMinute;
    clay.setSettings('WEATHER_PROVIDER_ID', providerId);
    clay.setSettings('WEATHER_DISPLAY_MODE', displayMode);
    clay.setSettings('DND_START_TIME', timeMinutesToClay(dndStartMinute));
    clay.setSettings('DND_END_TIME', timeMinutesToClay(dndEndMinute));

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
