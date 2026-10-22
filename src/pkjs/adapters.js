var weatherProviders = require('./providers');
var weatherConditions = require('./conditions');

var WEATHER_SLOT_COUNT = 12;
var WEATHER_FORECAST_HEADER_SIZE = 5;
var WEATHER_FORECAST_SLOT_SIZE = 2;
var SECONDS_PER_HOUR = 60 * 60;

function finiteNumber(value) {
    return typeof value === 'number' && isFinite(value);
}

function integerNumber(value) {
    return finiteNumber(value) && Math.floor(value) === value;
}

function encodeTemperature(value) {
    if (!finiteNumber(value)) {
        return null;
    }

    var rounded = Math.round(value);

    if (rounded < -128 || rounded > 127) {
        return null;
    }

    return rounded < 0 ? rounded + 256 : rounded;
}

function openMeteoCondition(code) {
    if (code === 0) {
        return weatherConditions.CLEAR;
    }
    if (code === 1) {
        return weatherConditions.MOSTLY_CLEAR;
    }
    if (code === 2) {
        return weatherConditions.PARTLY_CLOUDY;
    }
    if (code === 3) {
        return weatherConditions.OVERCAST;
    }
    if (code === 45 || code === 48) {
        return weatherConditions.FOG;
    }
    if (code >= 51 && code <= 55) {
        return weatherConditions.DRIZZLE;
    }
    if (code === 56 || code === 57) {
        return weatherConditions.FREEZING_DRIZZLE;
    }
    if (code >= 61 && code <= 65) {
        return weatherConditions.RAIN;
    }
    if (code === 66 || code === 67) {
        return weatherConditions.FREEZING_RAIN;
    }
    if (code >= 71 && code <= 75) {
        return weatherConditions.SNOW;
    }
    if (code === 77) {
        return weatherConditions.SNOW_GRAINS;
    }
    if (code >= 80 && code <= 82) {
        return weatherConditions.SHOWERS;
    }
    if (code === 85 || code === 86) {
        return weatherConditions.SNOW_SHOWERS;
    }
    if (code === 95) {
        return weatherConditions.THUNDERSTORM;
    }
    if (code === 96 || code === 99) {
        return weatherConditions.HAIL_STORM;
    }

    return weatherConditions.UNKNOWN;
}

function normalizeOpenMeteo(responseText) {
    var json;

    try {
        json = JSON.parse(responseText);
    } catch (error) {
        return null;
    }

    if (!json || !json.hourly) {
        return null;
    }

    var times = json.hourly.time;
    var temperatures = json.hourly.temperature_2m;
    var weatherCodes = json.hourly.weather_code;

    if (!Array.isArray(times) || !Array.isArray(temperatures) ||
        !Array.isArray(weatherCodes) ||
        times.length === 0 || times.length > WEATHER_SLOT_COUNT ||
        temperatures.length !== times.length || weatherCodes.length !== times.length) {
        return null;
    }

    var count = times.length;
    var firstTimestamp = times[0];
    var i;

    if (!integerNumber(firstTimestamp) || firstTimestamp < 0 || firstTimestamp > 0xffffffff ||
        firstTimestamp % SECONDS_PER_HOUR !== 0) {
        return null;
    }

    var slots = [];

    for (i = 0; i < count; i += 1) {
        var timestamp = times[i];
        var temperature = encodeTemperature(temperatures[i]);
        var weatherCode = weatherCodes[i];

        if (!integerNumber(timestamp) ||
            timestamp !== firstTimestamp + i * SECONDS_PER_HOUR ||
            temperature === null || !integerNumber(weatherCode)) {
            return null;
        }

        slots.push({
            temperature: temperature,
            condition: openMeteoCondition(weatherCode)
        });
    }

    return {
        count: count,
        startTimestamp: firstTimestamp,
        slots: slots
    };
}

function metNorwayCondition(symbolCode) {
    if (typeof symbolCode !== 'string') {
        return weatherConditions.UNKNOWN;
    }

    var symbol = symbolCode.replace(/_(day|night|polartwilight)$/, '');

    if (/andthunder$/.test(symbol)) {
        return weatherConditions.THUNDERSTORM;
    }

    switch (symbol) {
        case 'clearsky': return weatherConditions.CLEAR;
        case 'fair': return weatherConditions.MOSTLY_CLEAR;
        case 'partlycloudy': return weatherConditions.PARTLY_CLOUDY;
        case 'cloudy': return weatherConditions.OVERCAST;
        case 'fog': return weatherConditions.FOG;
        case 'lightrain':
        case 'rain':
        case 'heavyrain': return weatherConditions.RAIN;
        case 'lightrainshowers':
        case 'rainshowers':
        case 'heavyrainshowers': return weatherConditions.SHOWERS;
        case 'lightsnow':
        case 'snow':
        case 'heavysnow':
        case 'lightsleet':
        case 'sleet':
        case 'heavysleet': return weatherConditions.SNOW;
        case 'lightsnowshowers':
        case 'snowshowers':
        case 'heavysnowshowers':
        case 'lightsleetshowers':
        case 'sleetshowers':
        case 'heavysleetshowers': return weatherConditions.SNOW_SHOWERS;
    }

    return weatherConditions.UNKNOWN;
}

function normalizeMetNorway(responseText) {
    var json;

    try {
        json = JSON.parse(responseText);
    } catch (error) {
        return null;
    }

    var timeseries = json && json.properties && json.properties.timeseries;

    if (!Array.isArray(timeseries) || timeseries.length === 0) {
        return null;
    }

    var currentHour = Math.floor(Date.now() / (SECONDS_PER_HOUR * 1000)) * SECONDS_PER_HOUR;
    var firstTimestamp = null;
    var slots = [];
    var i;

    for (i = 0; i < timeseries.length && slots.length < WEATHER_SLOT_COUNT; i += 1) {
        var entry = timeseries[i];
        var timestamp = entry && Date.parse(entry.time) / 1000;

        if (!integerNumber(timestamp) || timestamp < 0 || timestamp > 0xffffffff ||
            timestamp % SECONDS_PER_HOUR !== 0) {
            return null;
        }

        if (timestamp < currentHour) {
            continue;
        }

        if (firstTimestamp === null) {
            firstTimestamp = timestamp;
        } else if (timestamp !== firstTimestamp + slots.length * SECONDS_PER_HOUR) {
            break;
        }

        var data = entry.data;
        var temperature = encodeTemperature(data && data.instant &&
            data.instant.details && data.instant.details.air_temperature);
        var summary = data && data.next_1_hours && data.next_1_hours.summary;

        if (temperature === null || !summary || typeof summary.symbol_code !== 'string') {
            return null;
        }

        slots.push({
            temperature: temperature,
            condition: metNorwayCondition(summary.symbol_code)
        });
    }

    if (slots.length === 0) {
        return null;
    }

    return {
        count: slots.length,
        startTimestamp: firstTimestamp,
        slots: slots
    };
}

function writeUint32(payload, offset, value) {
    payload[offset] = value & 0xff;
    payload[offset + 1] = (value >>> 8) & 0xff;
    payload[offset + 2] = (value >>> 16) & 0xff;
    payload[offset + 3] = (value >>> 24) & 0xff;
}

function encodeForecast(forecast) {
    if (!forecast || forecast.count === 0 || forecast.count > WEATHER_SLOT_COUNT ||
        forecast.slots.length !== forecast.count) {
        return null;
    }

    var payloadLength = WEATHER_FORECAST_HEADER_SIZE +
        forecast.count * WEATHER_FORECAST_SLOT_SIZE;
    var payload = [];
    var i;

    for (i = 0; i < payloadLength; i += 1) {
        payload.push(0);
    }

    payload[0] = forecast.count;
    writeUint32(payload, 1, forecast.startTimestamp);

    for (i = 0; i < forecast.count; i += 1) {
        var offset = WEATHER_FORECAST_HEADER_SIZE + i * WEATHER_FORECAST_SLOT_SIZE;
        var slot = forecast.slots[i];

        if (!weatherConditions.isValid(slot.condition)) {
            return null;
        }

        payload[offset] = slot.temperature;
        payload[offset + 1] = slot.condition;
    }

    return payload;
}

function encode(providerId, responseText) {
    var forecast = null;

    if (providerId === weatherProviders.openMeteoId) {
        forecast = normalizeOpenMeteo(responseText);
    } else if (providerId === weatherProviders.metNorwayId) {
        forecast = normalizeMetNorway(responseText);
    }

    return encodeForecast(forecast);
}

module.exports = {
    slotCount: WEATHER_SLOT_COUNT,
    encode: encode
};
