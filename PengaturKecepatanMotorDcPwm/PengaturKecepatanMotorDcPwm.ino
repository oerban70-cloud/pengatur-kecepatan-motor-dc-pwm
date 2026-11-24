// PengaturKecepatanMotorDcPwm — Pengatur kecepatan motor DC dengan PWM
// (ESP32 + potensiometer + driver L298N + motor DC + tombol arah)
// ---------------------------------------------------------------------------
// Putar potensiometer: kecepatan motor berubah halus dari berhenti sampai
// penuh. ESP32 membaca tegangan wiper potensiometer lewat ADC 12-bit,
// mengubahnya jadi persen kecepatan 0-100%, lalu mengirim sinyal PWM ke
// pin ENA driver L298N. Semakin besar duty cycle PWM, semakin besar
// tegangan rata-rata yang diterima motor, semakin kencang putarannya.
// Tombol tekan membalik arah putar (maju/mundur) lewat pin IN1/IN2 L298N.
// LED hijau menyala selama motor berputar, padam saat motor berhenti.
//
// Kenapa PWM, bukan tegangan langsung? Potensiometer sebagai pembagi
// tegangan langsung ke motor akan membuang daya jadi panas dan motor
// berputar kasar. PWM menyalakan-mematikan motor ribuan kali per detik
// (di sini 5 kHz) — motor merespons tegangan rata-ratanya, jadi putaran
// halus, efisien, dan torsi di kecepatan rendah tetap terjaga.
//
// Wiring (lihat schematic.png):
//   Potensiometer 10k  wiper (tengah) -> GPIO34 (ADC1, hanya input)
//   Potensiometer 10k  ujung kiri     -> GND
//   Potensiometer 10k  ujung kanan    -> 3V3
//   L298N  IN1  -> GPIO26
//   L298N  IN2  -> GPIO27
//   L298N  ENA  -> GPIO25 (PWM)
//   L298N  OUT1 / OUT2 -> motor DC (dua kabel motor, bolak-balik bebas)
//   L298N  +12V -> adaptor 12V (catu motor, JANGAN dari pin ESP32)
//   L298N  GND  -> GND ESP32 (GND harus disatukan)
//   L298N  5V   -> (jumper 5V EN terpasang) catu logika dari regulator L298N
//   Tombol arah  kaki 1 -> GPIO33 (INPUT_PULLUP, aktif LOW)
//   Tombol arah  kaki 2 -> GND
//   LED hijau (seri resistor 220 ohm)  anoda -> GPIO32, katoda -> GND
//   ESP32 dicatu USB 5V.
//
// PERHATIAN:
//   - Motor DC dicatu adaptor 12V sendiri lewat L298N. Jangan mencatu
//     motor dari pin 5V/3V3 ESP32 — arusnya tidak cukup dan bisa merusak
//     board. Arus motor maksimal 2A per kanal (L298N), pakai motor DC
//     12V di bawah 2A. Motor yang lebih besar butuh driver yang lebih kuat.
//   - Duty cycle di bawah kira-kira 25% sering belum cukup memutar motor
//     (tergantung beban dan motornya) — putar potensiometer sedikit lebih
//     jauh sampai motor mulai berputar. Itu normal, bukan kerusakan.
//   - Saat membalik arah, kode mematikan PWM sebentar (150 ms) supaya
//     motor berhenti dulu sebelum berputar berlawanan arah — melindungi
//     driver dan gir motor dari hentakan.
//
// Library yang dipakai: tidak ada (hanya API bawaan ESP32: ADC + ledc).
// ---------------------------------------------------------------------------

// ------------------------- Konfigurasi -------------------------------------
// Pin input dan aktuator.
constexpr uint8_t PIN_POTENSIOMETER = 34;  // wiper potensiometer (ADC1_CH6)
constexpr uint8_t PIN_L298N_IN1     = 26;  // arah putar motor (L298N IN1)
constexpr uint8_t PIN_L298N_IN2     = 27;  // arah putar motor (L298N IN2)
constexpr uint8_t PIN_L298N_ENA     = 25;  // kecepatan motor (PWM, L298N ENA)
constexpr uint8_t PIN_TOMBOL_ARAH   = 33;  // tombol balik arah, aktif LOW
constexpr uint8_t PIN_LED_JALAN     = 32;  // LED hijau, nyala saat motor jalan

// Parameter PWM (ledc, API baru ESP32 core 3.x/4.x).
constexpr uint32_t FREKUENSI_PWM_HZ = 5000;  // 5 kHz, di atas jangkauan dengar
constexpr uint8_t  RESOLUSI_PWM_BIT = 8;     // duty 0-255
constexpr uint8_t  DUTY_MAKSIMAL     = 255;  // duty penuh (100%)

// Parameter pembacaan potensiometer (ADC 12-bit: 0-4095).
constexpr uint16_t ADC_MAKSIMAL   = 4095;  // nilai ADC saat wiper di 3V3
constexpr uint16_t AMBANG_MATI    = 80;    // di bawah ini motor dianggap mati
constexpr uint8_t  JUMLAH_SAMPEL  = 10;    // sampel ADC yang dirata-rata
constexpr uint16_t JEDA_SAMPEL_MS = 2;     // jeda antar sampel ADC

// Parameter tombol arah (debounce + jeda balik arah).
constexpr uint16_t DEBOUNCE_TOMBOL_MS = 50;   // abaikan pantulan tombol
constexpr uint16_t JEDA_BALIK_ARAH_MS = 150;  // motor berhenti dulu sebelum balik

// Interval tampil status di Serial Monitor.
constexpr uint16_t INTERVAL_STATUS_MS = 500;

// ------------------------- Status ------------------------------------------
// Arah putar motor: true = maju (IN1 HIGH, IN2 LOW), false = mundur.
bool arahMaju = true;
// Status tombol sebelumnya (untuk deteksi tepi + debounce).
bool tombolTerakhirDitekan = false;
uint32_t waktuTombolTerakhirMs = 0;
// Kecepatan terakhir (persen) untuk Serial Monitor.
uint8_t persenTerakhir = 255;  // 255 = belum pernah tampil
uint32_t waktuStatusTerakhirMs = 0;

// ------------------------- Fungsi ------------------------------------------
// Baca potensiometer (rata-rata 10 sampel) dan kembalikan persen 0-100.
// Di bawah AMBANG_MATI hasilnya 0 — zona mati di ujung putaran.
uint8_t bacaPersenPotensio() {
  uint32_t jumlah = 0;
  for (uint8_t i = 0; i < JUMLAH_SAMPEL; i++) {
    jumlah += analogRead(PIN_POTENSIOMETER);
    delay(JEDA_SAMPEL_MS);
  }
  const uint16_t rataRata = jumlah / JUMLAH_SAMPEL;
  if (rataRata <= AMBANG_MATI) {
    return 0;
  }
  // Petakan sisa rentang ADC (AMBANG_MATI-4095) ke persen 0-100.
  return map(rataRata, AMBANG_MATI, ADC_MAKSIMAL, 0, 100);
}

// Ubah persen kecepatan (0-100) jadi duty PWM (0-255).
uint8_t persenKeDuty(uint8_t persen) {
  return map(persen, 0, 100, 0, DUTY_MAKSIMAL);
}

// Atur arah putar lewat IN1/IN2 L298N (tanpa mengubah kecepatan).
void aturArah(bool maju) {
  digitalWrite(PIN_L298N_IN1, maju ? HIGH : LOW);
  digitalWrite(PIN_L298N_IN2, maju ? LOW : HIGH);
}

// Atur kecepatan motor (duty PWM) dan LED indikator jalan.
void aturKecepatan(uint8_t duty) {
  ledcWrite(PIN_L298N_ENA, duty);
  digitalWrite(PIN_LED_JALAN, duty > 0 ? HIGH : LOW);
}

// Tangani tombol arah: sekali tekan (tepi turun + debounce) balik arah.
// Motor dimatikan sebentar dulu supaya tidak menghentak saat berbalik.
void tanganiTombolArah(uint8_t dutySaatIni) {
  const bool ditekan = digitalRead(PIN_TOMBOL_ARAH) == LOW;
  const uint32_t sekarang = millis();
  if (ditekan && !tombolTerakhirDitekan &&
      sekarang - waktuTombolTerakhirMs > DEBOUNCE_TOMBOL_MS) {
    waktuTombolTerakhirMs = sekarang;
    arahMaju = !arahMaju;
    aturKecepatan(0);              // berhenti dulu
    delay(JEDA_BALIK_ARAH_MS);     // tunggu motor berhenti
    aturArah(arahMaju);            // balik IN1/IN2
    aturKecepatan(dutySaatIni);    // lanjut dengan kecepatan yang sama
    Serial.print("Arah dibalik: ");
    Serial.println(arahMaju ? "MAJU" : "MUNDUR");
  }
  tombolTerakhirDitekan = ditekan;
}

// Tampilkan kecepatan + arah di Serial Monitor tiap 500 ms (bila berubah).
void tampilkanStatus(uint8_t persen, uint8_t duty) {
  const uint32_t sekarang = millis();
  if (persen == persenTerakhir &&
      sekarang - waktuStatusTerakhirMs < INTERVAL_STATUS_MS) {
    return;
  }
  persenTerakhir = persen;
  waktuStatusTerakhirMs = sekarang;
  Serial.print("Kecepatan: ");
  Serial.print(persen);
  Serial.print("%  (duty ");
  Serial.print(duty);
  Serial.print("/255)  Arah: ");
  Serial.println(arahMaju ? "MAJU" : "MUNDUR");
}

// ------------------------- Setup & loop ------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_L298N_IN1, OUTPUT);
  pinMode(PIN_L298N_IN2, OUTPUT);
  pinMode(PIN_TOMBOL_ARAH, INPUT_PULLUP);
  pinMode(PIN_LED_JALAN, OUTPUT);

  // PWM 5 kHz, resolusi 8-bit di pin ENA (API ledc baru core 3.x/4.x).
  ledcAttach(PIN_L298N_ENA, FREKUENSI_PWM_HZ, RESOLUSI_PWM_BIT);

  aturArah(arahMaju);
  aturKecepatan(0);
  Serial.println("Pengatur kecepatan motor DC PWM siap.");
  Serial.println("Putar potensiometer untuk mengatur kecepatan,");
  Serial.println("tekan tombol untuk membalik arah putar.");
}

void loop() {
  const uint8_t persen = bacaPersenPotensio();
  const uint8_t duty = persenKeDuty(persen);
  tanganiTombolArah(duty);
  aturKecepatan(duty);
  tampilkanStatus(persen, duty);
}
