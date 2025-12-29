# This file is used to proceed esp32 requests
# It gets the weather and color data and sends them to the controller

from flask import Flask, request, jsonify
from dotenv import load_dotenv
from os import getenv, path
import json
from datetime import datetime

application = Flask(__name__)

load_dotenv()

ACCESS_KEY = getenv("ACCESS_KEY")
WEATHER_PATH = "weather.json"
LOG_PATH = "getter.log.json"

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

@application.route("/", methods=["GET", "POST"])
def main():
    if request.form.get("a") != ACCESS_KEY or not request.form.get("a"):
        return jsonify({"error": "Access denied: invalid access key"}), 400
    
    try:
        with open(WEATHER_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
            return jsonify({
                "weather": data["weather"],
                "r": data["r"],
                "g": data["g"],
                "b": data["b"],
                "last_update": data["last_update"],
                "timestamp": data["timestamp"]
            }), 200
    except Exception as e:
        logger(str(e))
        return jsonify({"error": str(e)}), 500            
    
if __name__ == "__main__":
    application.run(host="0.0.0.0")