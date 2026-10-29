# Architecture

## 1. Scope and current state

This document describes the firmware architecture that is present in the repository today.

The project targets the **STM32G070CBT6** and uses the STM32 Hardware Abstraction Layer (HAL). The current application initializes system clock, GPIO outputs, ADC1 with calibration, TIM1 base timer with interrupts, and USART2 communication. In the main loop, it continuously monitors the main power voltage through ADC1 and updates the multi-color LED status indicator accordingly.

## 2. Runtime startup flow

The entry point is `main()` in `Core/Src/main.c`.

```text
Reset / startup code
        |
        v
     main()
        |
        v
     HAL_Init()
        |
        v
 SystemClock_Config()
        |
        v
     MX_GPIO_Init()
     MX_ADC1_Init()
     MX_USART2_UART_Init()
     MX_TIM1_Init()
        |
        v
  HAL_ADCEx_Calibration_Start(&hadc1)
  HAL_TIM_Base_Start_IT(&htim1)
        |
        v
     while (1)
        |
        +--> Get_Power_Voltage()
        +--> Update LED states (Full, Medium, Low, Critical Low)
```

- `HAL_Init()` initializes HAL and SysTick infrastructure.
- `SystemClock_Config()` configures the internal HSI oscillator (16 MHz) as the system clock source.
- `MX_GPIO_Init()` configures the 3 LED signals (`BLUE_LED`, `RED_LED`, `GREEN_LED`) as push-pull outputs.
- `MX_ADC1_Init()` configures ADC1 for analog sampling on channel 4.
- `MX_USART2_UART_Init()` configures USART2 at 115200 baud (8N1).
- `MX_TIM1_Init()` configures TIM1 counter with prescaler and period for periodic updates.
- ADC calibration (`HAL_ADCEx_Calibration_Start`) is executed before any conversions.
- TIM1 interrupt base is started (`HAL_TIM_Base_Start_IT`).

## 3. Clock configuration

The current implementation in `SystemClock_Config()` uses:

- Internal HSI oscillator enabled (`RCC_HSI_ON`, 16 MHz).
- PLL is bypassed/disabled (`RCC_PLL_NONE`).
- SYSCLK source = HSI (16 MHz).
- AHB and APB1 prescalers = DIV1.
- Flash latency = 0 wait states.

Resulting bus clocks: **16 MHz SYSCLK / HCLK / PCLK1**.

## 4. Hardware interface and pin assignments

Pin assignments configured for the target PCB:

| MCU pin | Signal / user label | Current mode | Initialization |
| :--- | :--- | :--- | :--- |
| PA15 | `BLUE_LED` (GPS LED) | GPIO output, push-pull, no pull | Low |
| PD0  | `RED_LED` (System LED) | GPIO output, push-pull, no pull | Low |
| PD1  | `GREEN_LED` (GSM LED) | GPIO output, push-pull, no pull | Low |
| PA4  | `MCU_ADC_Main_Power` | Analog input, ADC1_IN4 | Configured by ADC MSP |
| PA2  | `Serial_Bus_TX` | USART2 TX, Alternate Function | Configured by UART MSP |
| PA3  | `Serial_Bus_RX` | USART2 RX, Alternate Function | Configured by UART MSP |

The pin macros are declared in `Core/Inc/main.h`.

## 5. ADC1 and Power Monitoring data path

ADC1 is initialized in `MX_ADC1_Init()` and calibrated on boot via `HAL_ADCEx_Calibration_Start(&hadc1)`.

### Settings:
- Input: Channel 4 (mapped to PA4 / `MCU_ADC_Main_Power`).
- Resolution: 12-bit, right-aligned (0 to 4095).
- Conversion: Software-triggered single conversion.
- Prescaler: Asynchronous DIV2.
- Sampling time: 1.5 ADC cycles (`ADC_SAMPLETIME_1CYCLE_5`).

### Conversion Algorithm (`Get_Power_Voltage`):
1. Starts conversion with `HAL_ADC_Start(&hadc1)`.
2. Takes 20 consecutive samples using `HAL_ADC_PollForConversion(&hadc1, 10)` and `HAL_ADC_GetValue(&hadc1)`.
3. Converts each raw digital reading to voltage at the MCU pin ($V_{pin} = \text{ADC} \times 3.3 / 4095$).
4. Computes the average pin voltage across the 20 samples (`Avrage_ACD_Val`).
5. Applies voltage divider scaling factor ($\approx 11.7$) to estimate input supply rail voltage:
   $$V_{supply} = V_{pin\_avg} \times 11.7$$

### Power State Decision Logic:
- **Full Power ($\ge 15.0\text{ V}$):** Blue LED = ON, Red LED = ON, Green LED = ON.
- **Medium Power ($13.0\text{ V} \le V < 15.0\text{ V}$):** Blue LED = ON, Red LED = ON, Green LED = OFF.
- **Low Power ($11.0\text{ V} \le V \le 13.0\text{ V}$):** Blue LED = ON, Red LED = OFF, Green LED = OFF.
- **Critical Low Power ($< 11.0\text{ V}$):** Blue, Red, and Green LEDs all toggle every 500 ms.

## 6. Interrupt handlers

Implemented in `Core/Src/stm32g0xx_it.c`:
- `SysTick_Handler()`: Handles HAL tick increments.
- `ADC1_IRQHandler()`: Routes to `HAL_ADC_IRQHandler(&hadc1)`.
- `TIM1_BRK_UP_TRG_COM_IRQHandler()` & `TIM1_CC_IRQHandler()`: Route to `HAL_TIM_IRQHandler(&htim1)`.
- `USART2_IRQHandler()`: Routes to `HAL_UART_IRQHandler(&huart2)`.

## 7. Source responsibilities

| File | Responsibility |
| :--- | :--- |
| `Core/Src/main.c` | Entry point, clock config, peripheral inits, `Get_Power_Voltage()`, and main LED control loop. |
| `Core/Inc/main.h` | Shared pin definitions (`BLUE_LED`, `RED_LED`, `GREEN_LED`, `Serial_Bus_TX`, etc.) and prototypes. |
| `Core/Src/stm32g0xx_it.c` | Interrupt handlers for SysTick, ADC1, TIM1, and USART2. |
| `Core/Src/stm32g0xx_hal_msp.c` | HAL MSP layer configuring GPIOs, clocks, and NVIC interrupts for ADC1, TIM1, and USART2. |
| `Core/Src/system_stm32g0xx.c` | CMSIS system clock support. |
| `SRM32G070CBT6.ioc` | CubeMX configuration file containing clock tree, pinout, and IP settings. |
| `MDK-ARM/SRM32G070CBT6.uvprojx` | Keil MDK project configuration. |
