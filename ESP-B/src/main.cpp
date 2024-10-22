//communicator_v2
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <U8G2lib.h>

// Define the MAC address of ESP-A
uint8_t broadcastAddress[] = {0xC4, 0xD8, 0xD5, 0x2B, 0x7C, 0x15};

#define OLED_SDA 14
#define OLED_SCL 12
#define OLED_RESET U8X8_PIN_NONE

#define STATE_BUTTON_PIN 2
#define SEND_BUTTON_PIN 13
#define GREEN_LED_PIN 5
#define RED_LED_PIN 4

#define MAX_RETRIES 3
bool isInCallState = false;

#define RETRY_DELAY 100 // milliseconds

const char* menuItems[] = {"Status", "Jetzt nicht", "Leise durch", "in Ordnung"};
const int numMenuItems = sizeof(menuItems) / sizeof(menuItems[0]);

const long FLASH_INTERVAL = 200;
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long POWER_SAVE_TIMEOUT = 30000; // 30 seconds
unsigned long currentTime = millis();


uint8_t currentState = 0;
int currentMenuItem = 0;
bool ledState = false;
unsigned long lastFlashTime = 0;
unsigned long lastButtonPressTime = 0;
unsigned long idleStartTime = 0;
bool isPowerSaving = false;

const unsigned long IDLE_TIMEOUT = 30000; // 30 seconds timeout for returning to idle


unsigned long lastActivityTime = 0;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);




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



void updateLEDs() {

  switch (currentState) {

    case 0: // Idle

      digitalWrite(RED_LED_PIN, LOW);

      digitalWrite(GREEN_LED_PIN, LOW);

      break;

    case 1: // State 1

      digitalWrite(RED_LED_PIN, HIGH);

      digitalWrite(GREEN_LED_PIN, LOW);

      break;

    case 2: // State 2

      digitalWrite(RED_LED_PIN, HIGH);

      digitalWrite(GREEN_LED_PIN, HIGH);

      break;

    case 3: // State 3

      digitalWrite(RED_LED_PIN, LOW);

      digitalWrite(GREEN_LED_PIN, HIGH);

      break;

    case 4: // Flashing (Call notification)


      if (currentTime - lastFlashTime >= FLASH_INTERVAL) {

        lastFlashTime = currentTime;

        ledState = !ledState;

        digitalWrite(RED_LED_PIN, !ledState);

        digitalWrite(GREEN_LED_PIN, ledState);

      }

      break;

    case 5: // State 5

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

    u8g2.clearDisplay(); // Clear the display buffer

    digitalWrite(GREEN_LED_PIN, LOW);

    digitalWrite(RED_LED_PIN, LOW);

    Serial.println("Entered power save mode");

    Serial.print("Time since last activity: ");

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

void updateActivityTime() {
  lastActivityTime = millis();
  idleStartTime = lastActivityTime;  // Update idle start time when there's activity
  if (isPowerSaving) {
    exitPowerSaveMode();
  }
}

void checkPowerSaving() {

  if (!isInCallState && currentState == 0 && !isPowerSaving && idleStartTime > 0) {

    if (millis() - idleStartTime >= POWER_SAVE_TIMEOUT) {

      enterPowerSaveMode();

    }

  }

}

void handleStatePress() {

  Serial.println("State button pressed");

  updateActivityTime();

  

  if (currentState == 4) {

    currentState = 5;

    currentMenuItem = 1; // Start at the first menu item

    isInCallState = false;

    Serial.println("Transitioned to state 5, menu interaction started");

  } else if (currentState == 5) {

    currentMenuItem = (currentMenuItem % (numMenuItems - 1)) + 1;

    Serial.print("Menu item changed to: ");

    Serial.println(currentMenuItem);

  }

  

  renderMenu();

}

void handleSendPress() {

  Serial.println("Send button pressed");

  updateActivityTime();

  

  if (currentState == 5 && currentMenuItem >= 1 && currentMenuItem <= 3) {

    uint8_t stateToSend = (uint8_t)currentMenuItem;

    Serial.print("Attempting to send state: ");

    Serial.println(stateToSend);

    

    for (int i = 0; i < MAX_RETRIES; i++) {

      uint8_t result = esp_now_send(broadcastAddress, &stateToSend, sizeof(uint8_t));

      if (result == 0) {

        Serial.print("Sent state to A: ");

        Serial.println(stateToSend);

        currentState = stateToSend;  // Update the current state

        Serial.print("Updated current state to: ");

        Serial.println(currentState);

        break;

      } else {

        Serial.println("Failed to send state, retrying...");

        delay(RETRY_DELAY);

      }

    }

    

    if (currentState != stateToSend) {

      Serial.println("Failed to send state after maximum retries");

    }

  } else {

    Serial.println("Invalid state or menu item for sending");

  }

  

  renderMenu();

}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {

  Serial.println("Data received");

  updateActivityTime();

  

  if (len == 1) {

    uint8_t receivedState = incomingData[0];

    Serial.print("Received message: state = ");

    Serial.println(receivedState);

    if (receivedState == 4 && (currentState == 0 || currentState == 5)) {

      currentState = 4;

      currentMenuItem = 0; // Reset menu item

      isInCallState = true;

      Serial.println("Call received. LEDs flashing.");

    } else if (receivedState >= 1 && receivedState <= 3) {

      currentState = receivedState;

      currentMenuItem = receivedState;

      isInCallState = false;

      Serial.print("State updated to: ");

      Serial.println(currentState);

    }

  }

  

  updateLEDs();

  renderMenu();

}

void setup() {
  Serial.begin(74880);
  pinMode(STATE_BUTTON_PIN, INPUT);
  pinMode(SEND_BUTTON_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  u8g2.begin();
  
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return; // or handle the error
  }
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("ESP-NOW initialized successfully");
  
  Serial.println("Setup complete. ESP-B ready.");
  currentState = 0;
  currentMenuItem = 0;
  renderMenu();
  idleStartTime = millis(); // Initialize idle timer
}

void enterIdleState() {
  if (currentState != 0) {
    currentState = 0;
    currentMenuItem = 0;
    isInCallState = false;
    updateLEDs();
    renderMenu();
    Serial.println("Entered idle state");
    idleStartTime = millis();  // Start counting idle time
  }
}


void loop() {
  currentTime = millis();

  int stateButtonState = digitalRead(STATE_BUTTON_PIN);
  int sendButtonState = digitalRead(SEND_BUTTON_PIN);

  if (stateButtonState == LOW || sendButtonState == LOW) {
    if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
      lastButtonPressTime = currentTime;
      updateActivityTime();

      if (stateButtonState == LOW) {
        handleStatePress();
      }

      if (sendButtonState == LOW && currentState == 5) {
        handleSendPress();
      }
    }
  }

  if (!isInCallState && currentState == 0 && currentTime - lastActivityTime > IDLE_TIMEOUT) {
    enterIdleState();
  }

  checkPowerSaving();  // Add this line to check and enter power-saving mode if needed
  updateLEDs();

  if (!isPowerSaving) {
    renderMenu();
  }

  delay(10);
}
