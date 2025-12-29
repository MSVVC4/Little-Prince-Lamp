/*
    This is the oldest version of program
    The leds color calculates at esp32
    Without animations
    This version used a different server code to get weather and its not included in this project
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include <WiFi.h>

#define LED_PIN 5
#define NUM_LEDS 6
CRGB leds[NUM_LEDS];

const char* ssid PROGMEM = "wifi name";
const char* password PROGMEM = "pass";
const char baseURL[] PROGMEM = "https://some weather api";
const char params[] PROGMEM = "url params";

bool isRetryFetch = false;
unsigned long lastRetryFetchTime = 0;

enum Weather {
    CLEAR,
    SUNNY,
    CLOUD,
    RAIN,
    SNOW,
    THUNDER,
    FOG,
    UNKNOWN
};

Weather currentWeather = UNKNOWN;
unsigned long lastWeatherUpdate = 0;

CRGB currentColor(50, 0, 50);
CRGB targetColor(0, 0, 0);

inline bool includes(const std::string& str, const std::string& substr) {
    return str.find(substr) != std::string::npos;
}

std::string toLower(std::string s) {
    for (size_t i = 0; i < s.length(); ++i)
        s[i] = tolower(s[i]);
    return s;
}

String getTimeOfDay(short hour) {
    if (hour >= 6 && hour < 12)
        return "morning";
    else if (hour >= 12 && hour < 18)
        return "day";
    else if (hour >= 18 && hour < 24)
        return "evening";
    else
        return "night";
}

void colorEngine(std::string condition, float temperature, String timeOfDay, short& r, short& g, short& b) {
    std::string w = toLower(condition);
    Weather weather;

    if (includes(w, "clear")) {
        r = 255;
        g = 180;
        b = 20;
        weather = CLEAR;
    } else if (includes(w, "sunny")) {
        r = 255;
        g = 255;
        b = 80;
        weather = SUNNY;
    } else if (includes(w, "cloud") || includes(w, "overcast")) {
        r = 130;
        g = 130;
        b = 130;
        weather = CLOUD;
    } else if (includes(w, "rain") || includes(w, "drizzle")) {
        r = 60;
        g = 140;
        b = 255;
        weather = RAIN;
    } else if (includes(w, "snow") || includes(w, "sleet") || includes(w, "ice")) {
        r = 180;
        g = 210;
        b = 255;
        weather = SNOW;
    } else if (includes(w, "thunder") || includes(w, "storm")) {
        r = 20;
        g = 10;
        b = 50;
        weather = THUNDER;
    } else if (includes(w, "fog") || includes(w, "mist") || includes(w, "haze")) {
        r = 150;
        g = 150;
        b = 150;
        weather = FOG;
    } else {
        r = 255;
        g = 0;
        b = 130;
        weather = UNKNOWN;
    }

    if (temperature > 20) {
        r = min(255, r + 20);
        g = min(255, g + 10);
        b = max(0, b - 10);
    } else if (temperature < 10) {
        r = max(0, r - 20);
        g = max(0, g - 10);
        b = min(255, b + 30);
    } else if (temperature < 0) {
        r = max(0, r - 40);
        g = max(0, g - 20);
        b = min(255, b + 50);
    }

    float brightnessFactor = 1.0f;

    if (timeOfDay == "morning") {
        brightnessFactor = 0.4f;
    } else if (timeOfDay == "day") {
        brightnessFactor = 0.6f;
    } else if (timeOfDay == "evening") {
        brightnessFactor = 0.2f;
    } else if (timeOfDay == "night") {
        brightnessFactor = 0.08f;
    }

    r = static_cast<short>(r * brightnessFactor);
    g = static_cast<short>(g * brightnessFactor);
    b = static_cast<short>(b * brightnessFactor);
}

void smoothColorTransition(int durationMs) {
    Serial.println("color transition");
    const int steps = 100;
    const int delayMs = durationMs / steps;

    for (int i = 0; i <= steps; i++) {
        uint8_t r = currentColor.r + ((targetColor.r - currentColor.r) * i) / steps;
        uint8_t g = currentColor.g + ((targetColor.g - currentColor.g) * i) / steps;
        uint8_t b = currentColor.b + ((targetColor.b - currentColor.b) * i) / steps;

        for (int j = 0; j < NUM_LEDS; j++) {
            leds[j] = CRGB(r, g, b);
        }

        FastLED.show();
        delay(delayMs);
    }

    currentColor = targetColor;
}

void fetchWeather() {
    HTTPClient http;
    http.begin(baseURL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int httpCode = http.POST(params);
    String response;

    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
        response = http.getString();
        isRetryFetch = false;
    } else {
        Serial.printf("Connection error: %s\n", http.errorToString(httpCode).c_str());
        targetColor = CRGB(200, 0, 0);
        http.end();
        isRetryFetch = true;
        return;
    }
    http.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
        Serial.print("JSON parsing error: ");
        Serial.println(error.c_str());
        return;
    }
    float temperature = doc["current"]["temp_c"];
    String condition = doc["current"]["condition"]["text"].as<String>();
    long localtime_epoch = doc["location"]["localtime_epoch"];
    time_t server_localtime = (time_t)localtime_epoch;
    struct tm* local_time = gmtime(&server_localtime);

    short hour = local_time ? local_time->tm_hour : 12;
    String timeOfDay = getTimeOfDay(hour);

    short r, g, b;
    colorEngine(condition.c_str(), temperature, timeOfDay, r, g, b);

    Serial.printf("Temperature: %.1f°C, Weather: %s, RGB: (%d, %d, %d)\n", temperature, condition.c_str(), r, g, b);

    targetColor = CRGB(r, g, b);

    std::string w = toLower(condition.c_str());
    if (includes(w, "clear"))
        currentWeather = CLEAR;
    else if (includes(w, "sunny"))
        currentWeather = SUNNY;
    else if (includes(w, "cloud") || includes(w, "overcast"))
        currentWeather = CLOUD;
    else if (includes(w, "rain") || includes(w, "drizzle"))
        currentWeather = RAIN;
    else if (includes(w, "snow") || includes(w, "sleet") || includes(w, "ice"))
        currentWeather = SNOW;
    else if (includes(w, "thunder") || includes(w, "storm"))
        currentWeather = THUNDER;
    else if (includes(w, "fog") || includes(w, "mist") || includes(w, "haze"))
        currentWeather = FOG;
    else
        currentWeather = UNKNOWN;
}

void connectWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed connecting to WiFi");
        return;
    }

    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(115200);
    randomSeed(analogRead(0));

    connectWiFi();

    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    fill_solid(leds, NUM_LEDS, currentColor);
    FastLED.show();

    fetchWeather();
    lastWeatherUpdate = millis();
    smoothColorTransition(3000);
}

void loop() {
    unsigned long now = millis();

    if (now - lastWeatherUpdate >= 600000 && !isRetryFetch && WiFi.status() == WL_CONNECTED) {
        lastWeatherUpdate = now;
        fetchWeather();
        smoothColorTransition(3000);
    }

    if (WiFi.status() != WL_CONNECTED) {
        targetColor = CRGB(255, 0, 0);
        smoothColorTransition(1000);
        connectWiFi();
    }

    if (isRetryFetch && now - lastRetryFetchTime >= 300000) {
        lastRetryFetchTime = now;
        fetchWeather();
    }
}