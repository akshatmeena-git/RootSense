# RootSense — Irrigation Fault Diagnosis System

RootSense is an irrigation fault diagnosis project for tomato plants. It combines an ESP32-based Wokwi simulation, a Python machine learning model, and a FastAPI backend to explore how soil moisture, water pressure, and water flow can be used to identify irrigation conditions.

## Project Overview

The project has three main parts:

1. **Wokwi / ESP32 simulation** — simulates surface moisture, root-zone moisture, pressure, and water-flow pulses during an irrigation cycle.
2. **Machine learning model** — trains a Decision Tree Classifier using the labelled `tomato.csv` dataset and saves the trained model as `model.pkl`.
3. **FastAPI backend** — provides a `/sensor` endpoint that accepts sensor readings, calculates moisture changes, produces a diagnosis, logs the result to a CSV file, and returns a JSON response.

The Python backend's decision tree and the Wokwi firmware's rule-based classifier are separate implementations in the current code.

## Diagnosis Classes

The machine learning dataset uses these condition labels:

- `SUPPLY_FAILURE`
- `LEAK`
- `EMITTER_CLOG`
- `SURFACE_ONLY`
- `NORMAL`
- `OVERWATERING`

The Wokwi firmware prints its own rule-based diagnosis labels, such as `POSSIBLE CLOG`, `POSSIBLE LEAK`, `INSUFFICIENT ROOT WETTING`, and `NORMAL`.

## Repository Files

- `main.py` — FastAPI backend.
- `train_model.py` — trains the Decision Tree Classifier and plots the tree.
- `model.pkl` — saved trained model.
- `tomato.csv` — labelled dataset used for model training.
- `sketch.ino` — ESP32 firmware for the Wokwi simulation.
- `images/decision_tree.png` — decision-tree visualization.
- `requirements.txt` — Python dependencies.

Add the Wokwi `diagram.json` and any other circuit configuration files from the Wokwi project if they are available.

## Requirements

- Python 3.10 or a compatible Python version
- The packages listed in `requirements.txt`
- Wokwi (for running the ESP32 simulation)

## Setup and Run

Run these commands from the repository root folder.

### 1. Install Python dependencies

```bash
pip install -r requirements.txt
```

### 2. Train the model

```bash
python train_model.py
```

This reads `tomato.csv`, trains the model, saves `model.pkl`, and displays the decision-tree visualization.

### 3. Start the FastAPI backend

```bash
uvicorn main:app --reload
```

Open the interactive API documentation at:

`http://127.0.0.1:8000/docs`

The backend defines a POST endpoint at `/sensor`.

## Wokwi Simulation

The simulation uses potentiometers to represent surface moisture, root-zone moisture, and pressure. A pulse input represents the flow sensor. The firmware calculates delivered water volume and moisture response, then applies rule-based conditions to produce a diagnostic report in the Serial Monitor. It also sends sensor values to the Wokwi Plotter.

Change `ACTIVE_SCENARIO` in `sketch.ino` to test the scenarios defined in the firmware.

## Current Status and Limitations

- The Wokwi simulation and FastAPI backend are present as separate parts; an HTTP connection from the simulation to the backend must be implemented for end-to-end data transfer.
- The training script uses eight model features, while the current backend prediction list defines eleven. These must be made consistent before the API prediction path can work reliably.
- The backend currently sets `duration`, `root_delay`, and `water_volume` to fixed values in `calculate_features()`.
- The backend includes a machine-specific absolute path for its generated CSV output; this path may need to be changed on another computer.
- Model performance should be evaluated with held-out test data before making claims about real-world accuracy. The current training script does not calculate test accuracy because its evaluation code is commented out.
- The diagnosis labels and thresholds used by the Python model and Wokwi rule-based classifier are not identical.

## Team Members and Contributions

| Name | Roll No. | Contribution |
|---|---|---|
| Akshat Meena (Team Leader) | 125103023 | Documenting the report, guiding team members, developing the project idea, and working on the Wokwi simulation and circuit. |
| Aniket Yadav | 125103022 | Machine learning model, dataset generation, and backend development. |
| Ayush Raj | 125103006 | Machine learning model and backend development. |
| Vedansh Singh Thakur | 125103021 | Future aspects and conclusion of the report. |
| Gagandeep | 125103019 | Drawbacks, related work, existing projects, and dataset generation. |
| Aditya Rana | 125103024 | Helped write the abstract and report sections covering implementation details, parameters used, and results tables; contributed to ideation and building the simulation. |


RootSense is a prototype for learning and experimentation. Its simulated data and predictions should be validated against independent measurements and real irrigation hardware before the system is used for real-world irrigation decisions.
