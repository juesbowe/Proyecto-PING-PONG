#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// --- CONFIGURACIÓN DE PINES Y MAC ---
#define I2C_SDA_PIN 21 
#define I2C_SCL_PIN 22 
#define PIN_BOTON_REMOTO 33 

uint8_t broadcastAddress[] = {0x94, 0xB5, 0x55, 0xF8, 0x4C, 0x48};

// --- VARIABLES PARA ANTIRREBOTE (DEBOUNCE) ---
bool lastPhysicalBtnState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // 50ms para estabilizar la señal

// Estructura de comunicación
typedef struct struct_message {
    int16_t joy_y_val;
    bool btn_pressed; 
} AccelData_t;

AccelData_t myData;
Adafruit_MPU6050 mpu;
esp_now_peer_info_t peerInfo;

void setup() {
  Serial.begin(115200);
  
  // Botón con resistencia pull-up interna
  pinMode(PIN_BOTON_REMOTO, INPUT_PULLUP);

  // 1. Inicializar I2C y Sensor
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

  // 3. Registrar Receptor
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error al agregar receptor");
    return;
  }

  Serial.println("Mando con Debounce Listo en GPIO 32.");
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // --- PARTE 1: LÓGICA DE ANTIRREBOTE ---
  int reading = digitalRead(PIN_BOTON_REMOTO);

  // Si el botón cambió por ruido o pulsación
  if (reading != lastPhysicalBtnState) {
    lastDebounceTime = millis();
  }

  // Solo si el estado se mantiene estable por más de 50ms
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Si es LOW, el botón está presionado realmente
    myData.btn_pressed = (reading == LOW);
  }
  lastPhysicalBtnState = reading;

  // --- PARTE 2: MOVIMIENTO ---
  float accY = a.acceleration.y;
  float inclinacion = constrain(accY, -6.0, 6.0);
  myData.joy_y_val = map(inclinacion * 100, -600, 600, 0, 4095);

  // --- PARTE 3: ENVÍO ---
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  delay(20); 
}