/*
 * ads1115.c
 * Tam ve Güncellenmiş Sürüm (F411 & High Speed)
 */

#include "ads1115.h"
#include "string.h" // memcpy için

// --- Global Değişkenler (Eski Fonksiyonlar İçin) ---
// Kare dalga modu bu değişkenleri kullanır
I2C_HandleTypeDef *ADS1115_I2C_Handler_ptr;
uint8_t ADS1115_devAddress = 0x48; // ADDR -> GND
uint8_t ADS1115_config[2];
uint8_t ADS1115_rawValue[2];
float ADS1115_voltCoef = 0.125f; // Varsayılan 4.096V aralığı katsayısı
uint16_t ADS1115_pga = ADS1115_PGA_ONE;
uint16_t ADS1115_dataRate = ADS1115_DATA_RATE_128;

// --- SABİTLER ---
#define REG_CONVERSION 0x00
#define REG_CONFIG     0x01

// =============================================================
// 1. BÖLÜM: YENİ TURBO FONKSİYONLAR (Sürekli Mod İçin)
// =============================================================

// Bu fonksiyon ayarları gönderir ve ibreyi veriye kilitler (Sadece başta 1 kez çalışır)
HAL_StatusTypeDef ADS1115_StartContinuous_And_PointToData(I2C_HandleTypeDef *hi2c, uint16_t rate) {
    uint8_t config_data[3];

    // Config Register Adresi
    config_data[0] = REG_CONFIG;

    // AYARLAR:
    // MSB: OS(0) | MUX(100-AIN0) | PGA(001-4.096V) | MODE(0-Continuous) -> 0x42
    // LSB: DR(rate) | COMP(Disabled-0003)

    // Eğer 4.096V dışında bir aralık isterseniz 0x4200 kısmını değiştirmelisiniz.
    // Şimdilik varsayılan olarak +/- 4.096V kullanıyoruz.
    uint16_t confWord = 0x4200 | rate | 0x0003;

    config_data[1] = (uint8_t)(confWord >> 8);
    config_data[2] = (uint8_t)(confWord & 0xFF);

    // 1. Ayarları Yaz
    if (HAL_I2C_Master_Transmit(hi2c, (ADS1115_devAddress << 1), config_data, 3, 100) != HAL_OK) {
        return HAL_ERROR;
    }

    // 2. İbreyi Sonuç Register'ına (0x00) Çevir
    uint8_t ptr = REG_CONVERSION;
    return HAL_I2C_Master_Transmit(hi2c, (ADS1115_devAddress << 1), &ptr, 1, 100);
}

// Bu fonksiyon adres sormadan direkt okur (Döngüde sürekli çağrılır)
HAL_StatusTypeDef ADS1115_ReadResultOnly_NoPointer(I2C_HandleTypeDef *hi2c, float *voltage) {
    uint8_t rawData[2];

    // Sadece RECEIVE komutu (Transmit yok, bu yüzden çok hızlı)
    if (HAL_I2C_Master_Receive(hi2c, (ADS1115_devAddress << 1), rawData, 2, 10) == HAL_OK) {
        int16_t val = (rawData[0] << 8) | rawData[1];

        // 4.096V aralığı için katsayı: 0.125mV
        *voltage = (float)val * 0.125f;
        return HAL_OK;
    }
    return HAL_ERROR;
}


// =============================================================
// 2. BÖLÜM: ESKİ FONKSİYONLAR (Kare Dalga ve Init İçin)
// =============================================================

HAL_StatusTypeDef ADS1115_Init(I2C_HandleTypeDef *handler, uint16_t setDataRate, uint16_t setPGA) {
    ADS1115_I2C_Handler_ptr = handler;
    ADS1115_dataRate = setDataRate;
    ADS1115_pga = setPGA;

    // Katsayıyı ayarla
    switch (ADS1115_pga) {
        case ADS1115_PGA_TWOTHIRDS: ADS1115_voltCoef = 0.1875f; break;
        case ADS1115_PGA_ONE:       ADS1115_voltCoef = 0.125f;  break;
        case ADS1115_PGA_TWO:       ADS1115_voltCoef = 0.0625f; break;
        case ADS1115_PGA_FOUR:      ADS1115_voltCoef = 0.03125f; break;
        case ADS1115_PGA_EIGHT:     ADS1115_voltCoef = 0.015625f; break;
        case ADS1115_PGA_SIXTEEN:   ADS1115_voltCoef = 0.0078125f; break;
        default:                    ADS1115_voltCoef = 0.125f; break;
    }

    // Cihaz hazır mı kontrol et
    if (HAL_I2C_IsDeviceReady(ADS1115_I2C_Handler_ptr, (uint16_t)(ADS1115_devAddress << 1), 5, 100) == HAL_OK) {
        return HAL_OK;
    } else {
        return HAL_ERROR;
    }
}

HAL_StatusTypeDef ADS1115_readSingleEnded(uint16_t muxPort, float *voltage) {
    // Single-Shot Mod Ayarları
    ADS1115_config[0] = ADS1115_OS | muxPort | ADS1115_pga | ADS1115_MODE;
    ADS1115_config[1] = ADS1115_dataRate | ADS1115_COMP_MODE | ADS1115_COMP_POL | ADS1115_COMP_LAT | ADS1115_COMP_QUE;

    // 1. Konfigürasyonu Yaz
    if(HAL_I2C_Mem_Write(ADS1115_I2C_Handler_ptr, (uint16_t)(ADS1115_devAddress << 1), REG_CONFIG, 1, ADS1115_config, 2, 100) != HAL_OK) {
        return HAL_ERROR;
    }

    // 2. Dönüşümün bitmesini bekle (Basit Delay yöntemi - Kare dalga modu yavaş olduğu için sorun olmaz)
    // 860 SPS için yaklaşık 2ms, daha yavaşlar için daha fazla beklemek gerekir.
    // Garanti olsun diye 2ms bekliyoruz.
    HAL_Delay(2);

    // 3. Veriyi Oku
    if(HAL_I2C_Mem_Read(ADS1115_I2C_Handler_ptr, (uint16_t)((ADS1115_devAddress << 1) | 0x1), REG_CONVERSION, 1, ADS1115_rawValue, 2, 100) == HAL_OK)
    {
        int16_t val = (ADS1115_rawValue[0] << 8) | ADS1115_rawValue[1];
        *voltage = (float)val * ADS1115_voltCoef;
        return HAL_OK;
    }

    return HAL_ERROR;
}
