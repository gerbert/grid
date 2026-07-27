var UNKNOWN = 0;
var CLEAR = 1;
var MOSTLY_CLEAR = 2;
var PARTLY_CLOUDY = 3;
var OVERCAST = 4;
var FOG = 5;
var DRIZZLE = 6;
var FREEZING_DRIZZLE = 7;
var RAIN = 8;
var FREEZING_RAIN = 9;
var SNOW = 10;
var SNOW_GRAINS = 11;
var SHOWERS = 12;
var SNOW_SHOWERS = 13;
var THUNDERSTORM = 14;
var HAIL_STORM = 15;
var COUNT = 16;

function isValid(conditionId) {
    return typeof conditionId === 'number' && isFinite(conditionId) &&
        Math.floor(conditionId) === conditionId &&
        conditionId >= UNKNOWN && conditionId < COUNT;
}

module.exports = {
    UNKNOWN: UNKNOWN,
    CLEAR: CLEAR,
    MOSTLY_CLEAR: MOSTLY_CLEAR,
    PARTLY_CLOUDY: PARTLY_CLOUDY,
    OVERCAST: OVERCAST,
    FOG: FOG,
    DRIZZLE: DRIZZLE,
    FREEZING_DRIZZLE: FREEZING_DRIZZLE,
    RAIN: RAIN,
    FREEZING_RAIN: FREEZING_RAIN,
    SNOW: SNOW,
    SNOW_GRAINS: SNOW_GRAINS,
    SHOWERS: SHOWERS,
    SNOW_SHOWERS: SNOW_SHOWERS,
    THUNDERSTORM: THUNDERSTORM,
    HAIL_STORM: HAIL_STORM,
    COUNT: COUNT,
    isValid: isValid
};
