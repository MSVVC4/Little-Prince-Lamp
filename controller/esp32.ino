/*
    A stable version to get weather and update leds with animations
    Some parameters needs setup
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

unsigned long lastWeatherUpdate = 0;
bool isRetryFetch = false;
short retriesCount = 0;
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
Weather targetWeather = UNKNOWN;
Weather lastFetchedWeather = UNKNOWN;
short sameWeatherCount = 0;
bool firstStart = true;

CRGB currentColor(50, 0, 50);
CRGB targetColor(0, 0, 0);

TaskHandle_t breathAnimationHandle = NULL;
TaskHandle_t streaksAnimationHandle = NULL;
bool animationAllowed = false;
bool animationRunning = false;

void colorTransition(int durationMs) {
    Serial.println("color transition");
    const short steps = 100;
    const short delayMs = durationMs / steps;

    for (int i = 0; i <= steps; i++) {
        uint8_t r = currentColor.r + ((targetColor.r - currentColor.r) * i) / steps;
        uint8_t g = currentColor.g + ((targetColor.g - currentColor.g) * i) / steps;
        uint8_t b = currentColor.b + ((targetColor.b - currentColor.b) * i) / steps;

        for (int j = 0; j < NUM_LEDS; j++) {
            leds[j] = CRGB(r, g, b);
        }

        FastLED.show();
        vTaskDelay(delayMs / portTICK_PERIOD_MS);
    }

    currentColor = targetColor;
    currentWeather = targetWeather;
}

void restoreToCurrentColor(short transitionSteps = 60, short delayMs = 20) {
    if (transitionSteps <= 0) transitionSteps = 1;
    for (int step = 0; step <= transitionSteps; step++) {
        for (int j = 0; j < NUM_LEDS; j++) {
            int r = leds[j].r + ((int)currentColor.r - leds[j].r) * step / transitionSteps;
            int g = leds[j].g + ((int)currentColor.g - leds[j].g) * step / transitionSteps;
            int b = leds[j].b + ((int)currentColor.b - leds[j].b) * step / transitionSteps;
            leds[j] = CRGB(r, g, b);
        }
        FastLED.show();
        vTaskDelay(delayMs / portTICK_PERIOD_MS);
    }

    for (int j = 0; j < NUM_LEDS; j++) leds[j] = currentColor;
    FastLED.show();
}

void breathAnimation(void* param) {
    Serial.println("breathAni");
    const CRGB color = currentColor;
    CRGB maxColor = CRGB(
        min(255, static_cast<int>(color.r * 1.2f)),
        min(255, static_cast<int>(color.g * 1.2f)),
        min(255, static_cast<int>(color.b * 1.2f)));
    CRGB minColor = CRGB(
        max(0, static_cast<int>(color.r * 0.1f)),
        max(0, static_cast<int>(color.g * 0.1f)),
        max(0, static_cast<int>(color.b * 0.1f)));

    const int steps = 90;
    const int stepsMid = steps * 2;
    animationRunning = true;

    while (animationAllowed) {
        for (int i = 0; i <= steps; i++) {
            float factor = (float)i / steps;
            short r = static_cast<short>(color.r + (maxColor.r - color.r) * factor);
            short g = static_cast<short>(color.g + (maxColor.g - color.g) * factor);
            short b = static_cast<short>(color.b + (maxColor.b - color.b) * factor);
            r = constrain(r, 0, 255);
            g = constrain(g, 0, 255);
            b = constrain(b, 0, 255);
            for (int j = 1; j < NUM_LEDS; j += 2) {
                leds[j] = CRGB(r, g, b);
            }
            FastLED.show();
            vTaskDelay(30 / portTICK_PERIOD_MS);
        }
        for (int i = 0; i <= stepsMid; i++) {
            float factor = (float)i / stepsMid;
            short r = static_cast<short>(maxColor.r + (minColor.r - maxColor.r) * factor);
            short g = static_cast<short>(maxColor.g + (minColor.g - maxColor.g) * factor);
            short b = static_cast<short>(maxColor.b + (minColor.b - maxColor.b) * factor);
            r = constrain(r, 0, 255);
            g = constrain(g, 0, 255);
            b = constrain(b, 0, 255);
            for (int j = 1; j < NUM_LEDS; j += 2) {
                leds[j] = CRGB(r, g, b);
            }
            FastLED.show();
            vTaskDelay(30 / portTICK_PERIOD_MS);
        }
        for (int i = 0; i <= steps; i++) {
            float factor = (float)i / steps;
            short r = static_cast<short>(minColor.r + (color.r - minColor.r) * factor);
            short g = static_cast<short>(minColor.g + (color.g - minColor.g) * factor);
            short b = static_cast<short>(minColor.b + (color.b - minColor.b) * factor);
            r = constrain(r, 0, 255);
            g = constrain(g, 0, 255);
            b = constrain(b, 0, 255);
            for (int j = 1; j < NUM_LEDS; j += 2) {
                leds[j] = CRGB(r, g, b);
            }
            FastLED.show();
            vTaskDelay(30 / portTICK_PERIOD_MS);
        }
    }

    if (!animationAllowed) {
        restoreToCurrentColor(120, 20);
    }

    animationRunning = false;
    Serial.println("animation end");
    vTaskDelete(NULL);
}

void streaksAnimation(void* param) {
    Serial.println("streaksAni");
    CRGB color = currentColor;
    short steps = 120;
    short maxBrightness;
    short probability;

    switch (currentWeather) {
        case RAIN:
            maxBrightness = 210;
            probability = 6;
            break;
        case SNOW:
            maxBrightness = 220;
            probability = 8;
            break;
        case THUNDER:
            maxBrightness = 230;
            probability = 3;
            break;
        default:
            maxBrightness = 150;
            probability = 5;
            break;
    }

    animationRunning = true;

    while (animationAllowed) {
        for (int step = 0; step < steps; step++) {
            for (int i = 1; i < NUM_LEDS; i += 2) {
                if (random(0, 100) <= probability) {
                    if (currentWeather == RAIN) {
                        for (int br = color.b; br < maxBrightness; br += 30) {
                            leds[i] = CRGB(color.r, color.g, br);
                            FastLED.show();
                            vTaskDelay(30 / portTICK_PERIOD_MS);
                        }
                        for (int br = maxBrightness; br >= color.b; br -= 10) {
                            leds[i] = CRGB(color.r, color.g, br);
                            FastLED.show();
                            vTaskDelay(30 / portTICK_PERIOD_MS);
                        }
                    } else if (currentWeather == SNOW || currentWeather == THUNDER) {
                        CRGB originalColor = CRGB(color.r, color.g, color.b);
                        CRGB currentLED = CRGB(color.r, color.g, color.b);
                        const float multiplierUp = currentWeather == SNOW ? 1.15f : 1.4f;
                        const float multiplierDown = currentWeather == SNOW ? 0.97f : 0.85f;

                        while (currentLED.r < maxBrightness || currentLED.g < maxBrightness || currentLED.b < maxBrightness) {
                            if (currentLED.r < maxBrightness) {
                                currentLED.r = min((uint16_t)maxBrightness, (uint16_t)(currentLED.r * multiplierUp));
                            }
                            if (currentLED.g < maxBrightness) {
                                currentLED.g = min((uint16_t)maxBrightness, (uint16_t)(currentLED.g * multiplierUp));
                            }
                            if (currentLED.b < maxBrightness) {
                                currentLED.b = min((uint16_t)maxBrightness, (uint16_t)(currentLED.b * multiplierUp));
                            }
                            leds[i] = currentLED;
                            FastLED.show();
                            vTaskDelay(30 / portTICK_PERIOD_MS);
                        }

                        while (currentLED.r > originalColor.r || currentLED.g > originalColor.g || currentLED.b > originalColor.b) {
                            if (currentLED.r > originalColor.r) {
                                currentLED.r = max((uint16_t)originalColor.r, (uint16_t)(currentLED.r * multiplierDown));
                            }
                            if (currentLED.g > originalColor.g) {
                                currentLED.g = max((uint16_t)originalColor.g, (uint16_t)(currentLED.g * multiplierDown));
                            }
                            if (currentLED.b > originalColor.b) {
                                currentLED.b = max((uint16_t)originalColor.b, (uint16_t)(currentLED.b * multiplierDown));
                            }
                            leds[i] = currentLED;
                            FastLED.show();
                            vTaskDelay(30 / portTICK_PERIOD_MS);
                        }
                    }
                } else {
                    for (int i = 255; i >= maxBrightness; i -= 10) {
                        vTaskDelay(30 / portTICK_PERIOD_MS);
                    }
                }
            }
        }
    }

    if (!animationAllowed) {
        restoreToCurrentColor(120, 20);
    }

    animationRunning = false;

    Serial.println("animation end");
    vTaskDelete(NULL);
}

void controller() {
    Serial.println("controller");
    if (firstStart) {
        Serial.println("firstStart");
        colorTransition(3000);
        firstStart = false;
        sameWeatherCount = 0;
        return;
    }

    if (targetWeather == currentWeather && currentColor.r == targetColor.r && currentColor.g == targetColor.g && currentColor.b == targetColor.b) {
        Serial.println("targetWeather == currentWeather");
        if (animationRunning) return;

        sameWeatherCount++;
        Serial.printf("Same weather count: %d\n", sameWeatherCount);

        if (sameWeatherCount >= 3) {
            animationAllowed = true;
            if (currentWeather == RAIN || currentWeather == SNOW || currentWeather == THUNDER) {
                xTaskCreate(
                    streaksAnimation,
                    "streaksAni",
                    4096,
                    NULL,
                    1,
                    &streaksAnimationHandle);
                Serial.printf("Stack remaining: %d\n", uxTaskGetStackHighWaterMark(streaksAnimationHandle));
            } else {
                xTaskCreate(
                    breathAnimation,
                    "breathAni",
                    4096,
                    NULL,
                    1,
                    &breathAnimationHandle);
                Serial.printf("Stack remaining: %d\n", uxTaskGetStackHighWaterMark(breathAnimationHandle));
            }
        }
    } else if (targetWeather == currentWeather && (currentColor.r != targetColor.r || currentColor.g != targetColor.g || currentColor.b != targetColor.b)) {
        Serial.println("controller color change");
        animationAllowed = false;
        while (animationRunning) {
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        colorTransition(3000);

        animationAllowed = true;
        if (currentWeather == RAIN || currentWeather == SNOW || currentWeather == THUNDER) {
            xTaskCreate(
                streaksAnimation,
                "streaksAni",
                4096,
                NULL,
                1,
                &streaksAnimationHandle);
            Serial.printf("Stack remaining: %d\n", uxTaskGetStackHighWaterMark(streaksAnimationHandle));
        } else {
            xTaskCreate(
                breathAnimation,
                "breathAni",
                4096,
                NULL,
                1,
                &breathAnimationHandle);
            Serial.printf("Stack remaining: %d\n", uxTaskGetStackHighWaterMark(breathAnimationHandle));
        }
    } else {
        Serial.println("controller change");
        sameWeatherCount = 0;
        animationAllowed = false;
        while (animationRunning) {
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }

        colorTransition(3000);
    }
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
        retriesCount = 0;
    } else {
        Serial.printf("Connection error: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        retriesCount++;
        if (retriesCount >= 3) {
            animationAllowed = false;
            while (animationRunning) {
                vTaskDelay(20 / portTICK_PERIOD_MS);
            }

            targetColor = CRGB(120, 0, 0);
            colorTransition(1000);
        }
        isRetryFetch = true;
        return;
    }
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        Serial.print("JSON parsing error: ");
        Serial.println(err.c_str());
        return;
    }
    uint8_t r = doc["r"].as<uint8_t>();
    uint8_t g = doc["g"].as<uint8_t>();
    uint8_t b = doc["b"].as<uint8_t>();
    targetColor = CRGB(r, g, b);
    String weather = doc["weather"].as<String>();
    Serial.printf("r: %d, g: %d, b: %d\n", r, g, b);
    Serial.printf("weather: %s\n", weather);

    if (weather == "clear")
        targetWeather = CLEAR;
    else if (weather == "sunny")
        targetWeather = SUNNY;
    else if (weather == "cloud")
        targetWeather = CLOUD;
    else if (weather == "rain")
        targetWeather = RAIN;
    else if (weather == "snow")
        targetWeather = SNOW;
    else if (weather == "thunder")
        targetWeather = THUNDER;
    else if (weather == "fog")
        targetWeather = FOG;
    else
        targetWeather = UNKNOWN;

    lastFetchedWeather = targetWeather;
}

void connectWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        Serial.print(".");
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed connecting to WiFi");
        retriesCount++;
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
    controller();
    lastWeatherUpdate = millis();
}

void loop() {
    unsigned long now = millis();

    if (now - lastWeatherUpdate >= 600000 && !isRetryFetch && WiFi.status() == WL_CONNECTED) {
        fetchWeather();
        lastWeatherUpdate = now;
        controller();
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (retriesCount >= 3) {
            animationAllowed = false;
            while (animationRunning) {
                vTaskDelay(20 / portTICK_PERIOD_MS);
            }

            targetColor = CRGB(160, 0, 0);
            colorTransition(1000);
        }
        connectWiFi();
    }

    if (isRetryFetch && now - lastRetryFetchTime > 120000) {
        lastRetryFetchTime = now;
        fetchWeather();
    }
}