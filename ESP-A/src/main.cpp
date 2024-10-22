//communicator_v2
#include <ESP8266WiFi.h>
#include <espnow.h>

// Define the MAC address of ESP-B
uint8_t broadcastAddress[] = {0x98, 0xF4, 0xAB, 0xBC, 0xCE, 0x25};

const int CALL_BUTTON_PIN = 2;  // GPIO 2 (D4)
const int LED_Red = 14;  // GPIO 14 (D5)
const int LED_Yellow = 12;  // GPIO 12 (D6)
const int LED_Green = 13;  // GPIO 13 (D7)

const int DEBOUNCE_DELAY = 50;  // Debounce delay in milliseconds
const unsigned long IDLE_TIMEOUT = 30000; // 30 seconds timeout for returning to idle

uint8_t state = 0;
int callButtonState = HIGH;
int callButtonLastState = HIGH;
unsigned long callButtonLastDebounceTime = 0;
unsigned long lastStateChangeTime = 0;
unsigned long lastEffectUpdateTime = 0;
int currentLED = 0;
int direction = 1;

#define MAX_RETRIES 3

#define RETRY_DELAY 100 // milliseconds
void sendWithRetry(uint8_t stateToSend) {

  for (int i = 0; i < MAX_RETRIES; i++) {

    uint8_t result = esp_now_send(broadcastAddress, &stateToSend, 1);

    if (result == 0) {

      Serial.print("Successfully sent state to ESP-B: ");

      Serial.println(stateToSend);

      return;

    }

    Serial.println("Failed to send state, retrying...");

    delay(RETRY_DELAY);

  }

  Serial.println("Failed to send state after maximum retries");

}


void updateKnightRiderEffect() {
  if (millis() - lastEffectUpdateTime >= 100) { // Update every 100ms
    digitalWrite(LED_Red, currentLED == 0 ? HIGH : LOW);
    digitalWrite(LED_Yellow, currentLED == 1 ? HIGH : LOW);
    digitalWrite(LED_Green, currentLED == 2 ? HIGH : LOW);
    
    currentLED += direction;
    
    if (currentLED == 2 || currentLED == 0) {
      direction *= -1;
    }
    
    lastEffectUpdateTime = millis();
  }
}

void updateLEDs() {
  switch (state) {
    case 0: // Idle (all LEDs off)
      digitalWrite(LED_Red, LOW);
      digitalWrite(LED_Yellow, LOW);
      digitalWrite(LED_Green, LOW);
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
    case 4: // Call state (Knight Rider effect)
      updateKnightRiderEffect();
      break;
  }
}

void setState(uint8_t newState) {

  if (newState != state) {

    state = newState;

    lastStateChangeTime = millis();

    Serial.print("State changed to: ");

    Serial.println(state);

    sendWithRetry(state);

  }

}

void OnDataRecv(uint8_t* mac, uint8_t* incomingData, uint8_t len) {
  if (len == 1) {
    uint8_t receivedState = incomingData[0];
    Serial.print("Received state from ESP-B: ");
    Serial.println(receivedState);
    if (receivedState >= 1 && receivedState <= 3) {
      setState(receivedState);
    }
  }
}

void setup() {
  Serial.begin(74880);
  
  pinMode(CALL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_Red, OUTPUT);
  pinMode(LED_Yellow, OUTPUT);
  pinMode(LED_Green, OUTPUT);
  
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return; // or handle the error
  }
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("ESP-NOW initialized successfully");
  
  Serial.println("Setup complete. ESP-A ready.");
}

void loop() {
  int callButtonReading = digitalRead(CALL_BUTTON_PIN);
  
  if (callButtonReading != callButtonLastState) {
    callButtonLastDebounceTime = millis();
  }
  
  if ((millis() - callButtonLastDebounceTime) > DEBOUNCE_DELAY) {
    if (callButtonState != callButtonReading) {
      callButtonState = callButtonReading;
      
      if (callButtonState == LOW) {
        Serial.println("Call button pressed, sending wake-up message to ESP-B");
        setState(4);  // This will send state 4 to ESP-B
      }
    }
  }
  
  callButtonLastState = callButtonReading;
  
  updateLEDs();
  
  if (millis() - lastStateChangeTime > IDLE_TIMEOUT && state != 0) {
    state = 0;  // Transition to idle state locally
    updateLEDs();  // Update LEDs to reflect idle state
    Serial.println("Timeout reached, entered idle state");
  }
  
  delay(10);
}
