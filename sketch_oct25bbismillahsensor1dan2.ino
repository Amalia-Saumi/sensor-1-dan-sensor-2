#define BLYNK_TEMPLATE_ID "TMPL63elZ23Za"
#define BLYNK_TEMPLATE_NAME "Pengukuran dan analisis debit air"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

// Konfigurasi Blynk
char auth[] = "MsU3-o-oSF8-BB8m5i1KaSImE9zcFmBw";
char ssid[] = "Ndk Modal";
char pass[] = "12345678";

// Pin untuk Sensor Aliran Air
#define FLOW_SENSOR_PIN1 D5  // Sensor 1
#define FLOW_SENSOR_PIN2 D6  // Sensor 2

// Inisialisasi LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Alamat I2C LCD mungkin berbeda

volatile int flowPulseCount1 = 0;
volatile int flowPulseCount2 = 0;

// Koefisien kalibrasi sensor aliran
const float calibrationFactor = 7.5;  // Pulsa per liter

unsigned long previousMillis = 0; // Variabel untuk menghitung waktu
float totalVolumeSensor2_m3 = 0.0;    // Variabel untuk menyimpan total volume sensor 2 (m³)
float totalBiaya = 0.0;               // Variabel untuk menyimpan total biaya

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000);  // Waktu UTC+7 (25200 detik offset)

// ISR untuk menangkap pulsa dari sensor flow
void ICACHE_RAM_ATTR flowPulseISR1() { flowPulseCount1++; }
void ICACHE_RAM_ATTR flowPulseISR2() { flowPulseCount2++; }

void setup() {
  Serial.begin(9600);
  delay(100);

  Serial.println("Mulai setup...");
  Blynk.begin(auth, ssid, pass);

  lcd.init();
  lcd.backlight();
  Serial.println("LCD diinisialisasi...");

  pinMode(FLOW_SENSOR_PIN1, INPUT);
  pinMode(FLOW_SENSOR_PIN2, INPUT);

  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN1), flowPulseISR1, RISING);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN2), flowPulseISR2, RISING);

  Serial.println("Interrupts diinisialisasi...");

  timeClient.begin();  // Mulai client NTP
}

// Fungsi untuk menghitung biaya berdasarkan volume air
float hitungBiaya(float totalVolumeSensor2_m3) {
  float biaya = 0;

  if (totalVolumeSensor2_m3 <= 10) {
    biaya = totalVolumeSensor2_m3 * 210;  // per m³
  } else if (totalVolumeSensor2_m3 <= 20) {
    biaya = (10 * 210) + ((totalVolumeSensor2_m3 - 10) * 310);
  } else if (totalVolumeSensor2_m3 <= 30) {
    biaya = (10 * 210) + (10 * 310) + ((totalVolumeSensor2_m3 - 20) * 450);
  } else {
    biaya = (10 * 210) + (10 * 310) + (10 * 450) + ((totalVolumeSensor2_m3 - 30) * 630);
  }
  return biaya;
}

// Fungsi untuk menghitung dan menampilkan status kebocoran
void tampilkanStatusKebocoran(float flowRate1, float flowRate2) {
  // Toleransi 5% untuk perbedaan debit
  float toleransi = 0.05 * flowRate1;

  // Hitung selisih debit
  float selisihDebit = abs(flowRate1 - flowRate2);
  
  String statusKebocoran = "Aman";
  String tingkatKebocoran = "Tidak ada";

  // Tentukan status kebocoran berdasarkan selisih
  if (selisihDebit > toleransi) {
    if (selisihDebit >= 0.01) {
      tingkatKebocoran = "Besar";
    } else if (selisihDebit >= 0.006) {
      tingkatKebocoran = "Sedang";
    } else if (selisihDebit >= 0.005) {
      tingkatKebocoran = "Kecil";
    }
    statusKebocoran = "Bocor cabang2";
  }

  // Tampilkan status kebocoran di Serial Monitor
  Serial.print("Status Kebocoran: ");
  Serial.println(statusKebocoran);
  Serial.print("Tingkat Kebocoran: ");
  Serial.println(tingkatKebocoran);

  // Kirim status kebocoran ke Blynk
  Blynk.virtualWrite(V3, statusKebocoran);
  Blynk.virtualWrite(V4, tingkatKebocoran);

  // Tampilkan status kebocoran di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sts: ");
  lcd.print(statusKebocoran);
  lcd.setCursor(0, 1);
  lcd.print("Tingkat: ");
  lcd.print(tingkatKebocoran);
}

void loop() {
  Blynk.run();

  unsigned long currentMillis = millis();
  unsigned long interval = 1000;  // 1 detik

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Hentikan interrupt saat membaca pulsa
    noInterrupts();
    int pulseCount1 = flowPulseCount1;
    int pulseCount2 = flowPulseCount2;
    flowPulseCount1 = 0;
    flowPulseCount2 = 0;
    interrupts();

    // Hitung volume air dalam liter, kemudian konversi ke m³
    float volume1_m3 = (pulseCount1 / calibrationFactor) / 1000.0;  // Konversi ke m³
    float volume2_m3 = (pulseCount2 / calibrationFactor) / 1000.0;  // Konversi ke m³

    // Tambahkan volume ke total volume sensor 2
    totalVolumeSensor2_m3 += volume2_m3;

    // Hitung biaya berdasarkan total volume air yang terpakai
    totalBiaya = hitungBiaya(totalVolumeSensor2_m3);

    // **Perhitungan Debit Air** dalam m³/menit
    float debitAir1 = ((pulseCount1 / calibrationFactor) * 0.001);  // Debit dalam m³/menit
    float debitAir2 = ((pulseCount2 / calibrationFactor) * 0.001);  // Debit dalam m³/menit

    // Kirim data debit air ke Blynk
    Blynk.virtualWrite(V0, String(debitAir1, 3));  // Debit air sensor 1 (m³/menit)
    Blynk.virtualWrite(V1, String(debitAir2, 3));  // Debit air sensor 2 (m³/menit)
    
    // Kirim data biaya ke Blynk
    Blynk.virtualWrite(V2, String(totalBiaya, 3));  // Biaya total

    // Tampilkan volume dalam m³ di Serial Monitor dengan 3 angka di belakang koma
    Serial.print("Sensor 1: Volume = ");
    Serial.print(String(volume1_m3, 3));
    Serial.println(" m³");

    Serial.print("Sensor 2: Volume = ");
    Serial.print(String(volume2_m3, 3));
    Serial.println(" m³");

    Serial.print("Biaya Total: ");
    Serial.print(String(totalBiaya, 3));
    Serial.println(" Rupiah");

    // **Tampilkan Debit Air** di Serial Monitor dengan 3 angka di belakang koma
    Serial.print("Sensor 1: Debit = ");
    Serial.print(String(debitAir1, 3));
    Serial.println(" m³/menit");

    Serial.print("Sensor 2: Debit = ");
    Serial.print(String(debitAir2, 3));
    Serial.println(" m³/menit");

    // Tampilkan status kebocoran
    tampilkanStatusKebocoran(debitAir1, debitAir2);

    // Tampilkan waktu (hanya jam) di Serial Monitor
    timeClient.update();
    Serial.print("Waktu (jam): ");
    Serial.println(timeClient.getFormattedTime());
  }
}
