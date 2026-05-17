# HPA Calibration Workflow

This guide describes the workflow for calibrating an HPA power prediction model using the provided tools.

## Repository Overview
| Directory | Description |
|---|---|
| `modbus` | Everything connected to the Modbus device (read voltages, wiring, et cetera) |
| `calibration-tool` | The main Calibration Tool GUI application |
| `calibration-power-library` | The backend cpp code for power estimation (uses calibration data) |

## Requirements

- Power meter connected to the HPA
- Calibration GUI tool (`calibration-tool/calibration-tool-gui.py`)
- Voltage reading CLI tool (`modbus/read-voltage-cli-tool/`)

---

## 1. Measurement Setup

1. Connect the power meter to the HPA. (Make sure to use a load element as the end point.)
2. Upload the voltage reading tool to the Raspberry Pi.
3. Compile the voltage reading tool on the Pi:

```bash
cd ./modbus/read-voltage-cli-tool/
./build.sh
```

4. Start the calibration GUI on a local computer:

```bash
python ./calibration-tool/calibration-tool.py
```

---

## 2. Collect Calibration Data

### Configure the HPA

1. Set the HPA to the lowest supported frequency (example: `400 MHz`).
2. Set the HPA to the lowest supported gain (example: `20`).
3. Enable the HPA.

### Record Measurements

4. Run the voltage reading tool on the Pi and record the measured voltage.

```bash
./read-voltage
```

5. Read the power as well, and all the necessary values for one measurement is compiled:
   - Voltage
   - Measured power (W)
   - Frequency (Hz)

6. Enter the values into the Calibration Tool.

### Sweep Gain Values

7. Repeat the measurement process for increasing gain values until the highest supported gain is reached.

Example:

```text
20 → 22 → 24 → 26 → 28 → 30
```

### Sweep Frequency Values

8. Repeat the entire process for at least two additional frequencies.

Example:

```text
400 MHz → 440 MHz → 480 MHz
```

> For best results, use:
>
> - The lowest supported frequency
> - The highest supported frequency
> - One midpoint frequency

At the end of this process, the Calibration Tool should contain **15–18 measurement samples**.

---

## 3. Run Calibration

1. Press **Run Calibration** in the Calibration Tool.
2. (Optional) Use the Prediction Tool to test values outside the measured range and evaluate prediction accuracy.
3. (Optional) Repeat the entire workflow for additional HPAs.

---

## 4. Export Configuration

1. Save the generated coefficients as a JSON file (`Save config.json`).
2. Upload the JSON configuration file to the Raspberry Pi.

