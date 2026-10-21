// ============================================================
// ROOTSENSE - PHASE 2
//
// FLOW + PRESSURE + SURFACE + ROOT
// RULE-BASED IRRIGATION FAULT CLASSIFIER
//
// GPIO 34 -> Surface moisture potentiometer
// GPIO 35 -> Root-zone moisture potentiometer
// GPIO 32 -> Pressure potentiometer
// GPIO 27 -> Flow pulse input / clock generator
//
// GPIO 17 -> UART1 TX -> Wokwi Plotter
// GPIO 16 -> UART1 RX
//
// USB Serial -> Final diagnostic report
//
// IMPORTANT:
// Pressure is READ FROM GPIO32.
// The pressure potentiometer value in diagram.json
// determines the simulated pressure.
// ============================================================

#include <string.h>

// ============================================================
// PIN DEFINITIONS
// ============================================================

#define SURFACE_PIN   34
#define ROOT_PIN      35
#define PRESSURE_PIN  32
#define FLOW_PIN      27

// ============================================================
// SCENARIO SELECTION
// ============================================================

enum Scenario {
  NORMAL,
  CLOG,
  LEAK,
  OVERWATER,
  POOR_PENETRATION
};

// ------------------------------------------------------------
// CHANGE THIS LINE TO TEST ANOTHER CASE
// ------------------------------------------------------------

const Scenario ACTIVE_SCENARIO = NORMAL;

// ============================================================
// UART
// ============================================================

HardwareSerial PlotSerial(1);

// ============================================================
// FLOW CALIBRATION
// ============================================================
//
// 600 pulses = 1 litre
//
// Clock = 10 pulses/second
//
// 10 / 600 = 0.01667 L/sec
//
// 0.01667 × 60 ≈ 1 L/min
// ============================================================

const float PULSES_PER_LITER = 600.0;

// ============================================================
// IRRIGATION SETTINGS
// ============================================================

const unsigned long PLANNED_SEC = 20;

const unsigned long ROOT_DELAY_SEC = 5;

// ============================================================
// NORMAL FLOW
// ============================================================

const float NOMINAL_FLOW_LPM = 1.0;

// ============================================================
// PLANNED WATER
// ============================================================
//
// 1 L/min × 20 sec / 60
// = 0.3333 L
// ============================================================

const float PLANNED_LITRES =
  NOMINAL_FLOW_LPM * PLANNED_SEC / 60.0;

// ============================================================
// SCENARIO PROFILE
// ============================================================
//
// NOTE:
// Pressure is NOT stored here anymore.
//
// Pressure comes from Pot3 / GPIO32.
//
// The profile controls:
// - moisture response
// - irrigation duration
// - expected diagnosis
// ============================================================

struct ScenarioProfile {

  const char* name;

  float surfaceGain;

  float rootGain;

  unsigned long durationSec;

  const char* expectedLabel;
};

// ============================================================
// SCENARIO DEFINITIONS
// ============================================================

const ScenarioProfile PROFILES[] = {

  // ----------------------------------------------------------
  // NORMAL
  // ----------------------------------------------------------

  {
    "NORMAL",
    60.0,
    30.0,
    20,
    "NORMAL"
  },

  // ----------------------------------------------------------
  // CLOG
  // ----------------------------------------------------------

  {
    "CLOG",
    15.0,
    7.5,
    20,
    "POSSIBLE CLOG"
  },

  // ----------------------------------------------------------
  // LEAK
  // ----------------------------------------------------------

  {
    "LEAK",
    12.0,
    3.0,
    20,
    "POSSIBLE LEAK"
  },

  // ----------------------------------------------------------
  // OVERWATER
  // ----------------------------------------------------------

  {
    "OVERWATER",
    60.0,
    30.0,
    40,
    "OVERWATERING"
  },

  // ----------------------------------------------------------
  // POOR PENETRATION
  // ----------------------------------------------------------

  {
    "POOR PENETRATION",
    60.0,
    3.0,
    20,
    "INSUFFICIENT ROOT WETTING"
  }
};

// ============================================================
// ACTIVE PROFILE
// ============================================================

const ScenarioProfile& profile =
  PROFILES[ACTIVE_SCENARIO];

// ============================================================
// HEALTHY BASELINE
// ============================================================

const float EXPECTED_SURFACE_PER_L = 60.0;

const float EXPECTED_ROOT_PER_L = 30.0;

// ============================================================
// PRESSURE THRESHOLDS
//
// These are INDEX values, NOT kPa/bar.
// Pressure is now read from Pot3.
// ============================================================

const float PRESSURE_LOW = 30.0;

const float PRESSURE_HIGH = 70.0;

// ============================================================
// CLASSIFIER THRESHOLDS
// ============================================================

const float LOW_RESPONSE = 0.50;

const float GOOD_SURFACE = 0.70;

const float LOW_ROOT = 0.40;

const float GOOD_RESPONSE = 0.70;

const float OVERWATER_VOLUME_MIN = 1.50;

// ============================================================
// VARIABLES
// ============================================================

float surfaceMoisture;

float rootMoisture;

float pressureIndex;

float surfaceStart;

float rootStart;

float surfaceRise = 0.0;

float rootRise = 0.0;

float litresTotal = 0.0;

float litresAfterDelay = 0.0;

float averagePressure = 0.0;

unsigned long pressureSamples = 0;

unsigned long secondsElapsed = 0;

unsigned long lastSample;

volatile unsigned long pulseCount = 0;

bool finished = false;

// ============================================================
// FLOW SENSOR INTERRUPT
// ============================================================

void IRAM_ATTR flowPulseISR() {

  pulseCount++;
}

// ============================================================
// READ PRESSURE FROM POTENTIOMETER
// ============================================================

float readPotPressure() {

  int pressureRaw =
    analogRead(PRESSURE_PIN);

  float pressure =
    (pressureRaw / 4095.0) * 100.0;

  return constrain(
    pressure,
    0.0,
    100.0
  );
}

// ============================================================
// GET CURRENT PRESSURE
// ============================================================
//
// IMPORTANT:
// Pressure ALWAYS comes from GPIO32 now.
// No predefined scenario pressure is used.
// ============================================================

float getPressure() {

  return readPotPressure();
}

// ============================================================
// CLASSIFIER
// ============================================================

const char* classify(
  float& volumeRatio,
  float& surfaceRatio,
  float& rootRatio
) {

  // ----------------------------------------------------------
  // WATER DELIVERY RATIO
  // ----------------------------------------------------------

  volumeRatio =
    litresTotal / PLANNED_LITRES;

  // ----------------------------------------------------------
  // SURFACE RESPONSE PER LITRE
  // ----------------------------------------------------------

  float surfacePerL = 0.0;

  if (litresTotal > 0.0) {

    surfacePerL =
      surfaceRise / litresTotal;
  }

  // ----------------------------------------------------------
  // ROOT RESPONSE PER LITRE
  // ----------------------------------------------------------

  float rootPerL = 0.0;

  if (litresAfterDelay > 0.0) {

    rootPerL =
      rootRise / litresAfterDelay;
  }

  // ----------------------------------------------------------
  // RESPONSE RELATIVE TO HEALTHY BASELINE
  // ----------------------------------------------------------

  surfaceRatio =
    surfacePerL / EXPECTED_SURFACE_PER_L;

  rootRatio =
    rootPerL / EXPECTED_ROOT_PER_L;

  // ==========================================================
  // RULE 1
  // OVERWATERING
  // ==========================================================

  if (volumeRatio > OVERWATER_VOLUME_MIN) {

    return "OVERWATERING";
  }

  // ==========================================================
  // RULE 2
  // CLOG
  //
  // High pressure
  // +
  // poor surface/root response
  // ==========================================================

  if (
    averagePressure >= PRESSURE_HIGH &&
    surfaceRatio < LOW_RESPONSE &&
    rootRatio < LOW_RESPONSE
  ) {

    return "POSSIBLE CLOG";
  }

  // ==========================================================
  // RULE 3
  // LEAK
  //
  // Low pressure
  // +
  // poor surface/root response
  // ==========================================================

  if (
    averagePressure <= PRESSURE_LOW &&
    surfaceRatio < LOW_RESPONSE &&
    rootRatio < LOW_RESPONSE
  ) {

    return "POSSIBLE LEAK";
  }

  // ==========================================================
  // RULE 4
  // POOR ROOT PENETRATION
  //
  // Surface responds well
  // Root responds poorly
  // ==========================================================

  if (
    surfaceRatio >= GOOD_SURFACE &&
    rootRatio < LOW_ROOT
  ) {

    return "INSUFFICIENT ROOT WETTING";
  }

  // ==========================================================
  // RULE 5
  // NORMAL
  // ==========================================================

  if (
    surfaceRatio >= GOOD_RESPONSE &&
    rootRatio >= GOOD_RESPONSE
  ) {

    return "NORMAL";
  }

  // ==========================================================
  // OTHERWISE
  // ==========================================================

  return "UNKNOWN ABNORMAL";
}

// ============================================================
// FINAL REPORT
// ============================================================

void printReport() {

  float volumeRatio;

  float surfaceRatio;

  float rootRatio;

  const char* diagnosis =
    classify(
      volumeRatio,
      surfaceRatio,
      rootRatio
    );

  // ----------------------------------------------------------
  // AVERAGE FLOW
  // ----------------------------------------------------------

  float averageFlowLPM = 0.0;

  if (profile.durationSec > 0) {

    averageFlowLPM =
      (litresTotal / profile.durationSec) * 60.0;
  }

  // ----------------------------------------------------------
  // REPORT
  // ----------------------------------------------------------

  Serial.println();

  Serial.println("================================================");

  Serial.println(
    "          ROOTSENSE DIAGNOSTIC REPORT"
  );

  Serial.println("================================================");

  Serial.println();

  Serial.print("Simulation scenario : ");
  Serial.println(profile.name);

  Serial.print("Expected diagnosis  : ");
  Serial.println(profile.expectedLabel);

  // ==========================================================
  // FLOW
  // ==========================================================

  Serial.println();

  Serial.println("FLOW");

  Serial.print("Water delivered     : ");
  Serial.print(litresTotal, 3);
  Serial.println(" L");

  Serial.print("Planned water       : ");
  Serial.print(PLANNED_LITRES, 3);
  Serial.println(" L");

  Serial.print("Average flow        : ");
  Serial.print(averageFlowLPM, 3);
  Serial.println(" L/min");

  Serial.print("Volume ratio        : ");
  Serial.print(volumeRatio * 100.0, 1);
  Serial.println("%");

  // ==========================================================
  // PRESSURE
  // ==========================================================

  Serial.println();

  Serial.println("PRESSURE");

  Serial.print("Measured pressure   : ");
  Serial.print(averagePressure, 2);
  Serial.println(" / 100");

  // ==========================================================
  // SURFACE
  // ==========================================================

  Serial.println();

  Serial.println("SURFACE MOISTURE");

  Serial.print("Starting value      : ");
  Serial.println(surfaceStart, 2);

  Serial.print("Final value         : ");
  Serial.println(surfaceMoisture, 2);

  Serial.print("Moisture rise       : ");
  Serial.println(surfaceRise, 2);

  Serial.print("Response vs healthy : ");
  Serial.print(surfaceRatio * 100.0, 1);
  Serial.println("%");

  // ==========================================================
  // ROOT
  // ==========================================================

  Serial.println();

  Serial.println("ROOT MOISTURE");

  Serial.print("Starting value      : ");
  Serial.println(rootStart, 2);

  Serial.print("Final value         : ");
  Serial.println(rootMoisture, 2);

  Serial.print("Moisture rise       : ");
  Serial.println(rootRise, 2);

  Serial.print("Response vs healthy : ");
  Serial.print(rootRatio * 100.0, 1);
  Serial.println("%");

  // ==========================================================
  // RESULT
  // ==========================================================

  Serial.println();

  Serial.println("RESULT");

  Serial.print("Detected diagnosis  : ");
  Serial.println(diagnosis);

  if (
    strcmp(
      diagnosis,
      profile.expectedLabel
    ) == 0
  ) {

    Serial.println("Test result         : PASS");

  } else {

    Serial.println("Test result         : FAIL");
  }

  Serial.println();

  Serial.println("================================================");
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  // ----------------------------------------------------------
  // USB SERIAL
  // ----------------------------------------------------------

  Serial.begin(115200);

  // ----------------------------------------------------------
  // UART1 -> WOKWI PLOTTER
  // ----------------------------------------------------------

  PlotSerial.begin(
    115200,
    SERIAL_8N1,
    16,
    17
  );

  // ----------------------------------------------------------
  // INPUTS
  // ----------------------------------------------------------

  pinMode(
    SURFACE_PIN,
    INPUT
  );

  pinMode(
    ROOT_PIN,
    INPUT
  );

  pinMode(
    PRESSURE_PIN,
    INPUT
  );

  pinMode(
    FLOW_PIN,
    INPUT
  );

  // ----------------------------------------------------------
  // FLOW INTERRUPT
  // ----------------------------------------------------------

  attachInterrupt(
    digitalPinToInterrupt(FLOW_PIN),
    flowPulseISR,
    RISING
  );

  // ==========================================================
  // INITIAL SENSOR VALUES
  // ==========================================================

  int surfaceRaw =
    analogRead(SURFACE_PIN);

  int rootRaw =
    analogRead(ROOT_PIN);

  surfaceMoisture =
    (surfaceRaw / 4095.0) * 100.0;

  rootMoisture =
    (rootRaw / 4095.0) * 100.0;

  // ==========================================================
  // INITIAL PRESSURE
  // ==========================================================

  pressureIndex =
    getPressure();

  // ==========================================================
  // SAVE START VALUES
  // ==========================================================

  surfaceStart =
    surfaceMoisture;

  rootStart =
    rootMoisture;

  // ==========================================================
  // START TIMER
  // ==========================================================

  lastSample =
    millis();

  secondsElapsed = 0;

  // ==========================================================
  // START MESSAGE
  // ==========================================================

  Serial.println();

  Serial.println(
    "RootSense Phase 2 starting..."
  );

  Serial.print("Scenario: ");
  Serial.println(profile.name);

  Serial.println();
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  // ----------------------------------------------------------
  // STOP AFTER FINISH
  // ----------------------------------------------------------

  if (finished) {

    return;
  }

  unsigned long now =
    millis();

  // ----------------------------------------------------------
  // SAMPLE EVERY SECOND
  // ----------------------------------------------------------

  if (
    now - lastSample >= 1000
  ) {

    lastSample += 1000;

    secondsElapsed++;

    // ========================================================
    // END OF SCENARIO
    // ========================================================

    if (
      secondsElapsed > profile.durationSec
    ) {

      finished = true;

      printReport();

      return;
    }

    // ========================================================
    // PRESSURE
    // ========================================================

    pressureIndex =
      getPressure();

    pressureSamples++;

    averagePressure +=
      pressureIndex;

    // ========================================================
    // FLOW PULSES
    // ========================================================

    noInterrupts();

    unsigned long pulses =
      pulseCount;

    pulseCount = 0;

    interrupts();

    // ========================================================
    // PULSES -> LITRES
    // ========================================================

    float waterThisSecond =
      pulses / PULSES_PER_LITER;

    litresTotal +=
      waterThisSecond;

    // ========================================================
    // SURFACE RESPONSE
    // ========================================================

    surfaceMoisture +=
      waterThisSecond *
      profile.surfaceGain;

    // ========================================================
    // ROOT RESPONSE
    // ========================================================

    if (
      secondsElapsed > ROOT_DELAY_SEC
    ) {

      rootMoisture +=
        waterThisSecond *
        profile.rootGain;

      litresAfterDelay +=
        waterThisSecond;
    }

    // ========================================================
    // LIMIT DISPLAY VALUES
    // ========================================================

    surfaceMoisture =
      constrain(
        surfaceMoisture,
        0.0,
        100.0
      );

    rootMoisture =
      constrain(
        rootMoisture,
        0.0,
        100.0
      );

    // ========================================================
    // MOISTURE RISE
    // ========================================================

    surfaceRise =
      surfaceMoisture -
      surfaceStart;

    rootRise =
      rootMoisture -
      rootStart;

    // ========================================================
    // FLOW RATE FOR PLOTTER
    // ========================================================

    float flowLPM =
      waterThisSecond * 60.0;

    // ========================================================
    // PLOT FOUR SIGNALS
    // ========================================================

    PlotSerial.print(
      "Surface:"
    );

    PlotSerial.print(
      surfaceMoisture,
      2
    );

    PlotSerial.print(
      "\tRoot:"
    );

    PlotSerial.print(
      rootMoisture,
      2
    );

    PlotSerial.print(
      "\tPressure:"
    );

    PlotSerial.print(
      pressureIndex,
      2
    );

    PlotSerial.print(
      "\tFlow:"
    );

    PlotSerial.println(
      flowLPM,
      3
    );
  }
}
