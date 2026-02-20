# High-Speed ADS1115 Data Logger with STM32 & Python

Bu proje, STM32F411 mikrodenetleyicisi ve harici ADS1115 (16-bit ADC) kullanılarak geliştirilmiş, donanım limitlerinde (860 SPS) çalışan yüksek hassasiyetli bir veri toplama (Data Acquisition - DAQ) sistemidir. Toplanan veriler, gecikmesiz olarak bilgisayar ortamına aktarılır ve Python tabanlı bir terminal arayüzü ile anlık olarak izlenip kaydedilir.

## 🚀 Temel Özellikler

* **Maksimum Örnekleme Hızı (860 SPS):** Standart I2C kütüphanelerindeki engelleyici (blocking) gecikmeler kaldırılarak, ADS1115 "Continuous Mode" (Sürekli Mod) ile donanımsal tepe hızında çalıştırılmıştır.
* **Döngüsel Tampon (Circular Buffer):** Veri darboğazlarını (bottleneck) ve kayıplarını önlemek için RAM üzerinde 4096 elemanlı Float Circular Buffer mimarisi kurulmuştur.
* **Yüksek Hızlı Haberleşme:** STM32 ile PC arasındaki seri haberleşme (UART) hızı, anlık veri akışını kaldırabilmesi için **921.600 Baud Rate** seviyesine çıkarılmıştır.
* **Yazılımsal Örnekleme (Downsampling):** Donanım arka planda sürekli 860 SPS hızında çalışırken, kullanıcının 1-860 SPS arasında dilediği frekansta (Float hassasiyetinde zamanlama ile) veri alabilmesini sağlayan akıllı bir algoritma yazılmıştır.
* **Harici Tetikleme (Hardware Triggering):** Kare dalga vb. harici sinyal jeneratörleri ile senkronize kayıt alabilmek için EXTI (External Interrupt) yapısı entegre edilmiştir.
* **Canlı Python Arayüzü:** Anlık veri akışını ekranda kayma yapmadan (carriage return `\r` ile) gösteren, COM portunu otomatik bulan ve verileri `.txt` olarak kaydeden CLI tabanlı bir Python arayüzü geliştirilmiştir.

## 🛠️ Kullanılan Teknolojiler ve Donanımlar

* **Mikrodenetleyici:** STM32F411RE Nucleo
* **Sensör / ADC:** ADS1115 (16-bit)
* **Geliştirme Ortamı:** STM32CubeIDE, Python 3.x
* **Haberleşme Protokolleri:** I2C (400 kHz Fast Mode), UART (921.600 Baud)

## 🔌 Donanım Bağlantıları (Pinout)

STM32 ve ADS1115 arasındaki bağlantı tablosu:

| ADS1115 Pini | STM32 Pini | Açıklama |
| :--- | :--- | :--- |
| VDD | 3.3V / 5V | Güç Girişi |
| GND | GND | Toprak |
| SCL | PB8 (I2C1_SCL) | I2C Saat Sinyali (400 kHz) |
| SDA | PB9 (I2C1_SDA) | I2C Veri Sinyali |
| ADDR | GND | I2C Adres Seçimi (0x48) |
| AIN0 | - | Ölçülecek Analog Sinyal Girişi |
| **Harici Sinyal** | **PA7 (EXTI)** | Kare Dalga Tetikleme Girişi (Opsiyonel) |

*(Not: En iyi ölçüm kalitesi için I2C kablolarında ve ölçüm uçlarında Twisted Pair - Bükümlü Çift kablo kullanılması tavsiye edilir.)*

## 💻 Kurulum ve Kullanım

### 1. STM32 Tarafı
1.  Bu depodaki `STM32_Firmware` klasörünü STM32CubeIDE ile açın.
2.  Kodu derleyin ve STM32F411 kartınıza yükleyin (Flash).
3.  Kartı bilgisayara USB üzerinden bağlayın.

### 2. Python Tarafı
1.  Python 3'ün sisteminizde kurulu olduğundan emin olun.
2.  Gerekli kütüphaneyi yükleyin:
    ```bash
    pip install pyserial
    ```
3.  Arayüzü çalıştırın:
    ```bash
    python Python_Interface/ads_listener.py
    ```

Arayüz açıldığında STM32'yi otomatik bulacak ve sizden "Zaman Ayarlı" veya "Kare Dalga Tetiklemeli" modlarından birini seçmenizi isteyecektir.

## 👨‍💻 Geliştirici
*Bu proje, Prof. Dr. Mehmet Akif Erişmiş ile birlikte yürütülen çalışmalar kapsamında geliştirilmiştir.*
