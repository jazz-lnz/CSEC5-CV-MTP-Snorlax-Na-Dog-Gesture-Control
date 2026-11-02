# Gesture Control — Vision-Based Autonomous 4WD Robot (SnorlaxTheDog)

Computer Vision group project.

* **Midterm (done): SEE → RECOGNIZE.** A YOLO11n detector recognizes 5 hand gestures from the laptop webcam.
* **Final (in progress): SEE → DECIDE → ACT.** The robot acts on what its camera sees.
  * Current stage: the robot is wired and can be driven from the laptop keyboard over USB.

| Gesture | HaGRID class | Robot command (Final) |
|---|---|---|
| Fist | `fist` | STOP |
| Thumbs Up | `like` | FORWARD |
| Open Palm | `palm` | BACKWARD |
| Rock Sign | `rock` | LEFT |
| Call Sign | `call` | RIGHT |

Pipeline: **Webcam → Preprocessing (CLAHE when dark, letterbox 640, normalize) → YOLO11n (ONNX) → Detection → Visual output**

Robot control (current): **Laptop keyboard → `controller.py` → USB serial → Arduino Uno → L298N → 4 DC motors**

## Layout
```
src/config.py                         classes, commands, thresholds
src/training/train_colab.ipynb        HaGRID (Kaggle) download, split, training, evaluation, ONNX export (Colab)
src/vision/preprocess.py              preprocessing
src/vision/detector.py                ONNX YOLO detector (onnxruntime)
src/vision/demo_app.py                demo app: live webcam, image mode, pipeline view
src/robot/controller.py               laptop -> Arduino link; drive the robot with the keyboard
arduino/motor_test/motor_test.ino     wiring check: forward, backward, left, right in a loop
arduino/robot_controller/robot_controller.ino   receives commands from the laptop and drives the motors
models/  results/midterm/
```

## Workflow (computer vision)
1. **Laptop setup**
   * **Windows:**
     ```powershell
     python -m venv .venv
     .venv\Scripts\python -m pip install -r requirements.txt
     ```
   * **macOS:**
     ```bash
     python3 -m venv .venv
     .venv/bin/python -m pip install -r requirements.txt
     ```
   PyTorch is not installed locally (Windows Application Control blocks its DLLs on this laptop),
   so the model runs through ONNX Runtime.
2. **Train on Colab**: open `src/training/train_colab.ipynb`, choose a T4 GPU runtime, and run all cells.
3. **Bring results back**: download `MyDrive/SnorlaxTheDog_Gestures/midterm_results.zip`, put `best.onnx`
   in `models/`, and extract `figures/` and `tables/` into `results/midterm/`.
4. **Demo app** (live webcam + instructor images + pipeline view):
   * **Windows:** `.venv\Scripts\python src/vision/demo_app.py`
   * **macOS:** `.venv/bin/python src/vision/demo_app.py`

   Keys: `p` pipeline view, `o` open image(s), `f` open folder, `c` camera, `←/→` previous/next image, `s` screenshot,
   `m` mirror, `l` low-light, `q` quit. Use `--images <file-or-folder>` to start in image mode.
6. **Batch predictions** (annotated copies + CSV): `.venv\Scripts\python src/vision/predict_images.py --source <folder>`

## Robot hardware

| Component | Purpose |
|---|---|
| 4WD chassis with 4 DC motors | robot body and drive |
| L298N motor driver | switches motor power and direction (the Arduino pins cannot power motors directly) |
| Arduino Uno | receives commands from the laptop and controls the L298N |
| 6 × AA batteries in series (≈ 9 V) | motor power only |
| Laptop USB / powerbank | Arduino power (kept separate so motor current spikes don't reset the Arduino) |

### Wiring: L298N → Arduino
| L298N | Arduino Uno | Notes |
|---|---|---|
| ENA | D5 | PWM (speed)|
| IN1 | D7 | direction |
| IN2 | D8 | direction |
| IN3 | D9 | direction |
| IN4 | D10 | direction |
| ENB | D6 | PWM (speed)|
| GND | GND | **shared ground**|

Left motors → OUT1/OUT2, right motors → OUT3/OUT4. Leave the L298N **5V** terminal unconnected.

### Battery connections
* **Battery holder red wire (`+`)** → L298N **`+12V`** terminal (the label is the maximum input;
  ≈ 9 V from 6 × AA is correct).
* **Battery holder black wire (`-`)** → L298N **`GND`** terminal, together with a jumper wire to the
  **Arduino `GND`** pin. The Arduino and the L298N must share a ground, or the L298N can't read the Arduino's signals.
* **Two holders (4-slot + 2-slot):** connect Holder 1's **red (`+`)** to Holder 2's **black (`-`)** (in series).
  Then use the remaining red and black wires as above.
  * Check with a multimeter (DC V) across the free red and black wires: about **9 V** with alkaline AAs.
* The **Arduino** is powered through its own USB port (laptop or powerbank), never from the motor batteries.

## Robot control (current stage: keyboard over USB)

### 1. Upload the Arduino sketches (Arduino IDE)
1. Board: **Arduino Uno**. Port: the Arduino's USB port.
2. **Wiring check:** upload `arduino/motor_test/motor_test.ino` with the robot on a box (wheels in the air).
   * It repeats: forward, backward, spin left, spin right (1.5 s each) and prints each step in the Serial Monitor (9600 baud).
3. **Controller:** upload `arduino/robot_controller/robot_controller.ino`.
   * In the Serial Monitor, typing `F` should print `F`, then `WATCHDOG STOP` half a second later.

**Direction fixes for our chassis:** our right-side motors are wired reversed, and the chassis front is the
opposite end. Both sketches correct this in code:
* `IN3`/`IN4` are swapped to pins `10, 9`.
* Forward and backward are flipped.

The physical wiring stays as in the table above.

### 2. Drive from the laptop
Close the Arduino Serial Monitor first (only one program can use the port at a time).

* **macOS:**
  ```bash
  .venv/bin/python src/robot/controller.py                              # lists the serial ports
  .venv/bin/python src/robot/controller.py --port /dev/cu.usbserial-110  # use your port from the list
  ```
* **Windows:**
  ```powershell
  .venv\Scripts\python src/robot/controller.py
  .venv\Scripts\python src/robot/controller.py --port COM3
  ```

Click inside the terminal, then:

| Key | Action |
|---|---|
| hold `W` / `S` | forward / backward |
| hold `A` / `D` | spin left / spin right |
| `Space` | stop |
| `0`–`9` | speed (0 = slowest, 9 = full) |
| `Q` | quit (the robot stops) |

Releasing a key stops the robot.

### How the control works
* The laptop sends one letter per command (`F` forward, `B` backward, `L` left, `R` right, `S` stop, `0`–`9` speed),
  10 times per second, over USB serial at 9600 baud.
* The Arduino sets the L298N direction pins and the PWM speed.
  * Speed level `0`–`9` maps to PWM 110–255. Below about 110 the motors only hum.
* **Safety watchdog:** if the Arduino receives no command for 0.5 s (laptop crash, program closed, cable pulled),
  it stops the motors by itself.

## Dataset
See [dataset/README.md](dataset/README.md). HaGRID Sample 30k 384p from Kaggle (CC BY-SA 4.0).