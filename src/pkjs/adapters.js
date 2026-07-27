var weatherProviders = require('./providers');

var WEATHER_SLOT_COUNT = 12;
var WEATHER_CONDITION_LENGTH = 16;
var WEATHER_FORECAST_HEADER_SIZE = 7;
var WEATHER_FORECAST_SLOT_SIZE = 1 + WEATHER_CONDITION_LENGTH;
var SECONDS_PER_MINUTE = 60;

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
        return 'CLEAR';
    }
    if (code === 1) {
        return 'MOSTLY CLEAR';
    }
    if (code === 2) {
        return 'PARTLY CLOUDY';
    }
    if (code === 3) {
        return 'OVERCAST';
    }
    if (code === 45 || code === 48) {
        return 'FOG';
    }
    if (code >= 51 && code <= 55) {
        return 'DRIZZLE';
    }
    if (code === 56 || code === 57) {
        return 'ICY DRIZZLE';
    }
    if (code >= 61 && code <= 65) {
        return 'RAIN';
    }
    if (code === 66 || code === 67) {
        return 'FREEZING RAIN';
    }
    if (code >= 71 && code <= 75) {
        return 'SNOW';
    }
    if (code === 77) {
        return 'SNOW GRAINS';
    }
    if (code >= 80 && code <= 82) {
        return 'SHOWERS';
    }
    if (code === 85 || code === 86) {
        return 'SNOW SHOWERS';
    }
    if (code === 95) {
        return 'THUNDERSTORM';
    }
    if (code === 96 || code === 99) {
        return 'HAIL STORM';
    }

    return 'WEATHER';
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
    var intervalMinutes = 60;
    var i;

    if (!integerNumber(firstTimestamp) || firstTimestamp < 0 || firstTimestamp > 0xffffffff) {
        return null;
    }

    if (count > 1) {
        var intervalSeconds = times[1] - times[0];

        if (!integerNumber(intervalSeconds) || intervalSeconds <= 0 ||
            intervalSeconds % SECONDS_PER_MINUTE !== 0) {
            return null;
        }

        intervalMinutes = intervalSeconds / SECONDS_PER_MINUTE;

        if (intervalMinutes > 0xffff) {
            return null;
        }
    }

    var slots = [];

    for (i = 0; i < count; i += 1) {
        var timestamp = times[i];
        var temperature = encodeTemperature(temperatures[i]);
        var weatherCode = weatherCodes[i];

        if (!integerNumber(timestamp) ||
            timestamp !== firstTimestamp + i * intervalMinutes * SECONDS_PER_MINUTE ||
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
        intervalMinutes: intervalMinutes,
        startTimestamp: firstTimestamp,
        slots: slots
    };
}

function writeUint16(payload, offset, value) {
    payload[offset] = value & 0xff;
    payload[offset + 1] = (value >>> 8) & 0xff;
}

function writeUint32(payload, offset, value) {
    payload[offset] = value & 0xff;
    payload[offset + 1] = (value >>> 8) & 0xff;
    payload[offset + 2] = (value >>> 16) & 0xff;
    payload[offset + 3] = (value >>> 24) & 0xff;
}

function writeCondition(payload, offset, condition) {
    var i;

    for (i = 0; i < WEATHER_CONDITION_LENGTH; i += 1) {
        payload[offset + i] = 0;
    }

    for (i = 0; i < condition.length && i < WEATHER_CONDITION_LENGTH - 1; i += 1) {
        payload[offset + i] = condition.charCodeAt(i) & 0x7f;
    }
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
    writeUint16(payload, 1, forecast.intervalMinutes);
    writeUint32(payload, 3, forecast.startTimestamp);

    for (i = 0; i < forecast.count; i += 1) {
        var offset = WEATHER_FORECAST_HEADER_SIZE + i * WEATHER_FORECAST_SLOT_SIZE;

        payload[offset] = forecast.slots[i].temperature;
        writeCondition(payload, offset + 1, forecast.slots[i].condition);
    }

    return payload;
}

function encode(providerId, responseText) {
    var forecast = null;

    if (providerId === weatherProviders.openMeteoId) {
        forecast = normalizeOpenMeteo(responseText);
    }

    return encodeForecast(forecast);
}

module.exports = {
    slotCount: WEATHER_SLOT_COUNT,
    encode: encode
};
