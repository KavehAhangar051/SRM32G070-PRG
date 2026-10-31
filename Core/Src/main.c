/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/**
 * @brief وضعیت‌های مختلف سطح ولتاژ تغذیه
 */
typedef enum {
    POWER_STATE_CRITICAL = 0,
    POWER_STATE_LOW,
    POWER_STATE_MEDIUM,
    POWER_STATE_FULL
} PowerState_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/*
 * متغیرهای عمومی کنترل روشنایی LEDها (Software PWM):
 * - کلمه کلیدی volatile به کامپایلر اعلام می‌کند این متغیرها ممکن است درون روتین وقفه (ISR)
 *   تغییر کنند تا کامپایلر مقدار آن‌ها را کش نکند و بهینه‌سازی اشتباه انجام ندهد.
 */

/* درصد روشنایی فعال فعلی در مقیاس ۰ تا ۲۵۵ که توسط وقفه خوانده می‌شود */
volatile uint8_t g_pwm_duty = 0;

/* پرچم فعال‌سازی مود بحرانی:
 * مقدار 1: وارد وضعیت ولتاژ بحرانی شدیم و وقفه تایمر ۱ باید ال‌ای‌دی‌ها را با PWM کم‌نور و پرنور کند.
 * مقدار 0: وضعیت ولتاژ عادی است و وقفه تایمر ۱ در کار ال‌ای‌دی‌ها دخالتی نمی‌کند.
 */
volatile uint8_t g_critical_mode = 0;

/*
 * جدول تصحیح گامای چشم انسان (CIE 1931 Lightness Curve):
 * چشم انسان روشنایی را خطی نمی‌بیند (در تاریکی تغییرات کوچک نور را خیلی بزرگ می‌بیند).
 * اگر درصد نور را به صورت عادی زیاد کنیم ال‌ای‌دی چشمک می‌زند.
 * این آرایه ۶۴ تایی دارای اعداد مهندسی‌شده است تا تغییر روشنایی کاملاً نرم، سینمایی و بدون پرپر زدن به نظر برسد.
 */
static const uint8_t BREATH_TABLE[64] = {
    0,   0,   1,   1,   2,   2,   3,   3,   4,   5,   5,   6,   7,   8,   9,  10,
   12,  13,  14,  16,  18,  20,  22,  24,  26,  28,  31,  33,  36,  39,  42,  45,
   49,  52,  56,  60,  64,  68,  73,  77,  82,  87,  92,  98, 103, 109, 115, 122,
  128, 135, 142, 149, 156, 164, 172, 180, 189, 197, 206, 215, 225, 235, 245, 255
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */
float Get_Power_Voltage(void);
void Update_Power_LEDs(float voltage);
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
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
  /*
   * ۱. کالیبراسیون خودکار واحد ADC1 قبل از شروع اندازه‌گیری:
   * این تابع خطای آفست داخلی خازن‌های آنالوگ به دیجیتال میکروکنترلر را در لحظه استارت صفر می‌کند.
   */
  HAL_ADCEx_Calibration_Start(&hadc1);

  /*
   * ۲. راه‌اندازی تایمر ۱ در حالت وقفه دوره‌ای (Interrupt Mode):
   * این تابع تایمر ۱ را روشن کرده و اجازه می‌دهد هر ۱۰۰ میکروثانیه تابع HAL_TIM_PeriodElapsedCallback صدا زده شود.
   */
  HAL_TIM_Base_Start_IT(&htim1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /*
     * گام ۱: خواندن ولتاژ خط ورودی اصلی مدار (به کمک ADC1، میانگین ۱۶ تایی و فیلتر نرم‌افزاری EMA)
     */
    float voltage = Get_Power_Voltage();

    /*
     * گام ۲: به روزرسانی وضعیت چراغ‌های نشانگر بر اساس ولتاژ خوانده‌شده و حاشیه امن هیسترسیز
     */
    Update_Power_LEDs(voltage);

    /*
     * گام ۳: ایجاد فاصله ۵۰ میلی‌ثانیه‌ای میان هر پایش:
     * نرخ تکرار حلقه برابر ۲۰ بار در ثانیه (20Hz) تنظیم می‌شود که مانع از مصرف بیهوده توان پردازنده می‌گردد.
     */
    HAL_Delay(50);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_39CYCLES_5;
  hadc1.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_39CYCLES_5;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x00100D14;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 399;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0|GREEN_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BLUE_LED_Pin */
  GPIO_InitStruct.Pin = BLUE_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BLUE_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PD0 GREEN_LED_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GREEN_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
 * @brief  تابع پاسخگویی به وقفه سرریز تایمر ۱ (تولید Software PWM با سرعت بسیار بالا)
 *
 * @note   این تابع در سخت‌افزار با فرکانس ۱۰ کیلوهرتز (دقیقاً هر ۱۰۰ میکروثانیه یک‌بار)
 *         بدون معطل کردن حلقه اصلی برنامه (while) به صورت خودکار فراخوانی می‌شود.
 *
 * @param  htim اشاره‌گر به ساختار مدیریت تایمر فعال‌شده
 * @retval ندارد
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* بررسی این‌که آیا این وقفه متعلق به تایمر ۱ است یا خیر */
  if (htim->Instance == TIM1)
  {
    /* فقط در صورتی که ولتاژ مدار به زیر ۱۰.۸ ولت افت کرده باشد وارد کنترل PWM می‌شویم */
    if (g_critical_mode)
    {
      /*
       * شمارنده پله‌های PWM از ۰ تا ۲۵۵:
       * متغیر استاتیک مقدار قبلی خود را در هر بار خروج از تابع حفظ می‌کند.
       * با توجه به اندازه ۸ بیتی (uint8_t)، این متغیر بعد از ۲۵۵ به طور خودکار دوباره صفر می‌شود.
       */
      static uint8_t pwm_cnt = 0;
      pwm_cnt++;

      /*
       * مقایسه زمان روشن بودن (Duty Cycle):
       * اگر شمارنده کمتر از مقدار روشنایی درخواستی باشد، پین‌ها روشن می‌شوند.
       * اگر شمارنده بیشتر شود، پین‌ها تا پایان این دوره خاموش می‌مانند.
       */
      if (pwm_cnt < g_pwm_duty)
      {
        /*
         * پالس در سطح بالا (روشن کردن چراغ‌ها):
         * به جای استفاده از تابع کند HAL_GPIO_WritePin، مستقیماً از رجیسترهای سخت‌افزاری BSRR استفاده می‌کنیم.
         * رجیستر BSRR (Bit Set/Reset Register) پین‌ها را در ۱ سیکل کلاک (چند نانوثانیه) روشن می‌کند.
         * - پایه PA15: ال‌ای‌دی آبی
         * - پایه‌های PD0 و PD1: ال‌ای‌دی‌های قرمز و سبز به صورت همزمان با عملگر OR (|)
         */
        GPIOA->BSRR = BLUE_LED_Pin;
        GPIOD->BSRR = (RED_LED_Pin | GREEN_LED_Pin);
      }
      else
      {
        /*
         * پالس در سطح پایین (خاموش کردن چراغ‌ها):
         * رجیستر سخت‌افزاری BRR (Bit Reset Register) پین‌ها را به طور آنی صفر (GND) می‌کند.
         */
        GPIOA->BRR = BLUE_LED_Pin;
        GPIOD->BRR = (RED_LED_Pin | GREEN_LED_Pin);
      }
    }
  }
}

/**
 * @brief  نمونه‌برداری، تبدیل واحد و فیلتر دیجیتال ولتاژ تغذیه اصلی مدار
 *
 * @details روند کار این تابع:
 *          ۱. ۱۶ نمونه متوالی از ورودی آنالوگ کانال ۴ (پایه PA4) قرائت می‌کند.
 *          ۲. مقدار عددی دیجیتال (۰ تا ۴۰۹۵) را به ولتاژ واقعی پایه میکرو (۰ تا ۳.۳ ولت) تبدیل می‌نماید.
 *          ۳. با ضرب در ضریب ۱۱.۷ مقسم ولتاژ مقاومتی مدار، ولتاژ قبل از مقسم را محاسبه می‌کند.
 *          ۴. از فیلتر پایین‌گذر دیجیتال بازگشتی نمایی (EMA) استفاده می‌کند تا نویز و نوسانات لحظه‌ای حذف شوند.
 *
 * @param  ندارد
 * @return float مقدار نهایی و تصفیه‌شده ولتاژ منبع تغذیه به ولت (V)
 */
float Get_Power_Voltage(void)
{
  const uint8_t NUM_SAMPLES = 16;  /* تعداد دفعات نمونه‌برداری جهت میانگین‌گیری دیجیتال */
  uint32_t adc_accum = 0;          /* متغیر تجمیع مقادیر برای جمع زدن ۱۶ نمونه */
  static float filtered_voltage = 0.0f; /* مقدار ذخیره‌شده ولتاژ فیلترشده در طول اجرای برنامه */
  static uint8_t is_initialized = 0;    /* پرچم نشان‌دهنده نمونه‌برداری اولیه در لحظه روشن شدن برد */

  /* حلقه خواندن ۱۶ نمونه پی‌درپی */
  for (uint8_t i = 0; i < NUM_SAMPLES; i++)
  {
    /* صدور دستور سخت‌افزاری شروع تبدیل آنالوگ به دیجیتال */
    HAL_ADC_Start(&hadc1);

    /* صبر به مدت حداکثر ۵ میلی‌ثانیه تا اتمام تبدیل واحد ADC */
    if (HAL_ADC_PollForConversion(&hadc1, 5) == HAL_OK)
    {
      /* خواندن عدد دیجیتال ۱۲ بیتی (بازه ۰ تا ۴۰۹۵) و اضافه کردن به مجموع */
      adc_accum += HAL_ADC_GetValue(&hadc1);
    }
  }

  /* اتمام تبدیل و متوقف کردن واحد ADC جهت کاهش مصرف توان الکتریکی */
  HAL_ADC_Stop(&hadc1);

  /* محاسبه میانگین حسابی ۱۶ نمونه برداشته‌شده */
  float avg_raw = (float)adc_accum / (float)NUM_SAMPLES;

  /*
   * تبدیل عدد دیجیتال به ولتاژ پایه پین میکروکنترلر:
   * فرمول: V_pin = (ADC_Value * 3.3V) / 4095
   * ولتاژ مرجع (Vref) برابر ۳.۳ ولت و رزولوشن ADC برابر ۱۲ بیت (2^12 - 1 = 4095) است.
   */
  float pin_voltage = (avg_raw * 3.3f) / 4095.0f;

  /*
   * محاسبه ولتاژ خط ورودی اصلی قبل از مقسم مقاومتی مدار:
   * در شماتیک مدار، ولتاژ ورودی با دو مقاومت تقسیم شده تا به بازه امن میکرو (زیر ۳.۳ ولت) برسد.
   * ضریب معکوس این مقسم ولتاژ برابر ۱۱.۷ است.
   */
  float instant_voltage = pin_voltage * 11.7f;

  /*
   * فیلتر دیجیتال نمایی (Exponential Moving Average):
   * فرمول: V_filtered = (0.15 * V_instant) + (0.85 * V_filtered_prev)
   * هدف: جلوگیری از پرش‌های تصادفی ولتاژ بر اثر نویز برق و بار لحظه‌ای مدار
   */
  if (!is_initialized)
  {
    /* در اولین بار پس از روشن شدن میکرو، ولتاژ لحظه‌ای را مستقیماً ثبت می‌کنیم تا تاخیر نداشته باشد */
    filtered_voltage = instant_voltage;
    is_initialized = 1;
  }
  else
  {
    /* در دفعات بعدی، ۸۵ درصد از داده باثبات قبلی و ۱۵ درصد از داده جدید ترکیب می‌شود */
    filtered_voltage = (0.15f * instant_voltage) + (0.85f * filtered_voltage);
  }

  return filtered_voltage;
}

/**
 * @brief  کنترل وضعیت نشانگرهای LED بر اساس سطح ولتاژ تغذیه با حاشیه امن هیسترسیز و افکت تنفس
 *
 * @details منطق رفتاری ال‌ای‌دی‌ها در مدار:
 *          ۱. حالت Full (ولتاژ بالای ۱۵V): هر سه ال‌ای‌دی آبی، قرمز و سبز کاملاً روشن هستند.
 *          ۲. حالت Medium (ولتاژ بین ۱۳V تا ۱۵V): آبی و قرمز روشن، سبز خاموش است.
 *          ۳. حالت Low (ولتاژ بین ۱۱V تا ۱۳V): فقط آبی روشن، بقیه خاموش هستند.
 *          ۴. حالت Critical (ولتاژ زیر ۱۱V): هر سه ال‌ای‌دی با تکنیک PWM افکت تنفس نرم می‌گیرند.
 *
 * @param  voltage مقدار ولتاژ پالایش‌شده منبع تغذیه (بر حسب ولت)
 * @retval ندارد
 */
void Update_Power_LEDs(float voltage)
{
  /* متغیر استاتیک برای نگهداری وضعیت فعلی مدار حتی پس از خروج از تابع */
  static PowerState_t state = POWER_STATE_FULL;

  /*
   * ---------------------------------------------------------------------------------
   * گام اول: تصمیم‌گیری درباره وضعیت باتری با در نظر گرفتن منطق هیسترسیز (Hysteresis)
   * دلیل استفاده از هیسترسیز:
   * برای جلوگیری از پرپر زدن ال‌ای‌دی‌ها وقتی ولتاژ دقیقاً روی مرز (مثلاً ۱۵.۰ ولت) است.
   * مرز ورود به هر حالت با مرز خروج از آن ۰.۲ ولت فاصله دارد.
   * ---------------------------------------------------------------------------------
   */
  if (state == POWER_STATE_FULL)
  {
    /* فقط اگر ولتاژ از ۱۴.۸ ولت کمتر شد، از حالت پر به حالت متوسط سقوط می‌کنیم */
    if (voltage < 14.8f)
    {
      state = POWER_STATE_MEDIUM;
    }
  }
  else if (state == POWER_STATE_MEDIUM)
  {
    /* اگر ولتاژ با شارژ شدن مجدد به بالای ۱۵.۰ ولت رسید، به حالت پر برمی‌گردیم */
    if (voltage >= 15.0f)
    {
      state = POWER_STATE_FULL;
    }
    /* اگر افت ولتاژ ادامه داشت و از ۱۲.۸ ولت کمتر شد، به حالت ضعیف می‌رویم */
    else if (voltage < 12.8f)
    {
      state = POWER_STATE_LOW;
    }
  }
  else if (state == POWER_STATE_LOW)
  {
    /* اگر ولتاژ به ۱۳.۰ ولت افزایش یافت، به حالت متوسط برمی‌گردیم */
    if (voltage >= 13.0f)
    {
      state = POWER_STATE_MEDIUM;
    }
    /* اگر افت ولتاژ شدید شد و به زیر ۱۰.۸ ولت رسید، به وضعیت بحرانی می‌رویم */
    else if (voltage < 10.8f)
    {
      state = POWER_STATE_CRITICAL;
    }
  }
  else if (state == POWER_STATE_CRITICAL)
  {
    /* خروج از حالت بحرانی فقط در صورتی رخ می‌دهد که ولتاژ منبع تغذیه به بالای ۱۱.۰ ولت بازگردد */
    if (voltage >= 11.0f)
    {
      state = POWER_STATE_LOW;
    }
  }

  /*
   * ---------------------------------------------------------------------------------
   * گام دوم: اعمال خروجی‌های سخت‌افزاری بر روی پایه‌های ال‌ای‌دی‌ها
   * ---------------------------------------------------------------------------------
   */
  if (state == POWER_STATE_FULL)
  {
    /* غیرفعال کردن مد تنفس تایمر ۱ */
    g_critical_mode = 0;

    /* روشن کردن همزمان هر ۳ چراغ (آبی GPS، قرمز System، سبز GSM) */
    HAL_GPIO_WritePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_SET);
  }
  else if (state == POWER_STATE_MEDIUM)
  {
    /* غیرفعال کردن مد تنفس تایمر ۱ */
    g_critical_mode = 0;

    /* روشن ماندن آبی و قرمز / خاموش شدن سبز */
    HAL_GPIO_WritePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_RESET);
  }
  else if (state == POWER_STATE_LOW)
  {
    /* غیرفعال کردن مد تنفس تایمر ۱ */
    g_critical_mode = 0;

    /* تنها چراغ آبی روشن می‌ماند / قرمز و سبز خاموش می‌شوند */
    HAL_GPIO_WritePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_RESET);
  }
  else if (state == POWER_STATE_CRITICAL)
  {
    /*
     * فعال‌سازی مد PWM نرم‌افزاری در وقفه تایمر ۱:
     * متغیر پرچم برابر ۱ می‌شود تا تابع وقفه تایمر ۱ شروع به سوئیچینگ سریع پایه‌ها کند.
     */
    g_critical_mode = 1;

    /*
     * پایش زمان برای تغییر تدریجی و پله‌ای نور (افکت تنفس یا Breathing):
     * هر ۲۵ میلی‌ثانیه یک‌بار، یک گام در جدول تصحیح گاما جلو یا عقب می‌رویم.
     */
    static uint32_t last_step_time = 0; /* آخرین زمان تغییر گام بر حسب میلی‌ثانیه */
    static uint8_t step_index = 0;      /* اندیس گام فعلی از ۰ تا ۶۳ در جدول گاما */
    static int8_t step_dir = 1;         /* جهت حرکت نور: +1 در حال پرنور شدن / -1 در حال کم‌نور شدن */

    /* بررسی این‌که آیا ۲۵ میلی‌ثانیه از گام قبلی گذشته است یا خیر (روش غیرمسدودکننده Non-blocking) */
    if ((HAL_GetTick() - last_step_time) >= 25)
    {
      /* ثبت زمان فعلی برای گام بعدی */
      last_step_time = HAL_GetTick();

      /* استخراج میزان روشنایی متناسب با منحنی چشم از جدول گاما */
      g_pwm_duty = BREATH_TABLE[step_index];

      /* اگر در حال افزایش نور هستیم */
      if (step_dir == 1)
      {
        if (step_index >= 63)
        {
          /* به ۱۰۰٪ روشنایی رسیدیم؛ اکنون جهت را برعکس می‌کنیم تا نور کم شود */
          step_dir = -1;
        }
        else
        {
          step_index++; /* یک پله افزایش نور */
        }
      }
      /* اگر در حال کاهش نور هستیم */
      else
      {
        if (step_index <= 0)
        {
          /* به خاموشی کامل رسیدیم؛ اکنون جهت را برعکس می‌کنیم تا نور مجدداً زیاد شود */
          step_dir = 1;
        }
        else
        {
          step_index--; /* یک پله کاهش نور */
        }
      }
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
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
