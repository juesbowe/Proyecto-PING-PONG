#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// ==========================================================
// --- CONFIGURACIÓN DE JUGADOR Y RECEPTOR ---
// ==========================================================

// CAMBIAR A '2' PARA EL SEGUNDO MANDO
const int PLAYER_ID = 2; 

// Dirección MAC de la consola (Verificar en el monitor serial de la consola)
uint8_t broadcastAddress[] = {0x94, 0xB5, 0x55, 0xF8, 0x4C, 0x48};

// --- PINES ---
#define I2C_SDA_PIN 21 
#define I2C_SCL_PIN 22 
#define PIN_BOTON_REMOTO 33 

// ==========================================================
// --- ESTRUCTURA DE COMUNICACIÓN (DEBE COINCIDIR CON CONSOLA) ---
// ==========================================================
typedef struct struct_message {
    int player_id;      // 4 bytes
    int16_t joy_y_val;  // 2 bytes
    bool btn_pressed;   // 1 byte
} __attribute__((packed)) AccelData_t;

AccelData_t myData;
Adafruit_MPU6050 mpu;
esp_now_peer_info_t peerInfo;

// --- VARIABLES PARA DEBOUNCE ---
bool lastPhysicalBtnState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; 

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_BOTON_REMOTO, INPUT_PULLUP);

  // 1. Inicializar I2C y MPU6050
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!mpu.begin()) {
    Serial.println("¡Error al encontrar el MPU-6050!");
    while (1) yield();
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);

  // 2. Configurar WiFi y ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
    return;
  }

  // 3. Registrar el "Peer" (Consola)
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error al agregar receptor");
    return;
  }

  Serial.printf("Mando Jugador %d iniciado y listo.\n", PLAYER_ID);
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // --- 1. ASIGNAR ID DE JUGADOR ---
  myData.player_id = PLAYER_ID;

  // --- 2. LÓGICA DE BOTÓN CON DEBOUNCE ---
  int reading = digitalRead(PIN_BOTON_REMOTO);
  if (reading != lastPhysicalBtnState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    myData.btn_pressed = (reading == LOW);
  }
  lastPhysicalBtnState = reading;

  // --- 3. PROCESAR MOVIMIENTO (ACELERÓMETRO) ---
  float accY = a.acceleration.y;
  // Limitamos la inclinación para que no sea demasiado sensible
  float inclinacion = constrain(accY, -6.0, 6.0);
  // Mapeamos de -6.0/6.0 m/s^2 al rango del joystick 0-4095
  myData.joy_y_val = map(inclinacion * 100, -600, 600, 0, 4095);

  // --- 4. ENVIAR DATOS ---
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  // --- DEBUG (Opcional, para ver en monitor serial del mando) ---
  /*
  if (result == ESP_OK) {
    Serial.printf("ID: %d | Joy: %d | Btn: %d\n", myData.player_id, myData.joy_y_val, myData.btn_pressed);
  } else {
    Serial.println("Error en el envío");
  }
  */

  delay(20); // 50 Hz es suficiente para una respuesta fluida
}