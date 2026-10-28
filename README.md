# STM32G070CBT6-PRJ

Firmware project for the **STM32G070CBT6** microcontroller. The project is configured with STM32CubeMX and intended for development and building with **Keil MDK-ARM / µVision 5.32**.

## Overview

The current firmware is the STM32 HAL-based initialization scaffold. It initializes the system clock, GPIO, and ADC1; the main application loop is currently empty.

## Hardware configuration

The following PCB pin assignments are fixed and should not be changed without corresponding hardware changes:

| MCU pin | User label | Configuration |
| --- | --- | --- |
| PD0 | System LED | GPIO output, push-pull |
| PD1 | GSM LED | GPIO output, push-pull |
| PA15 | GPS LED | GPIO output, push-pull |
| PA4 | MCU ADC Main Power | ADC1_IN4, analog input |

The LED GPIO outputs are initialized low at startup.

## Clock

- **System clock (SYSCLK):** 64 MHz
- The clock is configured in `SystemClock_Config()` in `Core/Src/main.c`.

## Firmware peripherals

- **GPIO:** Initializes the System, GSM, and GPS LED output pins.
- **ADC1:** Configured for 12-bit resolution, right alignment, software-triggered single conversions, and channel 4 (PA4 / ADC1_IN4).
- **HAL error handling:** Initialization failures are routed to `Error_Handler()`, which disables interrupts and remains in an infinite loop.

> ADC1 is initialized, but application-level ADC sampling and processing have not yet been implemented.

## Repository structure

- `Core/Inc/main.h` — common application definitions and pin mappings.
- `Core/Src/main.c` — startup sequence, clock configuration, GPIO/ADC initialization, and main loop.
- `Core/Src/stm32g0xx_hal_msp.c` — low-level HAL peripheral and GPIO resource initialization.
- `.gitignore` — excludes local Keil settings and generated build artifacts.

## Development workflow

1. Open the project in STM32CubeMX when peripheral or clock configuration needs to be reviewed.
2. Preserve the fixed PCB pin assignments listed above.
3. Generate or update the initialization code from STM32CubeMX.
4. Open the generated Keil project in **Keil µVision 5.32 (MDK-ARM)**, build the firmware, and program the target using your configured debug/programming interface.
5. Place application code in the appropriate `USER CODE` sections when modifying CubeMX-generated source files, so that regeneration is less likely to overwrite custom changes.

The Keil project file and the exact device-programming/debugger setup should be verified against the files and hardware available in your local development environment.

## Status

This repository currently provides the peripheral initialization baseline. Application behavior—such as LED status logic, ADC measurement, and any external-module communication—remains to be implemented.
