# PC System Monitor on an Arduino LCD

Displays the real-time status of your PC (CPU, RAM, disk, network, temperature, battery, uptime...) on a 16x2 I2C LCD connected to an Arduino Uno R3.

A 16x2 screen only fits 32 characters, so the sketch rotates through **11 pages**. Each page stays on screen for 4 seconds, or you can jump to the next one with an optional push button.

An Arduino cannot read the PC's status by itself, so the project has two parts:

| File | Runs on | Purpose |
|------|---------|---------|
| `send_system_stats.py` | PC | Reads the system values with `psutil` and sends them over USB (serial) once per second. |
| `pc_system_monitor_lcd.ino` | Arduino Uno R3 | Receives the values, formats them and shows them on the LCD. |
| `requirements.txt` | PC | List of Python packages needed by the script. |

```
PC (Python + psutil)  --USB serial-->  Arduino Uno  --I2C-->  16x2 LCD
```

## Table of contents

1. [Pages shown on the LCD](#pages-shown-on-the-lcd)
2. [Hardware and wiring](#hardware-and-wiring)
3. [What you need to install](#what-you-need-to-install)
4. [Running the project](#running-the-project)
5. [Troubleshooting](#troubleshooting)
6. [Values read from the PC](#values-read-from-the-pc)
7. [How it works](#how-it-works)
8. [Customization](#customization)

## Pages shown on the LCD

| # | Row 1 | Row 2 |
|---|-------|-------|
| 1 | `CPU: 45.3%` | `Freq: 3400 MHz` |
| 2 | `CPU Cores: 8` | `Load 1m: 1.25` |
| 3 | `CPU Temp: 62.5°C` | `Fan: 1200 RPM` |
| 4 | `RAM: 45.3%` | `8123/16000 MB` |
| 5 | `Swap: 12.0%` | `Processes: 312` |
| 6 | `Disk: 45.3%` | `120.5/500.0 GB` |
| 7 | `Rd: 1.20 MB/s` | `Wr: 350.0 KB/s` |
| 8 | `↓ 1.25 MB/s` | `↑ 85.3 KB/s` |
| 9 | `Recv: 12.34 GB` | `Sent: 1.20 GB` |
| 10 | `Battery: 85%` | `Plugged: Yes` |
| 11 | `Uptime:` | `2d 03h 15m 07s` |

Values that are not available on your PC are shown as `N/A`. If the PC stops sending data for 3 seconds, the LCD shows `No data from PC`.

## Hardware and wiring

**Components**

- Arduino Uno R3
- 16x2 LCD with I2C backpack (PCF8574)
- USB cable (type A to B)
- Jumper wires
- Push button (optional)

**Wiring**

| LCD I2C | Arduino Uno |
|---------|-------------|
| GND | GND |
| VCC | 5V |
| SDA | A4 |
| SCL | A5 |

**Optional button:** connect one pin to `D2` and the other pin to `GND`. The internal pull-up resistor is used, so no external resistor is needed. Each press jumps to the next page.

## What you need to install

### 1. Arduino IDE

Download it from [arduino.cc/en/software](https://www.arduino.cc/en/software) (version 1.8.x or 2.x). Install it and connect the Arduino to the PC with the USB cable.

### 2. LiquidCrystal_I2C library

In the Arduino IDE, open the Library Manager (*Sketch → Include Library → Manage Libraries...*), search for **LiquidCrystal I2C** and install it.

> **Important:** there are several libraries with the same name `LiquidCrystal_I2C`, and they do not all use the same method to start the display:
>
> - Some use `lcd.begin()` (this is what the sketch uses by default).
> - Others use `lcd.init()` or `lcd.begin(16, 2)`.
>
> The sketch contains this line in `setup()`:
>
> ```cpp
> lcd.begin();
> ```
>
> If the compiler complains about `no matching function for call to 'LiquidCrystal_I2C::begin()'`, replace it with `lcd.begin(16, 2);`. If it complains about `'class LiquidCrystal_I2C' has no member named 'begin'`, replace it with `lcd.init();`.
>
> If you still get errors, check the folder `Documents\Arduino\libraries` and make sure you only have **one** copy of `LiquidCrystal_I2C` (delete duplicates and restart the IDE).

### 3. Python

You need **Python 3.8 or newer**. Download it from [python.org/downloads](https://www.python.org/downloads/).

- **Windows:** during installation, tick **"Add python.exe to PATH"**.
- **Linux / macOS:** Python 3 is usually already installed.

Check that it works by typing this in a terminal (not inside Python, see [where to type commands](#where-to-type-commands)):

```bash
python --version
```

On Linux and macOS use `python3 --version`. On Windows, if `python` is not recognized, try `py --version`.

### 4. Python packages (`psutil` and `pyserial`)

The script needs two packages:

| Package | Purpose |
|---------|---------|
| `psutil` | Reads CPU, RAM, disk, network, temperature, battery... |
| `pyserial` | Sends the data to the Arduino over the USB serial port |

Open a terminal in the project folder and run:

**Windows**

```bash
python -m pip install -r requirements.txt
```

If `python` does not work, use `py -m pip install -r requirements.txt`.

**macOS**

```bash
python3 -m pip install -r requirements.txt
```

**Linux (Debian, Ubuntu, Raspberry Pi OS...)**

If you get `error: externally-managed-environment`, use one of these options:

*Option A: virtual environment (recommended)*

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

If `venv` is not available, run `sudo apt install python3-venv` first. Each time you want to run the script, activate the environment again with `source venv/bin/activate`.

*Option B: system packages*

```bash
sudo apt install python3-psutil python3-serial
```

> Always use `python -m pip ...` instead of just `pip ...`. That way the packages are installed in the same Python that will run the script.

To check that `psutil` is installed:

```bash
python -m pip show psutil
```

### 5. Visual Studio Code (optional)

You can run the script from any terminal, but if you prefer VS Code:

1. Install [VS Code](https://code.visualstudio.com/) and the **Python** extension.
2. Open the project folder (*File → Open Folder...*).
3. Select the Python interpreter where you installed the packages: press `Ctrl + Shift + P`, type **Python: Select Interpreter** and choose it.
4. Open the integrated terminal from *Terminal → New Terminal*.

## Running the project

### 1. Upload the sketch to the Arduino

1. Open `pc_system_monitor_lcd.ino` in the Arduino IDE. The file must stay inside a folder with the same name (`pc_system_monitor_lcd`).
2. Select **Tools → Board → Arduino Uno** and the correct **Tools → Port**.
3. Click **Upload**.

The LCD should show `Waiting for PC..`.

### 2. Test the Arduino alone (optional)

1. Open the Serial Monitor (magnifier icon or `Ctrl + Shift + M`).
2. Set the speed to **9600 baud** and the line ending to **Newline**.
3. Paste this line and send it:
   ```
   452,3400,8,625,1200,125,453,8123,16000,120,312,453,1205,5000,1250000,358400,1310720,87340,12640,1229,85,1,183307
   ```
4. The LCD should show the first page.

The screen goes back to `No data from PC` after 3 seconds because the sketch expects continuous data. This is normal.

### 3. Find the serial port of the Arduino

- **Arduino IDE:** *Tools → Port* (for example `COM3`).
- **Windows:** Device Manager → *Ports (COM & LPT)*.
- **Linux:** `ls /dev/ttyACM* /dev/ttyUSB*` (usually `/dev/ttyACM0`).
- **macOS:** `ls /dev/cu.usbmodem*`.

**Linux permissions:** if you get `Permission denied` when opening the port, add your user to the `dialout` group, then log out and log in again:

```bash
sudo usermod -a -G dialout $USER
```

### 4. Close the Serial Monitor

**Close the Serial Monitor of the Arduino IDE** before running the script. Only one program can use the serial port at a time.

### 5. Run the script

In a terminal, inside the project folder:

```bash
python send_system_stats.py COM3              # Windows
python3 send_system_stats.py /dev/ttyACM0     # Linux
python3 send_system_stats.py /dev/cu.usbmodem1101   # macOS
```

Replace the port with the one you found in step 3. The console prints one line per second and the LCD updates continuously. Stop it with `Ctrl + C`.

**Before unplugging the Arduino,** stop the script with `Ctrl + C`. If you unplug it while the script is running, the script prints a connection error and exits. When you reconnect the board, check that the port name has not changed and run the script again.

### Where to type commands

Commands like `python -m pip install ...` or `python send_system_stats.py COM3` must be typed in the **terminal** (PowerShell, CMD, Terminal...), not inside the Python interpreter.

- If the line starts with `>>>`, you are **inside Python**. Type `exit()` and press Enter to leave it.
- If the line starts with a path (for example `PS C:\Users\you\project>`), you are in the terminal and can type the commands.

If you type a terminal command inside Python, you will get a `SyntaxError`.

## Troubleshooting

| Problem | Cause and solution |
|---------|--------------------|
| `no matching function for call to 'LiquidCrystal_I2C::begin()'` or `no member named 'init'` / `'begin'` | Another version of the library is installed. Use `lcd.begin(16, 2);` or `lcd.init();` in `setup()` (see [LiquidCrystal_I2C library](#2-liquidcrystal_i2c-library)). |
| `ModuleNotFoundError: No module named 'psutil'` | The package is not installed in the Python you are using. Run `python -m pip install psutil pyserial` and select the right interpreter in VS Code. |
| `No module named 'serial'` | You installed the wrong package. Run `python -m pip uninstall serial` and then `python -m pip install pyserial`. |
| `SyntaxError` when typing `python -m pip ...` | You typed it inside the Python interpreter (`>>>`). Type `exit()` and use the terminal. |
| `python` is not recognized (Windows) | Try `py` instead, or reinstall Python ticking **"Add python.exe to PATH"**. |
| `error: externally-managed-environment` (Linux) | Use a virtual environment or `apt` (see [Python packages](#4-python-packages-psutil-and-pyserial)). |
| `could not open port` / `Access denied` | The port is busy (Serial Monitor open) or the name is wrong. |
| `Permission denied` on `/dev/ttyACM0` (Linux) | Add your user to the `dialout` group (see [step 3](#3-find-the-serial-port-of-the-arduino)). |
| Only `Waiting for PC..` is shown | The Arduino is not receiving valid lines. Test it with the Serial Monitor. If that works, the problem is on the PC side (wrong port, script not running, port busy). |
| Blank screen or only white blocks | Try the I2C address `0x3F` instead of `0x27` in the sketch and adjust the contrast potentiometer on the I2C backpack. |
| Many `N/A` values | Temperature and fan speed are only available on Linux, and battery only on laptops. This is a `psutil` limitation, not a bug. |
| CPU usage is `0.0%` at start | The first measurement is always 0. It is correct from the second one. |

**Tip:** when everything works, the TX LED on the Uno blinks once per second. If it does not blink, the data is not reaching the board.

## Values read from the PC

| Value | Source (`psutil`) | Availability |
|-------|-------------------|--------------|
| CPU usage (%) | `cpu_percent()` | All systems |
| CPU frequency (MHz) | `cpu_freq()` | Most systems |
| Logical CPU cores | `cpu_count()` | All systems |
| CPU temperature | `sensors_temperatures()` | Linux / FreeBSD only |
| Fan speed (RPM) | `sensors_fans()` | Linux only |
| Load average (1 min) | `getloadavg()` | All systems (emulated on Windows) |
| RAM usage (%, used, total) | `virtual_memory()` | All systems |
| Swap usage (%) | `swap_memory()` | All systems |
| Number of processes | `pids()` | All systems |
| Disk usage (%, used, total) | `disk_usage()` | All systems (main disk) |
| Disk read / write speed | `disk_io_counters()` | Most systems |
| Network download / upload speed | `net_io_counters()` | All systems (all interfaces added up) |
| Network data received / sent since boot | `net_io_counters()` | All systems |
| Battery level and charger status | `sensors_battery()` | Laptops only |
| Uptime | `boot_time()` | All systems |

## How it works

### Serial protocol

The PC sends one line per second with **23 comma-separated integers** followed by a newline. A value of `-1` means "not available".

```
452,3400,8,625,1200,125,453,8123,16000,120,312,453,1205,5000,1250000,358400,1310720,87340,12640,1229,85,1,183307
```

| # | Field | Unit |
|---|-------|------|
| 1 | `cpu_percent` | tenths of % |
| 2 | `cpu_freq_mhz` | MHz |
| 3 | `cpu_cores` | cores |
| 4 | `cpu_temp` | tenths of °C |
| 5 | `fan_rpm` | RPM |
| 6 | `load_avg` | hundredths |
| 7 | `ram_percent` | tenths of % |
| 8 | `ram_used_mb` | MB |
| 9 | `ram_total_mb` | MB |
| 10 | `swap_percent` | tenths of % |
| 11 | `process_count` | processes |
| 12 | `disk_percent` | tenths of % |
| 13 | `disk_used_gb` | tenths of GB |
| 14 | `disk_total_gb` | tenths of GB |
| 15 | `disk_read_bps` | bytes/s |
| 16 | `disk_write_bps` | bytes/s |
| 17 | `net_down_bps` | bytes/s |
| 18 | `net_up_bps` | bytes/s |
| 19 | `net_recv_mb` | MB |
| 20 | `net_sent_mb` | MB |
| 21 | `battery_percent` | % |
| 22 | `battery_plugged` | 1 / 0 |
| 23 | `uptime_seconds` | seconds |

Decimals are sent as scaled integers (for example `452` = `45.2 %`) because floating point formatting is limited on the Arduino Uno.

The order must be the same in the `FIELD_NAMES` tuple of the Python script and in the `Field` enum of the sketch. If you add a new value, add it in both places and update `FIELD_COUNT`.

### Arduino sketch (`pc_system_monitor_lcd.ino`)

1. Initializes the LCD and creates two custom characters (down and up arrows).
2. Reads the serial port until it receives a newline character.
3. Splits the line into 23 values and rejects lines with a wrong number of values.
4. Draws the current page. The page changes every 4 seconds, or when the button is pressed.
5. If no valid data arrives for 3 seconds, it shows a warning message.

### Python script (`send_system_stats.py`)

1. Opens the serial port given as a command-line argument (9600 baud).
2. Every second, reads all the system values with `psutil`. Values that are not supported on your system are sent as `-1`.
3. Speeds are calculated from the difference between two counter readings divided by the elapsed time.
4. Sends the values as a single line and prints it in the console.
5. The Uno resets when the port is opened, so the script waits 2 seconds before sending data.

## Customization

- **Page duration:** change `PAGE_INTERVAL_MS` in the sketch.
- **Different disk:** the script reads the main disk (`/` or `C:\`). Change `self.disk_path` in `SystemMonitor` to monitor another one.
- **Single network interface:** use `psutil.net_io_counters(pernic=True)["interface_name"]` instead of `psutil.net_io_counters()`.
- **Different LCD:** for a 20x4 LCD, change the size in `LiquidCrystal_I2C lcd(0x27, 20, 4)` and adapt the pages.

## License

Released under the MIT License. Feel free to use and modify it.
