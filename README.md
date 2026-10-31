# STM32G070CBT6-PRJ

Firmware project for the **STM32G070CBT6** microcontroller. The project is configured with STM32CubeMX and intended for development and building with **Keil MDK-ARM / µVision 5.32**.

## Overview

Firmware implementation for the STM32G070CBT6 MCU featuring:
- Power supply voltage monitoring using internal ADC1 (Channel 4 on PA4) with moving average filtering and calibrated conversion.
- Battery/power status visualization via multi-color LED indicators (System/Red, GSM/Green, GPS/Blue).
- Peripheral drivers configured for USART2 serial communication and TIM1 base timer with interrupt handling.

## Hardware Configuration

Pin assignments configured for the target PCB:

| MCU Pin | User Label / Signal | Configuration | Description / Initial State |
| :--- | :--- | :--- | :--- |
| **PA15** | `BLUE_LED` (GPS LED) | GPIO Output, Push-Pull | Blue status indicator (Init: Low) |
| **PD0** | `RED_LED` (System LED) | GPIO Output, Push-Pull | Red status indicator (Init: Low) |
| **PD1** | `GREEN_LED` (GSM LED) | GPIO Output, Push-Pull | Green status indicator (Init: Low) |
| **PA4** | `MCU_ADC_Main_Power` | ADC1_IN4, Analog Input | Main power voltage sensing line |
| **PA2** | `Serial_Bus_TX` | USART2 TX, Alternate Function | Serial communication transmit |
| **PA3** | `Serial_Bus_RX` | USART2 RX, Alternate Function | Serial communication receive |

## Clock Configuration

- **Clock Source:** Internal HSI oscillator (16 MHz)
- **SYSCLK / HCLK / PCLK1:** 16 MHz
- Configured in `SystemClock_Config()` in `Core/Src/main.c`.

## Firmware Peripherals & Features

- **ADC1:** 12-bit resolution, right-aligned, software-triggered single conversion on channel 4 with 39.5 cycles sampling time (`ADC_SAMPLETIME_39CYCLES_5`). Self-calibration performed at startup (`HAL_ADCEx_Calibration_Start`).
- **USART2:** 115200 baud, 8 data bits, 1 stop bit, no parity, interrupt enabled in NVIC.
- **TIM1:** 16-bit timer base with periodic update interrupt enabled (`HAL_TIM_Base_Start_IT`), Prescaler = 999, Period = 3999.
- **Power Voltage Monitoring & Hysteresis (`Get_Power_Voltage`, `Update_Power_LEDs`):**
  - Samples ADC channel 4 sixteen times per cycle with hardware conversion start per sample.
  - Converts average digital reading to MCU pin voltage ($V_{pin} = \text{ADC} \times 3.3 / 4095$).
  - Multiplies by resistor divider ratio factor ($\approx 11.7$) to determine battery/main power rail voltage.
  - Applies Exponential Moving Average (EMA) low-pass filter ($\alpha = 0.15$) to eliminate noise and jitter.
  - Controls LED indicators using a state machine with **$0.2\,\text{V}$ hysteresis deadband** to prevent threshold chatter:
    - **Full Power ($\ge 15.0\,\text{V}$, drops at $< 14.8\,\text{V}$):** Blue, Red, Green all ON.
    - **Medium Power ($13.0\,\text{V} - 15.0\,\text{V}$, drops at $< 12.8\,\text{V}$):** Blue & Red ON, Green OFF.
    - **Low Power ($11.0\,\text{V} - 13.0\,\text{V}$, drops at $< 10.8\,\text{V}$):** Blue ON, Red & Green OFF.
    - **Critical Low Power ($< 11.0\,\text{V}$, rises at $\ge 11.0\,\text{V}$):** All LEDs toggling non-blockingly every 500 ms.

## Repository Structure

- `SRM32G070CBT6.ioc` — STM32CubeMX peripheral & pin configuration project file.
- `MDK-ARM/SRM32G070CBT6.uvprojx` — Keil MDK-ARM µVision project file.
- `Core/Inc/main.h` — Common pin macro definitions and application exports.
- `Core/Src/main.c` — Startup sequence, clock configuration, peripheral initialization, power monitoring, and LED state machine.
- `Core/Src/stm32g0xx_it.c` — Interrupt service routines for ADC1, TIM1, and USART2.
- `Core/Src/stm32g0xx_hal_msp.c` — Low-level MSP hardware initialization (GPIO, clock gating, NVIC).

## Development Workflow

1. Open `SRM32G070CBT6.ioc` in STM32CubeMX when peripheral or clock configuration needs to be modified.
2. Preserve the fixed PCB pin assignments listed above.
3. Generate code from STM32CubeMX.
4. Open the generated Keil project in **Keil µVision 5.32 (MDK-ARM)**, build the firmware, and program the target.
5. Place application code in the appropriate `USER CODE` sections when modifying CubeMX-generated source files so that code generation does not overwrite custom code.
