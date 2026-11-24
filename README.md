# Pengatur Kecepatan Motor DC PWM — ESP32 + Potensiometer + Driver L298N

Motor DC-mu cuma bisa kencang atau mati total? Satu putaran potensiometer
mengatur pelan dan kencangnya motor. ESP32 membaca tegangan wiper
potensiometer lewat ADC 12-bit, mengubahnya jadi persen kecepatan 0-100%,
lalu mengirim sinyal PWM 5 kHz ke pin ENA driver L298N. Semakin besar duty
cycle PWM, semakin besar tegangan rata-rata yang diterima motor, semakin
kencang putarannya. Tombol tekan membalik arah putar (maju/mundur), dan LED
hijau menyala selama motor berputar.

Video Short (60 detik): link menyusul setelah upload

## Cara kerja

1. Potensiometer 10k dipasang sebagai pembagi tegangan antara 3V3 dan GND;
   wiper-nya menghasilkan tegangan 0-3,3 V sesuai posisi putaran.
2. ESP32 membaca wiper lewat ADC 12-bit (GPIO34) dan merata-rata 10 sampel
   tiap pembacaan supaya angka kecepatan tidak lompat-lompat.
3. Nilai ADC dipetakan jadi persen kecepatan 0-100%. Di bawah ambang mati
   (ADC 80, sekitar 2% putaran) motor dianggap mati — ini zona mati di
   ujung putaran, supaya motor benar-benar berhenti saat potensio di nol.
4. Persen diubah jadi duty PWM 0-255 dan dikirim ke pin ENA L298N
   (frekuensi 5 kHz, di atas jangkauan dengar manusia, jadi motor tidak
   berdenging). L298N menyalakan-mematikan motor ribuan kali per detik;
   motor merespons tegangan rata-ratanya, jadi putaran halus dan efisien,
   torsi di kecepatan rendah tetap terjaga.
5. Arah putar diatur lewat pin IN1/IN2 L298N. Sekali tekan tombol, arah
   dibalik: kode mematikan PWM selama 150 ms dulu supaya motor berhenti
   sebelum berputar berlawanan arah — melindungi driver dan gir motor dari
   hentakan.
6. LED hijau menyala selama motor berputar, padam saat motor berhenti.
   Kecepatan dan arah tampil di Serial Monitor tiap 500 ms (baud 115200).

Kenapa PWM, bukan tegangan langsung? Potensiometer sebagai pembagi tegangan
langsung ke motor akan membuang daya jadi panas dan motor berputar kasar.
PWM menyalakan-mematikan motor ribuan kali per detik — motor merespons
tegangan rata-ratanya, jadi putaran halus, efisien, dan torsi di kecepatan
rendah tetap terjaga.

## Wiring

| Komponen | Pin modul | ESP32 |
| --- | --- | --- |
| Potensiometer 10k wiper (tengah) | WIPER | GPIO34 (ADC1) |
| Potensiometer 10k ujung 3V3 | 3V3 | 3V3 |
| Potensiometer 10k ujung GND | GND | GND |
| L298N IN1 (arah) | IN1 | GPIO26 |
| L298N IN2 (arah) | IN2 | GPIO27 |
| L298N ENA (PWM) | ENA | GPIO25 |
| L298N OUT1 / OUT2 | OUT1 / OUT2 | Motor DC 12V (dua kabel motor) |
| L298N +12V | +12V | Adaptor 12V |
| L298N GND | GND | GND (disatukan dengan GND ESP32) |
| Tombol arah kaki 1 (aktif LOW) | 1 | GPIO33 (INPUT_PULLUP) |
| Tombol arah kaki 2 | 2 | GND |
| LED hijau (seri 220 Ω) | Anoda | GPIO32 |
| LED hijau (seri 220 Ω) | Katoda | GND |
| ESP32 | USB | Adaptor USB 5V |

Skema lengkap (gaya Fritzing): lihat `schematic.png`.

PERHATIAN: motor DC dicatu adaptor 12V sendiri lewat L298N. Jangan mencatu
motor dari pin 5V/3V3 ESP32 — arusnya tidak cukup dan bisa merusak board.
Arus motor maksimal 2A per kanal (L298N); pakai motor DC 12V di bawah 2A.
Motor yang lebih besar butuh driver yang lebih kuat. Duty cycle di bawah
kira-kira 25% sering belum cukup memutar motor (tergantung beban dan
motornya) — putar potensiometer sedikit lebih jauh sampai motor mulai
berputar. Itu normal, bukan kerusakan.

## Konfigurasi

Di `PengaturKecepatanMotorDcPwm/PengaturKecepatanMotorDcPwm.ino`:

- `PIN_POTENSIOMETER` (34) — wiper potensiometer (ADC1_CH6).
- `PIN_L298N_IN1` (26) / `PIN_L298N_IN2` (27) — arah putar motor.
- `PIN_L298N_ENA` (25) — kecepatan motor (PWM).
- `PIN_TOMBOL_ARAH` (33) — tombol balik arah, aktif LOW (INPUT_PULLUP).
- `PIN_LED_JALAN` (32) — LED hijau, nyala saat motor jalan.
- `FREKUENSI_PWM_HZ` (5000) / `RESOLUSI_PWM_BIT` (8) — PWM 5 kHz, duty 0-255.
- `AMBANG_MATI` (80) — ADC di bawah ini = motor mati (zona mati).
- `JUMLAH_SAMPEL` (10) / `JEDA_SAMPEL_MS` (2) — perata-rata pembacaan ADC.
- `DEBOUNCE_TOMBOL_MS` (50) — abaikan pantulan tombol.
- `JEDA_BALIK_ARAH_MS` (150) — motor berhenti dulu sebelum balik arah.
- `INTERVAL_STATUS_MS` (500) — tampil status di Serial Monitor.

## Komponen

- ESP32 DevKit
- Potensiometer 10k (putar)
- Driver motor L298N
- Motor DC 12V (di bawah 2A)
- Tombol tekan (push button)
- LED hijau + resistor 220 ohm
- Adaptor 12V (catu motor) + adaptor USB 5V (catu ESP32)

## Verifikasi compile

Terverifikasi compile dengan Arduino CLI 1.6.0, ESP32 core 4.0.0,
board `esp32:esp32:esp32`:

- Sketch memakai 307.131 byte (23%) dari program storage (maksimal 1.310.720 byte).
- Variabel global memakai 23.516 byte (7%) dari dynamic memory (maksimal 327.680 byte).
- Tidak memakai library eksternal — hanya API bawaan ESP32 (ADC + ledc).

## Lisensi

MIT — lihat `LICENSE`.
