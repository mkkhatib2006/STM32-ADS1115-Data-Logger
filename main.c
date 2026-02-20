/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32F411 - 860 SPS - FLOAT PRECISION TIMING
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ads1115.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CIRCULAR_BUFFER_SIZE 4096
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile SystemState_t g_sistem_durumu = STATE_IDLE;
volatile int g_yeni_komut_geldi = 0;

uint8_t g_rx_char[1];
uint8_t g_rx_buffer[64];
volatile uint8_t g_rx_index = 0;

// Ayarlar
int g_calisma_modu = 0;
uint16_t g_istenen_sps = 860;
uint32_t g_sure_ayari_sn = 10;

// Zamanlama
uint32_t g_calisma_baslangic_zamani_ms = 0;
uint32_t g_calisma_suresi_ms = 0;

// --- DÜZELTME BURADA: ZAMANLAYICIYI FLOAT YAPTIK ---
float g_sonraki_hedef_zaman = 0.0f;
// ---------------------------------------------------

// Veri
float g_en_guncel_voltaj = 0.0f;
char tx_buffer[64];

// Tampon
float circular_buffer[CIRCULAR_BUFFER_SIZE];
volatile uint16_t head_index = 0;
volatile uint16_t tail_index = 0;

// EXTI
volatile int g_exti_tetiklendi = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
uint16_t map_sps_to_ads_config(uint16_t sps);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart2, g_rx_char, 1);

  // ADS1115'i Başlat (860 SPS)
  ADS1115_StartContinuous_And_PointToData(&hi2c1, 0x00E0);
  HAL_Delay(10);
  /* USER CODE END 2 */

  static uint32_t last_ready_time = 0;

  while (1)
  {
    /* USER CODE BEGIN 3 */

    // 1. ADC OKU
    float anlik_voltaj;
    if (ADS1115_ReadResultOnly_NoPointer(&hi2c1, &anlik_voltaj) == HAL_OK) {
        g_en_guncel_voltaj = anlik_voltaj;
    }

    // 2. STATE MACHINE
    if (g_sistem_durumu == STATE_IDLE)
    {
       if (g_yeni_komut_geldi == 1)
       {
           g_yeni_komut_geldi = 0;

           int temp_mod = 0, temp_sps = 860, temp_sure = 10;
           if (sscanf((char*)g_rx_buffer, "%d,%d,%d", &temp_mod, &temp_sps, &temp_sure) == 3) {
               g_calisma_modu = temp_mod;
               g_istenen_sps = (uint16_t)temp_sps;
               g_sure_ayari_sn = (uint32_t)temp_sure;

               sprintf(tx_buffer, "DEBUG: Mod=%d, SPS=%d, Sure=%lu\r\n", g_calisma_modu, g_istenen_sps, g_sure_ayari_sn);
               HAL_UART_Transmit(&huart2, (uint8_t*)tx_buffer, strlen(tx_buffer), 100);
           }

           head_index = 0; tail_index = 0;
           g_calisma_suresi_ms = g_sure_ayari_sn * 1000;

           if (g_calisma_modu == 0)
           {
               g_calisma_baslangic_zamani_ms = HAL_GetTick();

               // --- ZAMANLAYICIYI BAŞLAT ---
               // Hedef zaman = Şu an + Periyot
               g_sonraki_hedef_zaman = (float)HAL_GetTick();
               // ----------------------------

               g_sistem_durumu = STATE_RUNNING;
               HAL_UART_Transmit(&huart2, (uint8_t*)"START\r\n", 7, 100);
           }
           else
           {
               g_sistem_durumu = STATE_WAITING;
               HAL_UART_Transmit(&huart2, (uint8_t*)"WAITING_TRIGGER\r\n", 17, 100);
           }

           memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
           g_rx_index = 0;
       }
       else
       {
           if (HAL_GetTick() - last_ready_time >= 1000) {
               HAL_UART_Transmit(&huart2, (uint8_t*)"READY\r\n", 7, 100);
               last_ready_time = HAL_GetTick();
           }
       }
    }

    else if (g_sistem_durumu == STATE_RUNNING)
    {
      uint32_t anlik_zaman = HAL_GetTick();

      if (anlik_zaman - g_calisma_baslangic_zamani_ms >= g_calisma_suresi_ms) {
        g_sistem_durumu = STATE_DONE;
      }

      int kayit_yapilsin_mi = 0;

      // --- MANTIK DÜZELTME KISMI (FLOAT HESAPLAMA) ---
      if (g_calisma_modu == 0)
      {
          // Artık küsüratlı hesap yapıyoruz
          // Örn: 400 SPS için periyot = 2.5
          float periyot_float = 1000.0f / (float)g_istenen_sps;

          // Eğer şu anki zaman, hedef zamanı geçtiyse kayıt yap
          if ((float)anlik_zaman >= g_sonraki_hedef_zaman)
          {
              kayit_yapilsin_mi = 1;
              // Hedefi 2.5ms ileri at (Örn: 2.5 -> 5.0 -> 7.5 ...)
              g_sonraki_hedef_zaman += periyot_float;
          }
      }
      // -----------------------------------------------
      else
      {
          if (g_exti_tetiklendi == 1) {
              g_exti_tetiklendi = 0;
              kayit_yapilsin_mi = 1;
          }
      }

      if (kayit_yapilsin_mi == 1)
      {
          circular_buffer[head_index] = g_en_guncel_voltaj;
          head_index++;
          if (head_index >= CIRCULAR_BUFFER_SIZE) head_index = 0;
      }

      while (tail_index != head_index)
      {
          float veri = circular_buffer[tail_index];
          sprintf(tx_buffer, "%.2f\r\n", veri);
          HAL_UART_Transmit(&huart2, (uint8_t*)tx_buffer, strlen(tx_buffer), 10);
          tail_index++;
          if (tail_index >= CIRCULAR_BUFFER_SIZE) tail_index = 0;
      }
    }

    else if (g_sistem_durumu == STATE_DONE)
    {
      while (tail_index != head_index) {
          float veri = circular_buffer[tail_index];
          sprintf(tx_buffer, "%.2f\r\n", veri);
          HAL_UART_Transmit(&huart2, (uint8_t*)tx_buffer, strlen(tx_buffer), 10);
          tail_index++;
          if (tail_index >= CIRCULAR_BUFFER_SIZE) tail_index = 0;
      }
      HAL_UART_Transmit(&huart2, (uint8_t*)"DONE\r\n", 6, 100);
      g_sistem_durumu = STATE_IDLE;
    }
  }
  /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/* USER CODE BEGIN 4 */
uint16_t map_sps_to_ads_config(uint16_t sps) { return 0; } // Artık kullanılmıyor ama hata vermemesi için kalsın

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2) {
    if (g_rx_char[0] == '\n' || g_rx_char[0] == '\r') {
      if (g_rx_index > 0) {
        g_rx_buffer[g_rx_index] = '\0';
        g_yeni_komut_geldi = 1;
        g_rx_index = 0;
      }
    } else {
      if (g_rx_index < sizeof(g_rx_buffer) - 1)
        g_rx_buffer[g_rx_index++] = g_rx_char[0];
    }
    HAL_UART_Receive_IT(&huart2, g_rx_char, 1);
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_7) {
        if (g_sistem_durumu == STATE_RUNNING || g_sistem_durumu == STATE_WAITING) {
            HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
            if (g_sistem_durumu == STATE_WAITING) {
                extern uint32_t g_calisma_baslangic_zamani_ms;
                extern float g_sonraki_hedef_zaman; // Float değişkeni extern olarak al
                g_calisma_baslangic_zamani_ms = HAL_GetTick();
                g_sonraki_hedef_zaman = (float)HAL_GetTick(); // Zamanlayıcıyı sıfırla
                g_sistem_durumu = STATE_RUNNING;
            }
            g_exti_tetiklendi = 1;
        }
    }
}
/* USER CODE END 4 */

void Error_Handler(void) { __disable_irq(); while (1) {} }
#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
