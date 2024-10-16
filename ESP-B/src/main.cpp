#include <ESP8266WiFi.h>
#include <espnow.h>
#include <U8G2lib.h>

// Define PinOut
#define OLED_SDA 14
#define OLED_SCL 12
#define OLED_RESET U8X8_PIN_NONE
#define STATE_BUTTON_PIN 2
#define SEND_BUTTON_PIN 13
#define GREEN_LED_PIN 5
#define RED_LED_PIN 4

// Constants
const unsigned long DISPLAY_TIMEOUT = 30000;
const long FLASH_INTERVAL = 500;
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long STATE_DISPLAY_TIME = 5000;
const unsigned long IDLE_TIMEOUT = 30000; // 30 seconds

// Variables
uint8_t currentState = 0;
int currentMenuItem = 0;
bool displayActive = false;
bool ledState = false;
unsigned long lastActivityTime = 0;
unsigned long lastFlashTime = 0;
unsigned long lastButtonPressTime = 0;

// OLED display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

// Menu items
const char* menuItems[] = {"Status", "Jetzt nicht", "Leise durch", "in Ordnung"};
const int numMenuItems = sizeof(menuItems) / sizeof(menuItems[0]);

// ESPNow variables
uint8_t broadcastAddress[] = {0xC4, 0xD8, 0xD5, 0x2B, 0x7C, 0x15};

void turnOffDisplay() {
  displayActive = false;
  u8g2.setPowerSave(1);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  system_update_cpu_freq(80);
  currentState = 0; // Reset the state when turning off the display
  currentMenuItem = 0; // Reset the menu item as well
  Serial.println("Display turned off and state reset to idle");
}

void checkIdleTimeout() {
  if (currentState != 0 && millis() - lastActivityTime > IDLE_TIMEOUT) {
    Serial.println("Idle timeout reached. Returning to idle state.");
    turnOffDisplay();
  }
}

void renderMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(40, 16);
  u8g2.print(menuItems[0]);

  for (int i = 1; i < numMenuItems; i++) {
    u8g2.setCursor(15, 25 + (i-1) * 15);
    u8g2.print(menuItems[i]);
  }

  if (currentMenuItem > 0) {
    u8g2.setCursor(0, 25 + (currentMenuItem-1) * 15);
    u8g2.print(" > ");
  }

  u8g2.sendBuffer();
}

void turnOnDisplay() {
  if (!displayActive) {
    displayActive = true;
    u8g2.setPowerSave(0);
    system_update_cpu_freq(160);
    renderMenu();
  }
}



void wakeUpSystem() {
  lastActivityTime = millis();
  if (!displayActive) {
    displayActive = true;
    u8g2.setPowerSave(0);
    system_update_cpu_freq(160);
    if (currentState == 0) {
      currentState = 5; // Set to menu state if coming from idle
      currentMenuItem = 1; // Start at the first menu item
    }
    renderMenu();
    Serial.println("System woken up");
  }
}

void flashLEDs() {
  unsigned long currentTime = millis();
  if (currentTime - lastFlashTime >= FLASH_INTERVAL) {
    lastFlashTime = currentTime;
    ledState = !ledState;
    digitalWrite(RED_LED_PIN, ledState);
    digitalWrite(GREEN_LED_PIN, !ledState);
  }
}

void updateLEDs() {
  switch (currentState) {
     case 0: // Idle
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      break;
    case 1: // Waiting
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
      break;
    case 2: // Active
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);
      break;
    case 4: // Call notification
      flashLEDs();
      break;
    case 5: // User is interacting
    default:
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      break;
  }
}

void handleStatePress() {
  lastActivityTime = millis();
  if (currentState == 0 || currentState == 4) {
    wakeUpSystem();
    currentState = 5;
    currentMenuItem = 1; // Start at the first menu item
    uint8_t stateToSend = (uint8_t)currentState;
    esp_now_send(broadcastAddress, &stateToSend, sizeof(uint8_t));
    Serial.println("Transitioned to state 5, sent to device A");
  } else if (currentState == 5) {
    currentMenuItem = (currentMenuItem % (numMenuItems - 1)) + 1;
  }
  renderMenu();
}

void handleSendPress() {
  lastActivityTime = millis();
  if (currentState == 5 && currentMenuItem >= 1 && currentMenuItem <= 3) {
    currentState = currentMenuItem;
    uint8_t stateToSend = (uint8_t)currentState;

    Serial.print("Attempting to send state: ");
    Serial.println(stateToSend);

    uint8_t result = esp_now_send(broadcastAddress, &stateToSend, sizeof(uint8_t));

    if (result == 0) {
      Serial.print("Sent state to A: ");
      Serial.println(currentState);
    } else {
      Serial.println("Failed to send state");
    }

    delay(1000);
    renderMenu();
  } else {
    Serial.println("Invalid state or menu item for sending");
  }
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  lastActivityTime = millis();
  if (len != 1) {
    Serial.println("Invalid message length");
    return;
  }

  uint8_t receivedState = incomingData[0];

  Serial.print("Received message: state = ");
  Serial.println(receivedState);

  if (receivedState == 4 && currentState == 0) {  // Call state and device is idle
    wakeUpSystem();
    currentState = 4;
    currentMenuItem = 0; // Reset menu item
    Serial.println("Call received. LEDs flashing.");
  } else if (receivedState >= 1 && receivedState <= 3) {
    currentState = receivedState;
    currentMenuItem = receivedState;
    Serial.print("State updated to: ");
    Serial.println(currentState);
  } else if (receivedState == 5) {  // Request for current state
    if (currentState != 0) {
      uint8_t stateToSend = (uint8_t)currentState;
      esp_now_send(broadcastAddress, &stateToSend, sizeof(uint8_t));
      Serial.print("Sent current state to A: ");
      Serial.println(currentState);
    }
  } else {
    Serial.println("Unexpected state received");
  }

  // Update OLED display based on current state
  renderMenu();

  // Update LEDs based on the new state
  updateLEDs();
}

void setup() {
  Serial.begin(74880);
  pinMode(STATE_BUTTON_PIN, INPUT);
  pinMode(SEND_BUTTON_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  u8g2.begin();

  // Boot sequence
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 32, "Booting...");
  u8g2.sendBuffer();

  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  delay(500);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, HIGH);
  delay(500);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);
  delay(500);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) {
    Serial.println("ESPNow initialization failed");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);
  system_update_cpu_freq(80);
  lastActivityTime = millis();
 
  Serial.println("Setup complete");

  displayActive = false;

  u8g2.setPowerSave(1);

  currentState = 0;

  currentMenuItem = 0;

  turnOffDisplay();

}

void loop() {
  if (currentState == 0) {  // Idle state
    ESP.deepSleep(0);  // Deep sleep until external interrupt (button press or ESP-NOW message)
  } else {
    unsigned long currentTime = millis();

    int stateButtonState = digitalRead(STATE_BUTTON_PIN);
    int sendButtonState = digitalRead(SEND_BUTTON_PIN);

    if (stateButtonState == LOW || sendButtonState == LOW) {
      if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
        lastButtonPressTime = currentTime;
        lastActivityTime = currentTime;
        wakeUpSystem();
        if (stateButtonState == LOW) {
          handleStatePress();
        }
        if (sendButtonState == LOW && currentState == 5) {
          handleSendPress();
        }
      }
    }

    checkIdleTimeout();

    if (displayActive) {
      updateLEDs();
      renderMenu();
    }

    delay(100);
  }
}
