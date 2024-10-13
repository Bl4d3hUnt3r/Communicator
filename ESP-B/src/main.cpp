#include <ESP8266WiFi.h>
#include <espnow.h>
#include <U8G2lib.h>

// Define PinOut
#define OLED_SDA 14                  // D6
#define OLED_SCL 12                  // D5
#define OLED_RESET     U8X8_PIN_NONE // Reset pin
#define STATE_BUTTON_PIN 2           // GPIO 2 (D4)
#define SEND_BUTTON_PIN 13           // GPIO 13 (D7)
#define GREEN_LED_PIN 5              // GPIO 5 (D1)
#define RED_LED_PIN 4                // GPIO 4 (D2)

// Debouncing variables

const unsigned long DEBOUNCE_DELAY = 50; // 50ms debounce delay
int stateButtonCurrentState ;
unsigned long lastStateDebounceTime = 0;
int sendButtonCurrentState;
unsigned long lastSendDebounceTime = 0;

// Define struct for sending complex data
typedef struct {
  bool wakeUp;
  int state;
} MessageData;

// Define global variables
MessageData messageData;

// OLED display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

// button variables
int stateButtonState = HIGH;         // Initial state of state button
int stateButtonLastState = HIGH;     // Last state of state button
unsigned long stateButtonLastDebounceTime = 0;  // Last debounce time of state button
bool userIsInteracting = false;
int sendButtonState = HIGH;         // Initial state of send button
int sendButtonLastState = HIGH;     // Last state of send button
unsigned long sendButtonLastDebounceTime = 0;  // Last debounce time of send button

// Menu variables
int currentState = 0;               // Current state of the menu
int currentMenuItem = 0;            // Current menu item (initialize to 1)
bool menuActive = false;

// Menu items
const char* menuItems[] = {
  "Status",
  "Jetzt nicht",
  "Leise durch",
  "in Ordnung"
};

const int numMenuItems = sizeof(menuItems) / sizeof(menuItems[0]);

// ESPNow variables
uint8_t broadcastAddress[] = {0xC4, 0xD8, 0xD5, 0x2B, 0x7C, 0x15};

// Function to debounce the state button

bool debounceButton(int buttonPin, int& lastButtonState, unsigned long& lastDebounceTime) {
  int buttonState = digitalRead(buttonPin);
  unsigned long currentTime = millis();

  Serial.print("Button state: ");
  Serial.println(buttonState);

  if (buttonState != lastButtonState) {
    lastDebounceTime = currentTime;
    lastButtonState = buttonState;
    Serial.print("Last button state updated: ");
    Serial.println(lastButtonState);
  }

  if (currentTime - lastDebounceTime > DEBOUNCE_DELAY) {
    if (buttonState != lastButtonState) {
      lastButtonState = buttonState;
      Serial.println("Button press debounced");
      return true;
    }
  }

  return false;
}

bool debounceStateButton() {
  return debounceButton(STATE_BUTTON_PIN, stateButtonLastState, stateButtonLastDebounceTime);
}

bool debounceSendButton() {
  return debounceButton(SEND_BUTTON_PIN, sendButtonLastState, sendButtonLastDebounceTime);
}

void flashLEDsForCallNotification() {

  unsigned long lastFlashTime = 0;

  while (!userIsInteracting) {

    unsigned long currentTime = millis();

    if (currentTime - lastFlashTime >= 500) {

      lastFlashTime = currentTime;

      digitalWrite(RED_LED_PIN, HIGH);

      digitalWrite(GREEN_LED_PIN, LOW);

      delay(500);

      digitalWrite(RED_LED_PIN, LOW);

      digitalWrite(GREEN_LED_PIN, HIGH);

    }

  }

}

void handleSendPress() {
  // Send the current state using ESPNow
  messageData.wakeUp = true;
  messageData.state = currentState;

  // Debugging: Output the data to be sent over ESPNow to the serial monitor
  Serial.print("Sending ESPNow message: ");
  Serial.print("WakeUp: ");
  Serial.print(messageData.wakeUp);
  Serial.print(", State: ");
  Serial.println(messageData.state);

  esp_now_send(broadcastAddress, (uint8_t*) &messageData, sizeof(messageData));
}

void renderMenu() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.setCursor(40, 16);

  u8g2.print(menuItems[0 ]);

  u8g2.setCursor(40, 32);

  u8g2.print(menuItems[1]);

  u8g2.setCursor(40, 48);

  u8g2.print(menuItems[2]);

  u8g2.setCursor(40, 64);

  u8g2.print(menuItems[3]);

  u8g2.sendBuffer();

}

void OnDataRecv(uint8_t* mac, uint8_t* incomingData, uint8_t len) {
  MessageData incomingMessageData;

  if (len != sizeof(MessageData)) {
    Serial.println("Invalid message length");
    return;
  }

  memcpy(&incomingMessageData, incomingData, len);

  if (incomingMessageData.wakeUp) {
    userIsInteracting = false; // Reset interaction flag

    flashLEDsForCallNotification(); // Flash LEDs until user interacts

    // Move the button checking and state updating code here
    while (!userIsInteracting) {
      // Read the state of the state button
      stateButtonCurrentState = digitalRead(STATE_BUTTON_PIN);

      // Debounce the state button
      if (debounceStateButton()) {
        userIsInteracting = true; // Set interaction flag
        menuActive = true;

        // Update the state when the state button is pressed
        if (menuActive) {
          if (currentMenuItem == 0) {
            currentMenuItem = 1; // Initialize to 1 on first press
          } else if (currentMenuItem == 1) {
            currentMenuItem = 2;
          } else if (currentMenuItem == 2) {
            currentMenuItem = 3;
          } else if (currentMenuItem == 3) {
            currentMenuItem = 1;
          }

          // Update the currentState variable immediately
          switch (currentMenuItem) {
            case 1:
              currentState = 1;
              break;
            case 2:
              currentState = 2;
              break;
            case 3:
              currentState = 3;
              break;
            default:
              currentState = 0;
              break;
          }
        }
      }

      // Read the state of the send button
      sendButtonCurrentState = digitalRead(SEND_BUTTON_PIN);

      // Debounce the send button
      if (debounceSendButton()) {
        // Send the current state when the send button is pressed
        handleSendPress();
        menuActive = false; // Reset menuActive to false on send button press
      }

      // Update the OLED display
      renderMenu();

      delay(100); // Add a delay to avoid flooding the serial output
    }
  }
}

void setup() {

 Serial.begin(74880); // Initialize serial monitor at 115200 baud rate

  while (!Serial) {

    ; // Wait for serial monitor to be available

  }
  pinMode(STATE_BUTTON_PIN, INPUT);

  pinMode(SEND_BUTTON_PIN, INPUT);

  pinMode(GREEN_LED_PIN, OUTPUT);

  pinMode(RED_LED_PIN, OUTPUT);

  u8g2.begin();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != 0) {
    Serial.println("ESPNow initialization failed");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

  esp_now_register_recv_cb(OnDataRecv);

}

void loop() {

  // Check for incoming messages
  // No need to do anything here, the onReceive function will handle incoming messages
  delay(100); // Add a delay to avoid flooding the serial output

}

