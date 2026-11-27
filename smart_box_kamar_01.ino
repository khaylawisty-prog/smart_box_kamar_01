#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ESP32Servo.h>
#include <Keypad.h>

#define WIFI_SSID "NAMA_WIFI_KAMU"
#define WIFI_PASSWORD "PASSWORD_WIFI_KAMU"

#define API_KEY "ISI_API_KEY_KAMU"
#define DATABASE_URL "https://NAMA-PROJECT-KAMU-default-rtdb.firebaseio.com"

#define SERVO_PIN 13
#define SENSOR_PAKET_PIN 27

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {19, 18, 5, 17};
byte colPins[COLS] = {16, 4, 2, 15};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Servo kunciServo;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String pinAktif = "1234";
String inputKeypad = "";
bool pintuTerbuka = false;
unsigned long waktuBuka = 0;
const unsigned long DURASI_BUKA_MS = 5000;

int statusPaketTerakhir = -1;
unsigned long waktuSinkron = 0;
const unsigned long INTERVAL_SINKRON_MS = 1500;

void kunciPintu() {
  kunciServo.write(0);
  pintuTerbuka = false;
  Serial.println("Pintu terkunci.");
}

void bukaPintu5Detik(const char* sumber) {
  kunciServo.write(90);
  pintuTerbuka = true;
  waktuBuka = millis();
  Serial.printf("Pintu terbuka (%s) 5 detik...\n", sumber);
}

void cekSensorPaket() {
  int terbacaAda = (digitalRead(SENSOR_PAKET_PIN) == LOW) ? 1 : 0;
  if (terbacaAda != statusPaketTerakhir) {
    statusPaketTerakhir = terbacaAda;
    bool adaPaket = (terbacaAda == 1);
    if (Firebase.setBool(fbdo, "/kamar_01/ada_paket", adaPaket)) {
      Serial.printf("Update /kamar_01/ada_paket -> %s\n", adaPaket ? "true" : "false");
    } else {
      Serial.println("Gagal update ada_paket: " + fbdo.errorReason());
    }
  }
}

void sinkronFirebase() {
  if (Firebase.getString(fbdo, "/kamar_01/pin")) {
    if (fbdo.dataType() == "string") {
      String pinBaru = fbdo.stringData();
      pinBaru.trim();
      if (pinBaru.length() >= 4 && pinBaru != pinAktif) {
        pinAktif = pinBaru;
        Serial.println("PIN diperbarui dari Firebase: " + pinAktif);
      }
    }
  }

  if (Firebase.getBool(fbdo, "/kamar_01/buka_pintu")) {
    if (fbdo.dataType() == "boolean" && fbdo.boolData() == true) {
      bukaPintu5Detik("Web");
      Firebase.setBool(fbdo, "/kamar_01/buka_pintu", false);
    }
  }
}

void prosesKeypad() {
  char tombol = keypad.getKey();
  if (!tombol) return;

  if (tombol == '#') {
    if (inputKeypad == pinAktif) {
      Serial.println("PIN benar!");
      bukaPintu5Detik("Keypad");
    } else {
      Serial.println("PIN salah!");
    }
    inputKeypad = "";
  } else if (tombol == '*') {
    inputKeypad = "";
    Serial.println("Input PIN direset.");
  } else if (isDigit(tombol)) {
    inputKeypad += tombol;
    Serial.print("*");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PAKET_PIN, INPUT_PULLUP);

  kunciServo.attach(SERVO_PIN);
  kunciPintu();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung! IP: " + WiFi.localIP().toString());

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = "";
  auth.user.password = "";

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (!Firebase.getString(fbdo, "/kamar_01/pin")) {
    Firebase.setString(fbdo, "/kamar_01/pin", pinAktif);
  }
  Firebase.setBool(fbdo, "/kamar_01/buka_pintu", false);

  cekSensorPaket();
  sinkronFirebase();
}

void loop() {
  if (pintuTerbuka && (millis() - waktuBuka >= DURASI_BUKA_MS)) {
    kunciPintu();
  }

  cekSensorPaket();
  prosesKeypad();

  if (millis() - waktuSinkron >= INTERVAL_SINKRON_MS) {
    waktuSinkron = millis();
    sinkronFirebase();
  }
}