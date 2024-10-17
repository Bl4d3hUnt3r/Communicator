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

const long FLASH_INTERVAL = 500;

const unsigned long DEBOUNCE_DELAY = 50;

const unsigned long POWER_SAVE_TIMEOUT = 30000; // 30 seconds


// Variables

uint8_t currentState = 0;

int currentMenuItem = 0;

bool ledState = false;

unsigned long lastFlashTime = 0;

unsigned long lastButtonPressTime = 0;

unsigned long idleStartTime = 0;

bool isPowerSaving = false;


// OLED display

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);


// Menu items

const char* menuItems[] = {"Status", "Jetzt nicht", "Leise durch", "in Ordnung"};

const int numMenuItems = sizeof(menuItems) / sizeof(menuItems[0]);


// ESPNow variables

uint8_t broadcastAddress[] = {0xC4, 0xD8, 0xD5, 0x2B, 0x7C, 0x15};


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


void enterPowerSaveMode() {

  if (!isPowerSaving) {

    isPowerSaving = true;

    u8g2.setPowerSave(1); // Turn off OLED display

    digitalWrite(GREEN_LED_PIN, LOW);

    digitalWrite(RED_LED_PIN, LOW);

    Serial.println("Entered power save mode");

    Serial.print ("Time since last activity: ");

    Serial.println(millis() - idleStartTime);

  }

}


void exitPowerSaveMode() {

  if (isPowerSaving) {

    isPowerSaving = false;

    u8g2.setPowerSave(0); // Turn on OLED display

    renderMenu(); // Refresh the display

    Serial.println("Exited power save mode");

    idleStartTime = millis(); // Reset idle timer when exiting power save mode

  }

}


void checkPowerSaving() {

  unsigned long currentTime = millis();

  if (currentState == 0 && !isPowerSaving && (currentTime - idleStartTime >= POWER_SAVE_TIMEOUT)) {

    enterPowerSaveMode();

  }

}


void handleStatePress() {

  Serial.println("State button pressed");


  if (currentState == 0 || currentState == 4) {

    currentState = 5;

    currentMenuItem = 1; // Start at the first menu item

    uint8_t stateToSend = (uint8_t)currentState;

    esp_now_send(broadcastAddress, &stateToSend, sizeof(uint8_t));

    Serial.println("Transitioned to state 5, sent to device A");

    idleStartTime = millis(); // Reset idle timer only when transitioning from idle or call state

  } else if (currentState == 5) {

    currentMenuItem = (currentMenuItem % (numMenuItems - 1)) + 1;

    Serial.print("Menu item changed to: ");

    Serial.println(currentMenuItem);

  }


  renderMenu();

}


void handleSendPress() {

  Serial.println("Send button pressed");


  if (currentState == 5 && currentMenuItem >= 1 && currentMenuItem <= 3) {

    uint8_t stateToSend = (uint8_t)currentMenuItem;


    Serial.print("Attempting to send state: ");

    Serial.println(stateToSend);


    uint8_t result = esp_now_send(broadcastAddress, &stateToSend, sizeof(uint8_t));


    if (result == 0) {

      Serial.print("Sent state to A: ");

      Serial.println(stateToSend);


      // Reset to idle state after sending

      currentState = 0;

      currentMenuItem = 0;

      idleStartTime = millis(); // Reset idle timer

      isPowerSaving = false; // Ensure we're not in power-saving mode

      Serial.println("Reset to idle state (0)");

    } else {

      Serial.println("Failed to send state");

    }

  } else {

    Serial.println("Invalid state or menu item for sending");

  }


  renderMenu();

}


void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {

  Serial.println("Data received");

  

  if (isPowerSaving) {

    exitPowerSaveMode();

  }


  if (len != 1) {

    Serial.println("Invalid message length");

    return;

  }


  uint8_t receivedState = incomingData[0];


  Serial.print("Received message: state = ");

  Serial.println(receivedState);


  if (receivedState == 4 && currentState == 0) {  // Call state and device is idle

    currentState = 4;

    currentMenuItem = 0; // Reset menu item

    idleStartTime = millis(); // Reset idle timer

    Serial.println("Call received. LEDs flashing.");

  } else if (receivedState >= 1 && receivedState <= 3) {

    currentState = receivedState;

    currentMenuItem = receivedState;

    idleStartTime = millis(); // Reset idle timer

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
  Serial.begin(74880 );
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
  Serial.println("Setup complete");

  currentState = 0;
  currentMenuItem = 0;
  renderMenu();
  idleStartTime = millis(); // Initialize idle timer
}


void loop() {
  unsigned long currentTime = millis();
  static unsigned long lastLoopTime = 0;

  if (currentTime - lastLoopTime >= 1000) {
    lastLoopTime = currentTime;
    Serial.println("Loop cycle");
    if (currentState == 0) {
      Serial.print("Current state: ");
      Serial.println(currentState);
      Serial.print("Current menu item: ");
      Serial.println(currentMenuItem);
      Serial.print("Power saving: ");
      Serial.println(isPowerSaving);
      Serial.print("Time since idle: ");
      Serial.println(currentTime - idleStartTime);
      Serial.println();
    }
    checkPowerSaving();
  }


  // Check for power saving less frequently
  if (currentTime - lastPowerSaveCheckTime >= 1000) {
    lastPowerSaveCheckTime = currentTime;
    if (currentState == 0 && !isPowerSaving && (currentTime - idleStartTime >= POWER_SAVE_TIMEOUT)) {
      enterPowerSaveMode();
    }
  }

  int stateButtonState = digitalRead(STATE_BUTTON_PIN);
  int sendButtonState = digitalRead(SEND_BUTTON_PIN);

  if (stateButtonState == LOW || sendButtonState == LOW) {
    if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
      lastButtonPressTime = currentTime;

      if (isPowerSaving) {
        exitPowerSaveMode();
      } else {
        idleStartTime = currentTime; // Reset idle timer on button press
      }

      if (stateButtonState == LOW) {
        handleStatePress();
      }

      if (sendButtonState == LOW && currentState == 5) {
        handleSendPress();
      }
    }
  }

  updateLEDs();

  if (!isPowerSaving) {
    renderMenu();
  }

  delay(10);
}
