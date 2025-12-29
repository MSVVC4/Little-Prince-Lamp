# This file is used to calculate r, g, b for esp32 based on the weather, time of day and temperature
# At my server it starts automatically via CRON scheduler and writes calculated data to file
# All you need to set up is weatherapi api key
# Be careful with the directory where the color data is saved, it must be same as getter.py's directory
# LAT and LON needs setup, for example: LAT = "12.3456"

from os import getenv, path
from dotenv import load_dotenv
import requests
import json
from datetime import datetime
import time
from math import ceil

load_dotenv()

WEATHER_API_KEY = getenv("WEATHER_API_KEY")
BASE_URL = "https://api.weatherapi.com/v1/current.json"
CONDITIONS_PATH = "weather.json"
LOG_PATH = "weather.log.json"

LAT = "Your latitude"
LON = "Your longitude"
weather: str
r, g, b = 0, 0, 0

def getWeather():
    url = f"{BASE_URL}?key={WEATHER_API_KEY}&q={LAT},{LON}&aqi=no"
    response = requests.get(url)
    response.raise_for_status()
    data = response.json()

    return data

def colorEngine(weatherData):
    condtion = str(weatherData["current"]["condition"]["text"]).lower()
    temperature: float = weatherData["current"]["temp_c"]
    hour = datetime.now().hour
    brighnessFactor = 1.0
    global weather, r, g, b
    
    if "sunny" in condtion:
        r = 255
        g = 255
        b = 80
        weather = "sunny"
    elif "clear" in condtion:
        r = 255
        g = 180
        b = 20
        weather = "clear"
    elif "cloud" in condtion or "overcast" in condtion:
        r = 130
        g = 130
        b = 130
        weather = "cloud"
    elif "rain" in condtion or "drizzle" in condtion:
        r = 60
        g = 140
        b = 255
        weather = "rain"
    elif "snow" in condtion or "sleet" in condtion or "ice" in condtion:
        r = 180
        g = 210
        b = 255
        weather = "snow"
    elif "thunder" in condtion or "storm" in condtion:
        r = 20
        g = 10
        b = 50
        weather = "thunder"
    elif "fog" in condtion or "mist" in condtion or "haze" in condtion:
        r = 150
        g = 150
        b = 150
        weather = "fog"
    else:
        r = 255
        g = 0
        b = 130
        weather = "unknown"
        
    if temperature > 20:
        r = min(255, r + 20)
        g = min(255, g + 10)
        b = max(0, b - 10)
    elif temperature < 10:
        r = max(0, r - 20)
        g = max(0, g - 10)
        b = min(255, b + 30)
    elif temperature < 0:
        r = max(0, r - 40)
        g = max(0, g - 20)
        b = min(255, b + 50)
        
    if hour >= 7 and hour <= 10:
        brighnessFactor = 0.4
    elif hour > 10 and hour <= 16:
        brighnessFactor = 0.7
    elif hour > 16 and hour <= 22:
        brighnessFactor = 0.35
    else:
        brighnessFactor = 0.2
        
    r = ceil(r * brighnessFactor)
    g = ceil(g * brighnessFactor)
    b = ceil(b * brighnessFactor)
    
def logger(msg: str):
    time = datetime.now().strftime("%d-%m-%Y %H:%M:%S.%f")[:-3]
    data = {"errors": []}
    
    if path.exists(LOG_PATH):
        with open(LOG_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
    
    errors = data["errors"]
    if len(errors) > 50:
        errors.pop(0)
    
    err = {time: msg}
    errors.append(err)
    
    with open(LOG_PATH, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    
def main():
    try:
        weatherData = getWeather()
        colorEngine(weatherData)
        lastUpdate = datetime.now().strftime("%d-%m-%Y %H:%M:%S")
        lastUpdateTimestamp = int(time.time())
        
        with open(CONDITIONS_PATH, "w", encoding="utf-8") as f:
            weatherFile = {
                "weather": weather,
                "r": r,
                "g": g,
                "b": b,
                "last_update": lastUpdate,
                "timestamp": lastUpdateTimestamp
            }
            json.dump(weatherFile, f, ensure_ascii=False, indent=2)        
    except Exception as e:
        logger(str(e))
        
if __name__ == "__main__":
    main()