import joblib
from pathlib import Path
import pandas as pd
from fastapi import FastAPI
from pydantic import BaseModel

model = joblib.load("model.pkl")

print("Model loaded successfully")


df = pd.read_csv("tomato.csv") #

app = FastAPI()

#/////////////////////////////////////////////////////////////////

#pydantic model
class SensorData(BaseModel):
    initial_surface: float
    final_surface: float
    initial_root: float
    final_root: float
    pressure: float
    avg_flow: float

#///////////////////////////////////////////////////////////////////

INTERVAL_DURATION = 20.0
PULSES_PER_LITER = 600.0

#///////////////////////////////////////////////////////////////////

features = [
    "initial_surface",  #intial value of surface soil sensor
    "final_surface",    #final value of surface soil sensor
    "initial_root",     #intial value of root soil sensor
    "final_root",       #final value of root soil sensor
    "surface_change",   #change in initial and final value of the surface sensor in the given duration
    "root_change",      #change in initial and final value of the root sensor in the given duration
    "pressure",         #reading of pressure sensor
    "avg_flow",         #reading of the flow sensor
    "duration",         #duration watering required
    "root_delay",       #time taken by water to reach from surface to root
    "water_volume"
]

#///////////////////////////////////////////////////////////////////

#feature calculation
def calculate_features(data):
    surface_change = data["final_surface"] - data["initial_surface"]
    root_change = data["final_root"] - data["initial_root"]

    return {
        "initial_surface": data["initial_surface"],
        "final_surface": data["final_surface"],
        "initial_root": data["initial_root"],
        "final_root": data["final_root"],
        "surface_change": surface_change,
        "root_change": root_change,
        "pressure": data["pressure"],
        "avg_flow": data["avg_flow"],
        "duration": INTERVAL_DURATION,
        "root_delay": 0.0,
        "water_volume": 0.0
    }

#///////////////////////////////////////////////////////////////////

#api call
@app.post("/sensor")
def receive_data(data: SensorData):

    global df

    received_data = data.model_dump()

    #improving the data ccording to the requirement
    sensor_data = calculate_features(received_data)

    #calling the prrdiction function
    prediction = predict_condition(sensor_data)

    #adding new row to csv file
    row = {
        "initial_surface": sensor_data["initial_surface"],
        "final_surface": sensor_data["final_surface"],
        "initial_root": sensor_data["initial_root"],
        "final_root": sensor_data["final_root"],
        "surface_change": sensor_data["surface_change"],
        "root_change": sensor_data["root_change"],
        "pressure": sensor_data["pressure"],
        "avg_flow": sensor_data["avg_flow"],
        "duration": sensor_data["duration"],
        "root_delay": sensor_data["root_delay"],
        "water_volume": sensor_data["water_volume"],
        "prediction": prediction,
    }

    new_df = pd.DataFrame([row])

    df=pd.concat([df, new_df], ignore_index=True)

    df.to_csv(("/Users/aniketyadav/project(self)/preproceesing.py/tomato.csv"), index=False)

    # returning the result
    return {
        "message": "Sensor data processed successfully",
        "duration_seconds": INTERVAL_DURATION,
        "surface_change": sensor_data["surface_change"],
        "root_change": sensor_data["root_change"],
        "water_volume_litres": sensor_data["water_volume"],
        "average_flow_lpm": sensor_data["avg_flow"],
        "prediction": prediction,
    }

#///////////////////////////////////////////////////////////////////

#predict function
def predict_condition(sensor_data):

    flow = sensor_data["avg_flow"]

    if flow <= 0.01:
        return "Supply_Failure"

    input_data = pd.DataFrame(
