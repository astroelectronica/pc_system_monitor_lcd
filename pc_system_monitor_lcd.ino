/*
  PC system monitor on a 16x2 I2C LCD
  Board: Arduino Uno R3
  Library: LiquidCrystal_I2C

  LCD I2C wiring: GND -> GND, VCC -> 5V, SDA -> A4, SCL -> A5
  Optional push button: one pin -> D2, the other pin -> GND
  (each press jumps to the next page)
  If nothing is displayed, try the I2C address 0x3F instead of 0x27.

  The PC sends one line per second over the serial port with 23
  comma-separated integers, in the exact order of the Field enum below.
  A value of -1 means "not available on this PC".
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte BUTTON_PIN = 2;
const unsigned long PAGE_INTERVAL_MS = 4000;  // time each page stays on screen
const unsigned long TIMEOUT_MS = 3000;        // no data for 3 s -> show warning
const unsigned long DEBOUNCE_MS = 50;

// Order of the values received from the PC (must match the Python script)
enum Field {
  CPU_PERCENT,      // tenths of %
  CPU_FREQ_MHZ,     // MHz
  CPU_CORES,        // logical cores
  CPU_TEMP,         // tenths of degrees Celsius
  FAN_RPM,          // RPM
  LOAD_AVG,         // hundredths (1 minute load average)
  RAM_PERCENT,      // tenths of %
  RAM_USED_MB,      // MB
  RAM_TOTAL_MB,     // MB
  SWAP_PERCENT,     // tenths of %
  PROCESS_COUNT,    // number of processes
  DISK_PERCENT,     // tenths of %
  DISK_USED_GB,     // tenths of GB
  DISK_TOTAL_GB,    // tenths of GB
  DISK_READ_BPS,    // bytes per second
  DISK_WRITE_BPS,   // bytes per second
  NET_DOWN_BPS,     // bytes per second
  NET_UP_BPS,       // bytes per second
  NET_RECV_MB,      // MB received since boot
  NET_SENT_MB,      // MB sent since boot
  BATTERY_PERCENT,  // %
  BATTERY_PLUGGED,  // 1 = plugged, 0 = on battery
  UPTIME_SECONDS,   // seconds since boot
  FIELD_COUNT
};

// Pages shown on the LCD
enum Page {
  PAGE_CPU,
  PAGE_CPU_INFO,
  PAGE_TEMPERATURE,
  PAGE_RAM,
  PAGE_SWAP_PROCESSES,
  PAGE_DISK,
  PAGE_DISK_IO,
  PAGE_NETWORK,
  PAGE_NETWORK_TOTAL,
  PAGE_BATTERY,
  PAGE_UPTIME,
  PAGE_COUNT
};

// Custom characters: down arrow and up arrow
byte downArrow[8] = {
  B00100, B00100, B00100, B00100,
  B10101, B01110, B00100, B00000
};
byte upArrow[8] = {
  B00100, B01110, B10101, B00100,
  B00100, B00100, B00100, B00000
};

long values[FIELD_COUNT];
byte currentPage = 0;
unsigned long lastPageChange = 0;
unsigned long lastDataTime = 0;
bool noConnection = true;

char inputBuffer[192];
byte bufferPos = 0;
bool bufferOverflow = false;

bool buttonState = HIGH;
bool lastButtonReading = HIGH;
unsigned long lastDebounceTime = 0;

// ---------- Formatting helpers ----------

// Prints text on a row, padded with spaces to clear old characters
void printRow(byte row, const char *text) {
  char line[17];
  snprintf(line, sizeof(line), "%-16s", text);
  lcd.setCursor(0, row);
  lcd.print(line);
}

// Prints a custom icon followed by text on a row
void printRowWithIcon(byte row, byte icon, const char *text) {
  char line[15];
  snprintf(line, sizeof(line), "%-14s", text);
  lcd.setCursor(0, row);
  lcd.write(icon);
  lcd.print(' ');
  lcd.print(line);
}

// Value in tenths -> "12.3"
void formatTenths(long value, char *out, byte size) {
  snprintf(out, size, "%ld.%ld", value / 10, value % 10);
}

// Bytes per second -> "B/s", "KB/s" or "MB/s"
void formatSpeed(long bytesPerSecond, char *out, byte size) {
  if (bytesPerSecond < 0) {
    snprintf(out, size, "N/A");
    return;
  }
  unsigned long bytes = bytesPerSecond;
  if (bytes >= 1048576UL) {
    snprintf(out, size, "%lu.%02lu MB/s", bytes / 1048576UL, (bytes % 1048576UL) * 100UL / 1048576UL);
  } else if (bytes >= 1024UL) {
    snprintf(out, size, "%lu.%lu KB/s", bytes / 1024UL, (bytes % 1024UL) * 10UL / 1024UL);
  } else {
    snprintf(out, size, "%lu B/s", bytes);
  }
}

// Megabytes -> "MB" or "GB"
void formatMegabytes(long megabytes, char *out, byte size) {
  if (megabytes < 0) {
    snprintf(out, size, "N/A");
  } else if (megabytes >= 1024) {
    snprintf(out, size, "%ld.%02ld GB", megabytes / 1024, (megabytes % 1024) * 100 / 1024);
  } else {
    snprintf(out, size, "%ld MB", megabytes);
  }
}

// ---------- Page drawing ----------

void drawPage() {
  char text[17];
  char partA[14];
  char partB[14];

  switch (currentPage) {
    case PAGE_CPU:
      formatTenths(values[CPU_PERCENT], partA, sizeof(partA));
      snprintf(text, sizeof(text), "CPU: %s%%", partA);
      printRow(0, text);
      if (values[CPU_FREQ_MHZ] < 0) {
        snprintf(text, sizeof(text), "Freq: N/A");
      } else {
        snprintf(text, sizeof(text), "Freq: %ld MHz", values[CPU_FREQ_MHZ]);
      }
      printRow(1, text);
      break;

    case PAGE_CPU_INFO:
      snprintf(text, sizeof(text), "CPU Cores: %ld", values[CPU_CORES]);
      printRow(0, text);
      if (values[LOAD_AVG] < 0) {
        snprintf(text, sizeof(text), "Load 1m: N/A");
      } else {
        snprintf(text, sizeof(text), "Load 1m: %ld.%02ld", values[LOAD_AVG] / 100, values[LOAD_AVG] % 100);
      }
      printRow(1, text);
      break;

    case PAGE_TEMPERATURE:
      if (values[CPU_TEMP] < 0) {
        snprintf(text, sizeof(text), "CPU Temp: N/A");
      } else {
        formatTenths(values[CPU_TEMP], partA, sizeof(partA));
        snprintf(text, sizeof(text), "CPU Temp: %s%cC", partA, 223);  // 223 = degree symbol
      }
      printRow(0, text);
      if (values[FAN_RPM] < 0) {
        snprintf(text, sizeof(text), "Fan: N/A");
      } else {
        snprintf(text, sizeof(text), "Fan: %ld RPM", values[FAN_RPM]);
      }
      printRow(1, text);
      break;

    case PAGE_RAM:
      formatTenths(values[RAM_PERCENT], partA, sizeof(partA));
      snprintf(text, sizeof(text), "RAM: %s%%", partA);
      printRow(0, text);
      snprintf(text, sizeof(text), "%ld/%ld MB", values[RAM_USED_MB], values[RAM_TOTAL_MB]);
      printRow(1, text);
      break;

    case PAGE_SWAP_PROCESSES:
      formatTenths(values[SWAP_PERCENT], partA, sizeof(partA));
      snprintf(text, sizeof(text), "Swap: %s%%", partA);
      printRow(0, text);
      snprintf(text, sizeof(text), "Processes: %ld", values[PROCESS_COUNT]);
      printRow(1, text);
      break;

    case PAGE_DISK:
      formatTenths(values[DISK_PERCENT], partA, sizeof(partA));
      snprintf(text, sizeof(text), "Disk: %s%%", partA);
      printRow(0, text);
      formatTenths(values[DISK_USED_GB], partA, sizeof(partA));
      formatTenths(values[DISK_TOTAL_GB], partB, sizeof(partB));
      snprintf(text, sizeof(text), "%s/%s GB", partA, partB);
      printRow(1, text);
      break;

    case PAGE_DISK_IO:
      formatSpeed(values[DISK_READ_BPS], partA, sizeof(partA));
      snprintf(text, sizeof(text), "Rd: %s", partA);
      printRow(0, text);
      formatSpeed(values[DISK_WRITE_BPS], partA, sizeof(partA));
      snprintf(text, sizeof(text), "Wr: %s", partA);
      printRow(1, text);
      break;

    case PAGE_NETWORK:
      formatSpeed(values[NET_DOWN_BPS], partA, sizeof(partA));
      printRowWithIcon(0, 0, partA);  // icon 0 = down arrow
      formatSpeed(values[NET_UP_BPS], partA, sizeof(partA));
      printRowWithIcon(1, 1, partA);  // icon 1 = up arrow
      break;

    case PAGE_NETWORK_TOTAL:
      formatMegabytes(values[NET_RECV_MB], partA, sizeof(partA));
      snprintf(text, sizeof(text), "Recv: %s", partA);
      printRow(0, text);
      formatMegabytes(values[NET_SENT_MB], partA, sizeof(partA));
      snprintf(text, sizeof(text), "Sent: %s", partA);
      printRow(1, text);
      break;

    case PAGE_BATTERY:
      if (values[BATTERY_PERCENT] < 0) {
        snprintf(text, sizeof(text), "Battery: N/A");
      } else {
        snprintf(text, sizeof(text), "Battery: %ld%%", values[BATTERY_PERCENT]);
      }
      printRow(0, text);
      if (values[BATTERY_PLUGGED] < 0) {
        snprintf(text, sizeof(text), "Plugged: N/A");
      } else {
        snprintf(text, sizeof(text), "Plugged: %s", values[BATTERY_PLUGGED] ? "Yes" : "No");
      }
      printRow(1, text);
      break;

    case PAGE_UPTIME: {
      unsigned long seconds = values[UPTIME_SECONDS];
      printRow(0, "Uptime:");
      snprintf(text, sizeof(text), "%lud %02luh %02lum %02lus",
               seconds / 86400UL, (seconds % 86400UL) / 3600UL,
               (seconds % 3600UL) / 60UL, seconds % 60UL);
      printRow(1, text);
      break;
    }
  }
}

void nextPage() {
  currentPage = (currentPage + 1) % PAGE_COUNT;
  lastPageChange = millis();
}

// ---------- Serial input ----------

// Parses a received line and updates the display
void processLine() {
  byte count = 0;
  char *token = strtok(inputBuffer, ",");
  while (token != NULL && count < FIELD_COUNT) {
    values[count++] = atol(token);
    token = strtok(NULL, ",");
  }
  // Reject lines with too few or too many values
  if (count != FIELD_COUNT || token != NULL) return;

  if (noConnection) {
    lcd.clear();
    noConnection = false;
    lastPageChange = millis();
  }
  if (millis() - lastPageChange >= PAGE_INTERVAL_MS) {
    nextPage();
  }
  drawPage();
  lastDataTime = millis();
}

void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      if (!bufferOverflow) {
        inputBuffer[bufferPos] = '\0';
        processLine();
      }
      bufferPos = 0;
      bufferOverflow = false;
    } else if (c != '\r') {
      if (bufferPos < sizeof(inputBuffer) - 1) {
        inputBuffer[bufferPos++] = c;
      } else {
        bufferOverflow = true;  // line too long: discard it
      }
    }
  }
}

// ---------- Button (debounced) ----------

void readButton() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }
  if (millis() - lastDebounceTime > DEBOUNCE_MS && reading != buttonState) {
    buttonState = reading;
    if (buttonState == LOW && !noConnection) {
      nextPage();
      drawPage();
    }
  }
  lastButtonReading = reading;
}

// ---------- Arduino entry points ----------

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  lcd.begin();  // if your library version has no begin() without arguments, use lcd.begin(16, 2) or lcd.init()
  lcd.backlight();
  lcd.createChar(0, downArrow);
  lcd.createChar(1, upArrow);
  lcd.setCursor(0, 0);
  lcd.print("Waiting for PC..");
}

void loop() {
  readSerial();
  readButton();

  // Show a warning if the PC stopped sending data
  if (!noConnection && millis() - lastDataTime > TIMEOUT_MS) {
    noConnection = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No data from PC");
  }
}
