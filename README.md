# STM32 & ADS1115 High-Speed Data Logger

Bu proje, Prof. Dr. Mehmet Akif Erişmiş ile yürütülen çalışmalar kapsamında; STM32F411 mikrodenetleyicisi ve ADS1115 (16-bit ADC) kullanılarak geliştirilmiş, donanım limitlerinde (860 SPS) çalışan bir veri toplama (DAQ) sistemidir.

## 🚀 Öne Çıkan Özellikler

* **Maksimum Örnekleme Hızı:** I2C sürücüleri optimize edilerek ADS1115 donanımsal tepe noktası olan **860 SPS** hızına çıkarılmıştır.
* **Döngüsel Tampon (Circular Buffer):** Veri kaybını önlemek için C dilinde 4096 elemanlı float tabanlı döngüsel tampon mimarisi uygulanmıştır.
* **Hızlı Haberleşme:** UART arayüzü **921.600 Baud** hızına yapılandırılarak PC tarafında darboğaz oluşması engellenmiştir.
* **Yazılımsal Örnekleme:** Kullanıcının 1-860 SPS arasında dilediği hızda veri alabilmesi için float hassasiyetinde zamanlama algoritması geliştirilmiştir.
* **Tetikleme Desteği:** Kare dalga sinyalleri ile senkronize kayıt için EXTI (External Interrupt) entegrasyonu yapılmıştır.
* **Canlı Python Arayüzü:** Gerçek zamanlı sayaç ve veri görselleştirme sunan, verileri anlık olarak `.txt` formatında kaydeden Python CLI arayüzü sunulmuştur.

## 🛠️ Teknik Detaylar
* **Donanım:** STM32F411 Nucleo, ADS1115 ADC.
* **Protokoller:** I2C (400kHz Fast Mode), UART (921.600 bps).
* **Yazılım:** C (STM32 HAL), Python 3.

## 🔌 Bağlantı Şeması
| ADS1115 | STM32 |
| :--- | :--- |
| VDD/GND | 3.3V / GND |
| SCL/SDA | PB8 / PB9 |
| ADDR | GND (0x48) |
| AIN0 | Analog Giriş |
| Harici Tetik | PA7 (EXTI) |

## 👨‍💻 Geliştirici
Prof. Dr. Mehmet Akif Erişmiş danışmanlığında geliştirilmiştir.
