var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var messageKeys = require('message_keys');
var weatherProviders = require('./providers');
var weatherAdapter = require('./adapters');

function customClay() {
    var clayConfig = this;

    function updateWeatherDetails() {
        var items = clayConfig.getItemsByGroup('weather-details');
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

    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
        var weatherToggle = clayConfig.getItemByMessageKey('WEATHER_ENABLE');

        if (!weatherToggle) {
            return;
        }

        updateWeatherDetails.call(weatherToggle);
        weatherToggle.on('change', updateWeatherDetails);
    });
}

var clay = new Clay(clayConfig, customClay, { autoHandleEvents: false });

var WEATHER_REQUEST_TIMEOUT_MS = 15000;
var LOCATION_CACHE_MAX_AGE_MS = 6 * 60 * 60 * 1000;
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

    settings[messageKeys.WEATHER_PROVIDER_ID] = providerId;
    clay.setSettings('WEATHER_PROVIDER_ID', providerId);

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
