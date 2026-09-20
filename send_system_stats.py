"""
Reads system information from the PC with psutil and sends it to the Arduino
over the serial port, one line per second.

Install:  pip install -r requirements.txt
Usage:    python send_system_stats.py COM3             (Windows)
          python send_system_stats.py /dev/ttyACM0     (Linux)
          python send_system_stats.py /dev/cu.usbmodem1101  (macOS)

Close the Arduino IDE Serial Monitor before running this script.

Every line has the format: value1,value2,...,value23
All values are integers. A value of -1 means "not available on this PC".
The order of FIELD_NAMES must match the Field enum in the Arduino sketch.
"""

import os
import sys
import time

import psutil
import serial

BAUD_RATE = 9600
INTERVAL = 1.0  # seconds between measurements

# Order of the values sent to the Arduino (same as the Field enum in the sketch)
FIELD_NAMES = (
    "cpu_percent",      # tenths of %
    "cpu_freq_mhz",     # MHz
    "cpu_cores",        # logical cores
    "cpu_temp",         # tenths of degrees Celsius
    "fan_rpm",          # RPM
    "load_avg",         # hundredths (1 minute load average)
    "ram_percent",      # tenths of %
    "ram_used_mb",      # MB
    "ram_total_mb",     # MB
    "swap_percent",     # tenths of %
    "process_count",    # number of processes
    "disk_percent",     # tenths of %
    "disk_used_gb",     # tenths of GB
    "disk_total_gb",    # tenths of GB
    "disk_read_bps",    # bytes per second
    "disk_write_bps",   # bytes per second
    "net_down_bps",     # bytes per second
    "net_up_bps",       # bytes per second
    "net_recv_mb",      # MB received since boot
    "net_sent_mb",      # MB sent since boot
    "battery_percent",  # %
    "battery_plugged",  # 1 = plugged, 0 = on battery
    "uptime_seconds",   # seconds since boot
)

MB = 1024 * 1024
GB = 1024 * 1024 * 1024


def safe(function):
    """Runs a function and returns None if the value is not available on this system."""
    try:
        return function()
    except Exception:
        return None


def scaled(value, factor):
    """Scales a number to an integer, or returns -1 if the value is not available."""
    return -1 if value is None else int(round(value * factor))


def get_cpu_temperature():
    """Returns the CPU temperature in Celsius, or None (only supported on Linux/FreeBSD)."""
    if not hasattr(psutil, "sensors_temperatures"):
        return None
    sensors = safe(psutil.sensors_temperatures)
    if not sensors:
        return None
    for name in ("coretemp", "k10temp", "zenpower", "cpu_thermal", "cpu-thermal", "acpitz"):
        if sensors.get(name):
            return sensors[name][0].current
    first_sensor = next(iter(sensors.values()))
    return first_sensor[0].current if first_sensor else None


def get_fan_speed():
    """Returns the speed of the first fan in RPM, or None (only supported on Linux)."""
    if not hasattr(psutil, "sensors_fans"):
        return None
    fans = safe(psutil.sensors_fans)
    if not fans:
        return None
    for fan_list in fans.values():
        if fan_list:
            return fan_list[0].current
    return None


def bytes_per_second(current, previous, elapsed):
    """Speed between two counter readings, or -1 if the counters are not available."""
    if current is None or previous is None:
        return -1
    return max(0, int((current - previous) / elapsed))


class SystemMonitor:
    def __init__(self):
        psutil.cpu_percent(interval=None)  # the first call always returns 0.0
        self.disk_path = os.path.abspath(os.sep)  # "/" on Linux/macOS, "C:\\" on Windows
        self.previous_time = time.time()
        self.previous_net = psutil.net_io_counters()
        self.previous_disk_io = safe(psutil.disk_io_counters)

    def read(self):
        """Returns a list of integers in the order defined by FIELD_NAMES."""
        now = time.time()
        elapsed = now - self.previous_time

        net = psutil.net_io_counters()
        disk_io = safe(psutil.disk_io_counters)
        memory = psutil.virtual_memory()
        swap = safe(psutil.swap_memory)
        disk = safe(lambda: psutil.disk_usage(self.disk_path))
        frequency = safe(psutil.cpu_freq)
        load_average = safe(lambda: psutil.getloadavg()[0])
        battery = safe(psutil.sensors_battery) if hasattr(psutil, "sensors_battery") else None

        if battery is None or battery.power_plugged is None:
            battery_plugged = -1
        else:
            battery_plugged = 1 if battery.power_plugged else 0

        data = {
            "cpu_percent": scaled(psutil.cpu_percent(interval=None), 10),
            "cpu_freq_mhz": scaled(frequency.current if frequency else None, 1),
            "cpu_cores": psutil.cpu_count(logical=True) or 0,
            "cpu_temp": scaled(get_cpu_temperature(), 10),
            "fan_rpm": scaled(get_fan_speed(), 1),
            "load_avg": scaled(load_average, 100),
            "ram_percent": scaled(memory.percent, 10),
            "ram_used_mb": int(memory.used / MB),
            "ram_total_mb": int(memory.total / MB),
            "swap_percent": scaled(swap.percent if swap else 0, 10),
            "process_count": len(psutil.pids()),
            "disk_percent": scaled(disk.percent if disk else None, 10),
            "disk_used_gb": scaled(disk.used / GB if disk else None, 10),
            "disk_total_gb": scaled(disk.total / GB if disk else None, 10),
            "disk_read_bps": bytes_per_second(
                disk_io.read_bytes if disk_io else None,
                self.previous_disk_io.read_bytes if self.previous_disk_io else None,
                elapsed),
            "disk_write_bps": bytes_per_second(
                disk_io.write_bytes if disk_io else None,
                self.previous_disk_io.write_bytes if self.previous_disk_io else None,
                elapsed),
            "net_down_bps": bytes_per_second(net.bytes_recv, self.previous_net.bytes_recv, elapsed),
            "net_up_bps": bytes_per_second(net.bytes_sent, self.previous_net.bytes_sent, elapsed),
            "net_recv_mb": int(net.bytes_recv / MB),
            "net_sent_mb": int(net.bytes_sent / MB),
            "battery_percent": scaled(battery.percent if battery else None, 1),
            "battery_plugged": battery_plugged,
            "uptime_seconds": int(now - psutil.boot_time()),
        }

        self.previous_time = now
        self.previous_net = net
        self.previous_disk_io = disk_io

        return [data[name] for name in FIELD_NAMES]


def main():
    if len(sys.argv) < 2:
        print("Usage: python send_system_stats.py <serial_port>")
        sys.exit(1)

    try:
        arduino = serial.Serial(sys.argv[1], BAUD_RATE, timeout=1)
    except serial.SerialException as error:
        print(f"Could not open the serial port: {error}")
        sys.exit(1)

    time.sleep(2)  # the Uno resets when the port is opened
    monitor = SystemMonitor()

    print("Fields:", ",".join(FIELD_NAMES))
    try:
        while True:
            time.sleep(INTERVAL)
            message = ",".join(str(value) for value in monitor.read())
            arduino.write(f"{message}\n".encode())
            print(message)
    except KeyboardInterrupt:
        print("\nStopped.")
    except serial.SerialException as error:
        print(f"\nSerial connection lost: {error}")
    finally:
        arduino.close()


if __name__ == "__main__":
    main()
