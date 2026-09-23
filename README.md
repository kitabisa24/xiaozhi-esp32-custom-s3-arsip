# 🤖 Xiaozhi AI Custom ESP32-S3 N16R8 — Kipas & Lampu

Custom firmware **Xiaozhi AI** berbasis ESP32-S3-WROOM-1 N16R8 dengan kontrol perangkat menggunakan perintah suara Bahasa Indonesia.

Project ini mengembangkan Xiaozhi menjadi perangkat smart-home sederhana:

- 🌀 Relay 1 / GPIO18 → Kipas
- 💡 Relay 2 / GPIO8 → Lampu
- 🌡️ DS18B20 → Sensor suhu
- 🎵 DFPlayer Mini → Pemutar musik
- 🎙️ INMP441 → Mikrofon
- 🔊 MAX98357A → Audio output
- 🖥️ SSD1306 128x64 → Display
- 📶 Wi-Fi + server Xiaozhi
- wake word: Hi, Jarvis
- 🇮🇩 Bahasa Indonesia
- 🗂️ Startup logo **ARSIP — Created by Arsiparis**

> **Versi yang diuji pada project ini:** Xiaozhi **v2.2.3** + ESP-IDF **v5.5.4** + ESP32-S3.
>
> Jangan langsung mengikuti branch `main` terbaru tanpa menyesuaikan versi ESP-IDF. Versi Xiaozhi terbaru dapat memiliki persyaratan SDK yang berbeda.

---

# 1. Hasil Akhir

## 🌀 Kipas

Perintah suara:

```text
"Jarvis, nyalakan kipas."
"Jarvis, matikan kipas."
"Jarvis, apakah kipas menyala?"
"Jarvis, status kipas."
```

Hubungan:

```text
GPIO18 → Relay 1 → Kipas
```

## 💡 Lampu

Perintah suara:

```text
"Xiaozhi, nyalakan lampu."
"Xiaozhi, matikan lampu."
"Xiaozhi, apakah lampu menyala?"
"Xiaozhi, status lampu."
```

Hubungan:

```text
GPIO8 → Relay 2 → Lampu
```

Relay firmware menggunakan logika **Active HIGH**.

---

# 2. Arsitektur Sistem

```text
                 ┌─────────────────────┐
                 │      XIAOZHI AI     │
                 │      ESP32-S3       │
                 └──────────┬──────────┘
                            │
             ┌──────────────┼──────────────┐
             │              │              │
             ▼              ▼              ▼
          INMP441        Wi-Fi          OLED
        Mikrofon          │           SSD1306
             │            │
             ▼            ▼
           AFE/VAD    Server Xiaozhi
                          │
                          ▼
                         AI
                          │
             ┌────────────┼────────────┐
             │            │            │
             ▼            ▼            ▼
          MCP Tool     TTS Audio     Jawaban
             │            │
       ┌─────┴─────┐      ▼
       │           │   MAX98357A
       ▼           ▼      │
    GPIO18       GPIO8    ▼
      Kipas       Lampu  Speaker
```

Perangkat tambahan:

```text
GPIO9   → DS18B20
GPIO10  → DFPlayer RX
GPIO11  ← DFPlayer TX
```

---

# 3. Hardware

- ESP32-S3-WROOM-1 N16R8
- INMP441
- MAX98357A
- Speaker 3–8 Ω
- OLED SSD1306 128x64 I2C
- Relay 1 channel × 2
- DS18B20
- Resistor 4.7kΩ
- DFPlayer Mini
- microSD
- Catu daya 5V
- Step-down 5V → 3.3V jika diperlukan
- Kipas
- Lampu
- Kabel/PCB universal

---

# 4. Wiring Lengkap

## OLED SSD1306

| OLED | ESP32-S3 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO41 |
| SCL | GPIO42 |

Alamat I2C:

```text
0x3C
```

Resolusi:

```text
128 × 64
```

## INMP441

| INMP441 | ESP32-S3 |
|---|---|
| VDD | 3.3V |
| GND | GND |
| SCK | GPIO5 |
| WS | GPIO4 |
| SD | GPIO6 |
| L/R | GND |

Konfigurasi:

```text
WS  = GPIO4
SCK = GPIO5
SD  = GPIO6
```

## MAX98357A

| MAX98357A | ESP32-S3 |
|---|---|
| VIN | 3.3V / sesuai modul |
| GND | GND |
| BCLK | GPIO15 |
| LRC/LRCLK | GPIO16 |
| DIN | GPIO7 |
| SPK+ | Speaker + |
| SPK- | Speaker - |

Konfigurasi:

```text
BCLK = GPIO15
LRC  = GPIO16
DIN  = GPIO7
```

Jangan menghubungkan output speaker DFPlayer langsung ke DIN MAX98357A.

## DS18B20

| DS18B20 | ESP32-S3 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| DATA | GPIO9 |

Resistor:

```text
DATA ─── 4.7kΩ ─── 3.3V
```

## DFPlayer Mini

| DFPlayer | ESP32-S3 |
|---|---|
| VCC | 5V |
| GND | GND |
| RX | GPIO10 |
| TX | GPIO11 |

Hubungan:

```text
ESP GPIO10 TX ─────→ DFPlayer RX
ESP GPIO11 RX ←──── DFPlayer TX
```

Baud:

```text
9600
```

## Relay 1 — Kipas

```text
ESP32 GPIO18
      │
      ▼
  Relay 1 IN
      │
      ▼
    KIPAS
```

Firmware:

```cpp
RELAY1_GPIO = GPIO18
RELAY_ACTIVE_HIGH = 1
```

## Relay 2 — Lampu

```text
ESP32 GPIO8
      │
      ▼
  Relay 2 IN
      │
      ▼
    LAMPU
```

Firmware:

```cpp
RELAY2_GPIO = GPIO8
RELAY_ACTIVE_HIGH = 1
```

### Sisi beban relay

Untuk beban yang ingin **mati ketika relay tidak aktif**, umumnya digunakan:

```text
Sumber listrik
      │
      ▼
     COM
      │
      ▼
      NO
      │
      ▼
    Beban
      │
      ▼
Netral / return
```

> ⚠️ Jika Kipas/Lampu menggunakan PLN/AC, terminal COM/NO/NC berbahaya. Gunakan enclosure, terminal, kabel, dan proteksi yang sesuai. Pemasangan sisi PLN sebaiknya dilakukan teknisi/orang yang memahami instalasi listrik.

---

# 5. Ringkasan GPIO

| Fungsi | GPIO |
|---|---:|
| INMP441 WS | 4 |
| INMP441 SCK | 5 |
| INMP441 SD | 6 |
| MAX98357A DIN | 7 |
| MAX98357A BCLK | 15 |
| MAX98357A LRC | 16 |
| Relay 1 / Kipas | 18 |
| Relay 2 / Lampu | 8 |
| DS18B20 DATA | 9 |
| DFPlayer TX dari ESP | 10 |
| DFPlayer RX ke ESP | 11 |
| OLED SDA | 41 |
| OLED SCL | 42 |

GPIO19/20 tidak digunakan untuk perangkat custom karena berkaitan dengan USB-JTAG/native USB.

GPIO26–37 dihindari pada N16R8 karena terkait Flash/PSRAM.

---

# 6. Versi Software

```text
Xiaozhi ESP32 : v2.2.3
ESP-IDF       : v5.5.4
Target        : esp32s3
Python        : 3.11.2
Git           : 2.44.0
```

---

# 7. Mendapatkan Source Xiaozhi v2.2.3

Clone:

```bat
git clone --branch v2.2.3 --recursive https://github.com/78/xiaozhi-esp32.git C:\xiaozhi-custom
```

Masuk:

```bat
cd C:\xiaozhi-custom
```

Source upstream:

https://github.com/78/xiaozhi-esp32

Release v2.2.3:

https://github.com/78/xiaozhi-esp32/releases/tag/v2.2.3

---

# 8. ESP-IDF v5.5.4

Gunakan **ESP-IDF 5.5.4** untuk mengikuti environment project ini.

Cek:

```bat
idf.py --version
```

Target:

```text
ESP-IDF v5.5.4
```

Referensi:

https://github.com/espressif/esp-idf/releases/tag/v5.5.4

---

# 9. Set Target ESP32-S3

```bat
cd C:\xiaozhi-custom
idf.py set-target esp32s3
```

> ⚠️ Setelah `set-target`, periksa kembali `Board Type` karena `sdkconfig` dapat dibuat ulang.

---

# 10. Menambahkan Custom Board

Buat:

```text
main/boards/custom-xiaozhi-s3/
```

File utama:

```text
main/boards/custom-xiaozhi-s3/custom_xiaozhi_s3_board.cc
```

Board custom juga harus didaftarkan pada Kconfig dan CMake agar muncul di menu Board Type.

Panduan resmi:

https://github.com/78/xiaozhi-esp32/blob/main/docs/custom-board.md

---

# 11. Pilih Custom Board

Jalankan:

```bat
idf.py menuconfig
```

Kemudian:

```text
Xiaozhi Assistant
    └── Board Type
         └── Custom Xiaozhi ESP32-S3 N16R8
```

Pilih:

```text
(X) Custom Xiaozhi ESP32-S3 N16R8
```

Save → Exit.

---

# 12. Aktifkan Bahasa Indonesia

Masuk:

```bat
idf.py menuconfig
```

Cari:

```text
Default Language
```

Pilih:

```text
(X) Indonesian
```

Locale:

```text
id-ID
```

Build akan membuat konfigurasi bahasa secara otomatis.

---

# 13. Source Board Custom

File utama menangani:

```text
OLED
INMP441
MAX98357A
Relay
DS18B20
DFPlayer
MCP Tools
Boot Button
Wi-Fi
Startup logo
```

Fitur custom:

```text
Relay 1 = Kipas
Relay 2 = Lampu
```

MCP tools internal:

```text
self.relay.set
self.relay.get
self.temperature.get
self.dfplayer.play
self.dfplayer.stop
self.dfplayer.next
self.dfplayer.previous
self.dfplayer.volume
```

---

# 14. Build Firmware

```bat
cd C:\xiaozhi-custom
idf.py build
```

Jika berhasil:

```text
Project build complete.
```

Firmware:

```text
build\xiaozhi.bin
```

---

# 15. Flash ESP32-S3

Contoh jika port:

```text
COM5
```

Jalankan:

```bat
idf.py -p COM5 flash
```

Ganti `COM5` sesuai komputer.

Indikator berhasil:

```text
Hash of data verified.
```

dan:

```text
Hard resetting via RTS pin...
```

---

# 16. Monitor Serial

```bat
idf.py -p COM5 monitor
```

Keluar dari monitor:

```text
Ctrl + ]
```

---

# 17. Urutan Boot

```text
ESP32 reset
     ↓
Inisialisasi hardware
     ↓
Logo ARSIP
     ↓
Created by
Arsiparis
     ↓
Tampilan Xiaozhi
     ↓
Wi-Fi
     ↓
Server Xiaozhi
     ↓
Listening
     ↓
Perintah suara
```

---

# 18. Pengujian Kipas

```text
"Xiaozhi, nyalakan kipas."
```

Expected:

```text
GPIO18 = HIGH
Relay 1 aktif
Kipas menyala
```

Matikan:

```text
"Xiaozhi, matikan kipas."
```

Status:

```text
"Xiaozhi, apakah kipas menyala?"
```

---

# 19. Pengujian Lampu

```text
"Xiaozhi, nyalakan lampu."
```

Expected:

```text
GPIO8 = HIGH
Relay 2 aktif
Lampu menyala
```

Matikan:

```text
"Xiaozhi, matikan lampu."
```

Status:

```text
"Xiaozhi, apakah lampu menyala?"
```

---

# 20. Pengujian Suhu

```text
"Berapa suhu sekarang?"
```

Tool:

```text
self.temperature.get
```

Contoh:

```text
Suhu saat ini 29.1 derajat Celsius.
```

---

# 21. Pengujian DFPlayer

```text
"Putar lagu nomor lima."
```

Tool:

```text
self.dfplayer.play
```

DFPlayer menggunakan microSD dan UART 9600 baud.

---

# 22. Backup Firmware Stabil

Setelah build:

```bat
cd C:\xiaozhi-custom
copy /Y build\xiaozhi.bin build\xiaozhi-indonesia-kipas-lampu-arsip.bin
copy /Y sdkconfig sdkconfig-indonesia-kipas-lampu-arsip
```

Backup project:

```bat
robocopy . "K:\Xiaozhi AI\Xiaozhi AI custom sendiri\custom_xiaozhi_s3_working_backup" /E
```

Pada project asli, backup ini menghasilkan:

```text
FAILED : 0
```

sehingga dapat digunakan sebagai baseline firmware yang sudah terbukti bekerja.

---

# 23. Troubleshooting

## OLED tidak tampil

Periksa:

```text
SDA → GPIO41
SCL → GPIO42
VCC → 3.3V
GND → GND
I2C → 0x3C
```

## Mikrofon tidak sensitif

Periksa:

```text
VCC → 3.3V
GND → GND
SCK → GPIO5
WS  → GPIO4
SD  → GPIO6
```

Pastikan modul INMP441 tidak rusak.

## Relay terbalik

Jika:

```text
HIGH = mati
LOW  = nyala
```

ubah:

```cpp
#define RELAY_ACTIVE_HIGH 1
```

menjadi:

```cpp
#define RELAY_ACTIVE_HIGH 0
```

Lalu build dan flash ulang.

## Kipas tidak bekerja

Pastikan:

```text
GPIO18 → Relay 1 IN
```

## Lampu tidak bekerja

Pastikan:

```text
GPIO8 → Relay 2 IN
```

## DS18B20 tidak terbaca

Pastikan:

```text
DATA → GPIO9
```

dan:

```text
DATA ── 4.7kΩ ── 3.3V
```

## DFPlayer tidak bekerja

Periksa:

```text
GPIO10 TX → DFPlayer RX
GPIO11 RX ← DFPlayer TX
DFPlayer VCC → 5V
GND → GND
```

## Board kembali menjadi Bread Compact WiFi

Jalankan:

```bat
idf.py menuconfig
```

Pilih kembali:

```text
Xiaozhi Assistant
 → Board Type
  → Custom Xiaozhi ESP32-S3 N16R8
```

Save lalu:

```bat
idf.py build
```

---

# 24. Struktur Project

```text
C:\xiaozhi-custom
│
├── main
│   ├── boards
│   │   └── custom-xiaozhi-s3
│   │       └── custom_xiaozhi_s3_board.cc
│   │
│   ├── assets
│   │   └── locales
│   │       └── id-ID
│   │           └── language.json
│   │
│   ├── CMakeLists.txt
│   └── Kconfig.projbuild
│
├── build
│   └── xiaozhi.bin
│
├── sdkconfig
└── CMakeLists.txt
```

---

# 25. Keselamatan

⚠️ Relay yang mengendalikan listrik AC/PLN harus diperlakukan sebagai rangkaian berbahaya.

Jangan menguji terminal PLN dalam kondisi terbuka.

Gunakan:

- enclosure tertutup
- kabel sesuai arus/beban
- terminal yang sesuai
- proteksi listrik yang sesuai
- grounding jika diperlukan
- teknisi/orang yang memahami instalasi listrik

Untuk pengujian awal, lebih aman menggunakan beban tegangan rendah.

---

# 26. Referensi Resmi

- Xiaozhi ESP32: https://github.com/78/xiaozhi-esp32
- Release v2.2.3: https://github.com/78/xiaozhi-esp32/releases/tag/v2.2.3
- Custom Board Guide: https://github.com/78/xiaozhi-esp32/blob/main/docs/custom-board.md
- ESP-IDF v5.5.4: https://github.com/espressif/esp-idf/releases/tag/v5.5.4

---

# 27. Status Project yang Sudah Diuji

```text
Firmware      : Xiaozhi v2.2.3
MCU           : ESP32-S3-WROOM-1 N16R8
Flash         : 16 MB
PSRAM         : 8 MB
ESP-IDF       : 5.5.4
Language      : Indonesian
OLED          : SSD1306 128x64
Microphone    : INMP441
Amplifier     : MAX98357A
Temperature   : DS18B20
Music         : DFPlayer Mini
Relay 1       : GPIO18 → Kipas
Relay 2       : GPIO8  → Lampu
Relay logic   : Active HIGH
```

Pengujian fisik berhasil:

```text
✓ Build
✓ Flash
✓ Boot
✓ OLED
✓ Logo ARSIP
✓ Bahasa Indonesia
✓ Wi-Fi
✓ Server Xiaozhi
✓ Voice command
✓ Kipas
✓ Lampu
✓ Audio output
```

---

# 28. Roadmap

- [ ] Kontrol relay tambahan
- [ ] Sensor tambahan
- [ ] Jadwal otomatis Kipas/Lampu
- [ ] Database lagu lokal
- [ ] Pencarian lagu berdasarkan nama
- [ ] microSD langsung ke ESP32
- [ ] MP3 decoder software
- [ ] Status perangkat lebih detail
- [ ] PCB custom
- [ ] Enclosure
- [ ] Firmware release siap flash
- [ ] OTA untuk board custom

---

# 29. Atribusi

Project ini merupakan pengembangan/customisasi dari project open-source **Xiaozhi ESP32**.

Customisasi project ini meliputi:

- konfigurasi ESP32-S3 N16R8
- GPIO mapping
- kontrol Relay Kipas/Lampu
- DS18B20
- DFPlayer Mini
- MCP tools internal
- Bahasa Indonesia
- startup logo ARSIP
- penyesuaian board custom

Hormati lisensi dan atribusi project upstream.

---

# 🎯 Kesimpulan

Project ini menunjukkan bahwa Xiaozhi ESP32 dapat dikembangkan menjadi asisten suara Bahasa Indonesia yang juga mampu mengontrol perangkat fisik.

Contoh:

```text
"Xiaozhi, nyalakan kipas."
              ↓
          AI memahami
              ↓
       MCP self.relay.set
              ↓
           Relay 1
              ↓
         GPIO18 HIGH
              ↓
            KIPAS
```

Dan:

```text
"Xiaozhi, nyalakan lampu."
              ↓
          AI memahami
              ↓
       MCP self.relay.set
              ↓
           Relay 2
              ↓
          GPIO8 HIGH
              ↓
           LAMPU
```

Dengan pendekatan ini, ESP32-S3 tidak hanya menjadi perangkat chat suara, tetapi juga dapat menjadi **pengendali perangkat fisik berbasis bahasa alami**.
