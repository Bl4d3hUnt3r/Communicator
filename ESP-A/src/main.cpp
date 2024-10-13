#include <ESP8266WiFi.h>
#include <espnow.h>

// Define the MAC address of ESP-B
uint8_t broadcastAddress[] = {0x98, 0xF4, 0xAB, 0xBC, 0xCE, 0x25};

// Define a struct to send complex data
typedef struct {
  bool wakeUp;
  int state;
} MessageData;

MessageData messageData;

const int CALL_BUTTON_PIN = 2;  // GPIO 2 (D4)

const int DEBOUNCE_DELAY = 50;  // Debounce delay in milliseconds

int callButtonState = HIGH;  // Initial state of call button
int callButtonLastState = HIGH;  // Last state of call button
unsigned long callButtonLastDebounceTime = 0;  // Last debounce time of call button

const int LED_Red = 14;  // GPIO 14 (D5)
const int LED_Yellow = 12;  // GPIO 12 (D6)
const int LED_Green = 13;  // GPIO 13 (D7)

void onReceive(uint8_t* mac, uint8_t* incomingData, uint8_t len) {
  // Handle incoming data
  MessageData incomingMessageData;
  memcpy(&incomingMessageData, incomingData, len);

  messageData.wakeUp = incomingMessageData.wakeUp;
  messageData.state = incomingMessageData.state;
}

void takeAction(int state) {
  switch (state) {
    case 0: // Running light while waiting for interaction
      // Simple running light pattern: 0.7 seconds each at 50% brightness
      while (true) {
        analogWrite(LED_Red, 128);
        delay(700);
        analogWrite(LED_Red, 0);
        analogWrite(LED_Yellow, 128);
        delay(700);
        analogWrite(LED_Yellow, 0);
        analogWrite(LED_Green, 128);
        delay(700);
        analogWrite(LED_Green, 0);
      }
      break;
    case 1: // Solid Red
      digitalWrite(LED_Red, HIGH);
      digitalWrite(LED_Yellow, LOW);
      digitalWrite(LED_Green, LOW);
      break;
    case 2: // Solid Yellow
      digitalWrite(LED_Red, LOW);
      digitalWrite(LED_Yellow, HIGH);
      digitalWrite(LED_Green, LOW);
      break;
    case 3: // Solid Green
      digitalWrite(LED_Red, LOW);
      digitalWrite(LED_Yellow, LOW);
      digitalWrite(LED_Green, HIGH);
      break;
    default:
      // Handle unexpected state values
      break;
  }
}


void setup() {
  Serial.begin(74880);

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Initialize ESPNow
  if (esp_now_init() != 0) {
    Serial.println("ESPNow init failed");
    return;
  }

  // Register the sender and receiver callback functions
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  esp_now_register_recv_cb(onReceive);

  pinMode(CALL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_Red, OUTPUT);
  pinMode(LED_Yellow, OUTPUT);
  pinMode(LED_Green, OUTPUT);
}

void loop() {
  int callButtonReading = digitalRead(CALL_BUTTON_PIN);

  // Debounce the button readings
  if (callButtonReading != callButtonLastState) {
    callButtonLastDebounceTime = millis();
  }

  if (millis() - callButtonLastDebounceTime > DEBOUNCE_DELAY) {
    callButtonState = callButtonReading;
  }

  callButtonLastState = callButtonReading;

  // Update the LED indicators
  if (callButtonState == LOW) {
    // Call button is pressed
    digitalWrite(LED_Green, LOW);
    digitalWrite(LED_Yellow, HIGH);
  } else {
    // Call button is not pressed
    digitalWrite(LED_Green, HIGH);
    digitalWrite(LED_Yellow, LOW);
  }

 if (callButtonState == LOW) {

    messageData.wakeUp = true;

    messageData.state = 0; // Not used in this case

    esp_now_send(broadcastAddress, (uint8_t*) &messageData, sizeof(messageData));

  }
  
  // Handle wake-up sequence and LED lighting sequence
  if (messageData.wakeUp) {
    takeAction(messageData.state);
  }

  delay(100);
}

