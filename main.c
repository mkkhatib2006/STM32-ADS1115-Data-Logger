/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
// SystemState_t enum yapısını main.h dosyasına taşıdığınızı varsayıyorum.
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

// --- Sistem Durumu ---
volatile SystemState_t g_sistem_durumu = STATE_IDLE;
volatile int g_yeni_komut_geldi = 0;

// --- UART Alma Tamponu ---
uint8_t g_rx_char[1];
uint8_t g_rx_buffer[32];
uint8_t g_rx_index = 0;

// --- Ayarlar ---
// 0: Sürekli (SPS hızıyla), 1: Kare Dalga (Tetiklemeyle)
// Varsayılan olarak 0 yaptık ki hata olursa Sürekli modda çalışsın.
volatile int g_calisma_modu = 0;
volatile uint16_t g_sps_ayari = 128;
volatile uint32_t g_sure_ayari_sn = 10;

// --- Zamanlama ---
uint32_t g_calisma_baslangic_zamani_ms = 0;
uint32_t g_calisma_suresi_ms = 0;

// --- ADC ve Tetikleme ---
float okunan_voltaj_mv;
HAL_StatusTypeDef ads_durumu;
char tx_buffer[64];

// EXTI (main.c altındaki callback) içinden set edilecek bayrak
volatile int g_adc_tetiklendi = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

uint16_t map_sps_to_ads_config(uint16_t sps);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  // UART Interrupt Başlat
  HAL_UART_Receive_IT(&huart1, g_rx_char, 1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  static uint32_t last_ready_time = 0;

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // --- DURUM 1: IDLE (Komut Bekleme) ---
    if (g_sistem_durumu == STATE_IDLE)
    {
       if (g_yeni_komut_geldi == 1)
       {
           g_yeni_komut_geldi = 0;

           // 1. ADC Başlat
           uint16_t ads_sps_config = map_sps_to_ads_config(g_sps_ayari);
           ads_durumu = ADS1115_Init(&hi2c1, ads_sps_config, ADS1115_PGA_ONE);

           if (ads_durumu != HAL_OK) {
               HAL_UART_Transmit(&huart1, (uint8_t*)"ERROR: ADS_INIT\r\n", 17, 100);
           }
           else {
               // Süre ayarını kaydet
               g_calisma_suresi_ms = g_sure_ayari_sn * 1000;

               // --- MOD AYRIMI ---
               if (g_calisma_modu == 0)
               {
                   // MOD 0: SÜREKLİ OKUMA
                   g_calisma_baslangic_zamani_ms = HAL_GetTick();
                   g_sistem_durumu = STATE_RUNNING;

                   sprintf(tx_buffer, "START: %lu saniye, %hu SPS (Surekli)\r\n", g_sure_ayari_sn, g_sps_ayari);
                   HAL_UART_Transmit(&huart1, (uint8_t*)tx_buffer, strlen(tx_buffer), 100);
               }
               else
               {
                   // MOD 1: KARE DALGA TETİKLEME
                   g_sistem_durumu = STATE_WAITING;
                   HAL_UART_Transmit(&huart1, (uint8_t*)"WAITING_TRIGGER\r\n", 17, 100);
               }
           }
       }
       else
       {
           uint32_t anlik_zaman = HAL_GetTick();
           if (anlik_zaman - last_ready_time >= 1000) {
               HAL_UART_Transmit(&huart1, (uint8_t*)"READY\r\n", 7, 100);
               last_ready_time = anlik_zaman;
           }
       }
    }


    // --- DURUM 3: RUNNING (Ölçüm Anı) ---
    else if (g_sistem_durumu == STATE_RUNNING)
    {
      uint32_t anlik_zaman = HAL_GetTick();

      if (anlik_zaman - g_calisma_baslangic_zamani_ms >= g_calisma_suresi_ms)
      {
        g_sistem_durumu = STATE_DONE;
      }
      else
      {
         int okuma_izni = 0;

         if (g_calisma_modu == 0) {
             // Mod 0 (Sürekli): Her zaman oku
             okuma_izni = 1;
         }
         else {
             // Mod 1 (Kare Dalga): Sadece interrupt bayrağı kalktıysa oku
             if (g_adc_tetiklendi == 1) {
                 g_adc_tetiklendi = 0; // Bayrağı indir
                 okuma_izni = 1;
             }
         }

         if (okuma_izni == 1)
         {
             ads_durumu = ADS1115_readSingleEnded(ADS1115_MUX_AIN0, &okunan_voltaj_mv);
             if (ads_durumu == HAL_OK) {
                 // Tamsayı gönder
                 sprintf(tx_buffer, "%ld\r\n", (int32_t)(okunan_voltaj_mv * 100.0f));
                 HAL_UART_Transmit(&huart1, (uint8_t*)tx_buffer, strlen(tx_buffer), 100);
             }
             else {
                 HAL_UART_Transmit(&huart1, (uint8_t*)"READ_ERROR\r\n", 14, 100);
             }
         }
      }
    }

    // --- DURUM 4: DONE (Bitiş) ---
    else if (g_sistem_durumu == STATE_DONE)
    {
      HAL_UART_Transmit(&huart1, (uint8_t*)"DONE\r\n", 6, 100);
      g_sistem_durumu = STATE_IDLE;
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00503D58;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LED1_Pin */
  GPIO_InitStruct.Pin = LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED1_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

uint16_t map_sps_to_ads_config(uint16_t sps)
{
  switch(sps) {
    case 8:   return ADS1115_DATA_RATE_8;
    case 16:  return ADS1115_DATA_RATE_16;
    case 32:  return ADS1115_DATA_RATE_32;
    case 64:  return ADS1115_DATA_RATE_64;
    case 128: return ADS1115_DATA_RATE_128;
    case 250: return ADS1115_DATA_RATE_250;
    case 475: return ADS1115_DATA_RATE_475;
    case 860: return ADS1115_DATA_RATE_860;
    default:  return ADS1115_DATA_RATE_128;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    if (g_rx_char[0] == '\n' || g_rx_char[0] == '\r')
    {
      if (g_rx_index > 0)
      {
        g_rx_buffer[g_rx_index] = '\0';

        // --- 3 DE�?İ�?KEN OKU: MOD, SPS, SÜRE ---
        sscanf((char*)g_rx_buffer, "%d,%hu,%lu", &g_calisma_modu, &g_sps_ayari, &g_sure_ayari_sn);
        // -------------------------------------

        g_yeni_komut_geldi = 1;
        g_rx_index = 0;
      }
    }
    else
    {
      if (g_rx_index < sizeof(g_rx_buffer) - 1)
        g_rx_buffer[g_rx_index++] = g_rx_char[0];
    }
    HAL_UART_Receive_IT(&huart1, g_rx_char, 1);
  }
}

// --- KESME (INTERRUPT) CALLBACK ---
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_7)
    {
        if (g_sistem_durumu == STATE_RUNNING)
        {
            // LED'i yakıp söndür (Görsel Kontrol)
            HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
            g_adc_tetiklendi = 1;
        }
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
