#include <ESP8266WiFi.h>
#include <espnow.h>

// Define the MAC address of ESP-B
uint8_t broadcastAddress[] = {0x98, 0xF4, 0xAB, 0xBC, 0xCE, 0x25};

// Define the state
uint8_t state;

const int CALL_BUTTON_PIN = 2;  // GPIO 2 (D4)

const int DEBOUNCE_DELAY = 50;  // Debounce delay in milliseconds

int callButtonState = HIGH;  // Initial state of call button
int callButtonLastState = HIGH;  // Last state of call button
unsigned long callButtonLastDebounceTime = 0;  // Last debounce time of call button

const int LED_Red = 14;  // GPIO 14 (D5)
const int LED_Yellow = 12;  // GPIO 12 (D6)
const int LED_Green = 13;  // GPIO 13 (D7)



void takeAction(uint8_t state) {

  Serial.print("Taking action for state: ");

  Serial.println(state);

  

  switch (state) {

    case 1:

      Serial.println("Setting LEDs to Solid Red");

      digitalWrite(LED_Red, HIGH);

      digitalWrite(LED_Yellow, LOW);

      digitalWrite(LED_Green, LOW);

      break;

    case 2:

      Serial.println("Setting LEDs to Solid Yellow");

      digitalWrite(LED_Red, LOW);

      digitalWrite(LED_Yellow, HIGH);

      digitalWrite(LED_Green, LOW);

      break;

    case 3:

      Serial.println("Setting LEDs to Solid Green");

      digitalWrite(LED_Red, LOW);

      digitalWrite(LED_Yellow, LOW);

      digitalWrite(LED_Green, HIGH);

      break;

    default:

      Serial.println("WARNING: Unexpected state value");

      break;

  }

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

  // Handle incoming message
  if (receivedState >= 1 && receivedState <= 3) {
    state = receivedState;
    Serial.print("Valid state received. Updating to state: ");
    Serial.println(state);
    takeAction(state);
  } else {
    Serial.println("Unexpected state received");
  }

}

void setup() {
   Serial.begin(74880);

  Serial.println("ESP-A starting up...");

  // Initialize WiFi

  WiFi.mode(WIFI_STA);

  WiFi.disconnect();

  Serial.println("WiFi set to station mode");


  // Initialize ESPNow

  if (esp_now_init() != 0) {

    Serial.println("ERROR: ESPNow init failed");

    return;

  }

  Serial.println("ESPNow initialized successfully");

  // Register the sender and receiver callback functions
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("ESPNow initialized and receive callback registered");
  pinMode(CALL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_Red, OUTPUT);
  pinMode(LED_Yellow, OUTPUT);
  pinMode(LED_Green, OUTPUT);
  Serial.println("Setup complete. ESP-A ready.");
  }

void loop() {

  int callButtonReading = digitalRead(CALL_BUTTON_PIN);


  // Debounce the button readings

  if (callButtonReading != callButtonLastState) {

    callButtonLastDebounceTime = millis();

    Serial.println("Button state changed, starting debounce");

  }


  if ((millis() - callButtonLastDebounceTime) > DEBOUNCE_DELAY) {

    if (callButtonState != callButtonReading) {

      callButtonState = callButtonReading;

      Serial.print("Button state confirmed: ");

      Serial.println(callButtonState == LOW ? "PRESSED" : "RELEASED");


      // Only send the wake-up message when the button is pressed (transitions to LOW)

      if (callButtonState == LOW) {

        Serial.println("Call button pressed, sending wake-up message to ESP-B");

        digitalWrite(LED_Green, LOW);

        digitalWrite(LED_Yellow, HIGH);


        // Send wake-up message to ESP-B

        state = 4;

        esp_now_send(broadcastAddress, &state, 1);

        Serial.println("Wake-up message sent");

      }

    }

  }


  callButtonLastState = callButtonReading;


  delay(100);

}
