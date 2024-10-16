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
const unsigned long LED_TIMEOUT = 30000; // 30 seconds

uint8_t state = 0;
int callButtonState = HIGH;
int callButtonLastState = HIGH;
unsigned long callButtonLastDebounceTime = 0;

unsigned long lastStateChangeTime = 0;
unsigned long lastEffectUpdateTime = 0;
int currentLED = 0;
int direction = 1;

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
  if (millis() - lastStateChangeTime > LED_TIMEOUT && state != 0) {
    state = 0;
    Serial.println("LED timeout reached. Returning to idle state.");
  }

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

  state = newState;

  lastStateChangeTime = millis();

  Serial.print("State changed to: ");

  Serial.println(state);

  esp_now_send(broadcastAddress, &state, 1);

}

void OnDataRecv(uint8_t* mac, uint8_t* incomingData, uint8_t len) {

  Serial.println("Data received from ESP-B");

  

  if (len != 1) {

    Serial.println("Invalid message length");

    return;

  }


  uint8_t receivedState = incomingData[0];


  Serial.print("Received message: state = ");

  Serial.println(receivedState);


  if (receivedState >= 1 && receivedState <= 3) {

    // For states 1, 2, and 3, just update the state without sending it back

    state = receivedState;

    lastStateChangeTime = millis();

    Serial.print("State changed to: ");

    Serial.println(state);

  } else if (receivedState == 4) {

    // For state 4, use setState to ensure it's sent back

    setState(receivedState);

  } else {

    Serial.println("Unexpected state received");

  }

}

void setup() {
  Serial.begin(74880);
  
  pinMode(CALL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_Red, OUTPUT);
  pinMode(LED_Yellow, OUTPUT);
  pinMode(LED_Green, OUTPUT);
  
  // Boot sequence
  digitalWrite(LED_Red, HIGH);
  delay(500);
  digitalWrite(LED_Red, LOW);
  digitalWrite(LED_Yellow, HIGH);
  delay(500);
  digitalWrite(LED_Yellow, LOW);
  digitalWrite(LED_Green, HIGH);
  delay(500);
  digitalWrite(LED_Green, LOW);
  delay(500);
  
  Serial.println("ESP-A starting up...");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.println("WiFi set to station mode");
  
  if (esp_now_init() != 0) {
    Serial.println("ERROR: ESPNow init failed");
    return;
  }
  Serial.println("ESPNow initialized successfully");
  
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("ESPNow initialized and receive callback registered");
  
  Serial.println("Setup complete. ESP-A ready.");
}

void loop() {
  int callButtonReading = digitalRead(CALL_BUTTON_PIN);
  
  if (callButtonReading != callButtonLastState) {
    callButtonLastDebounceTime = millis();
    Serial.println("Button state changed, starting debounce");
  }
  
  if ((millis() - callButtonLastDebounceTime) > DEBOUNCE_DELAY) {
    if (callButtonState != callButtonReading) {
      callButtonState = callButtonReading;
      Serial.print("Button state confirmed: ");
      Serial.println(callButtonState == LOW ? "PRESSED" : "RELEASED");
      
      if (callButtonState == LOW) {
        Serial.println("Call button pressed, sending wake-up message to ESP-B");
        digitalWrite(LED_Green, LOW);
        digitalWrite(LED_Yellow, HIGH);
        
        setState(4);
      }
    }
  }
  
  callButtonLastState = callButtonReading;
  
  updateLEDs();
  
  delay(10);
}
