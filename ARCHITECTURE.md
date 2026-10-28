# Architecture

## 1. Scope and current state

This document describes the firmware architecture that is present in the repository today, not a proposed or future design.

The project targets the **STM32G070CBT6** and uses the STM32 Hardware Abstraction Layer (HAL). The current application is an initialization baseline: it configures the system clock, GPIO outputs, and ADC1, then enters an empty infinite loop. No application-level state machine, scheduler, module driver, or sensor-processing layer is currently implemented.

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
        |
        v
    MX_ADC1_Init()
        |
        v
    while (1)  -- currently empty
```

- `HAL_Init()` initializes the HAL and its tick infrastructure.
- `SystemClock_Config()` configures the voltage scaling, HSE and PLL, then applies the system and bus clock settings.
- `MX_GPIO_Init()` configures the three LED signals as push-pull outputs and initializes their output levels low.
- `MX_ADC1_Init()` configures ADC1 and its regular channel.
- Initialization errors call `Error_Handler()`, which disables interrupts and loops indefinitely.

## 3. Clock configuration

The current implementation in `SystemClock_Config()` uses:

- HSE enabled as the PLL input.
- PLLM = DIV1, PLLN = 16, PLLR = DIV2.
- SYSCLK source = PLLCLK.
- AHB and APB1 prescalers = DIV1.
- Flash latency = 2 wait states.

The HAL configuration defines `HSE_VALUE` as 8 MHz, so these settings produce a nominal **64 MHz SYSCLK/HCLK/PCLK1** when the physical HSE source matches that value. If the board's external oscillator differs, the HSE definition and clock setup must be reconciled with the hardware.

## 4. Hardware interface and fixed pin assignments

These pin assignments are part of the PCB interface and must not be reassigned without checking the hardware design.

| MCU pin | Signal / user label | Current mode | Initialization |
| --- | --- | --- | --- |
| PD0 | `System_LED` | GPIO output, push-pull, no pull | Low |
| PD1 | `GSM_LED` | GPIO output, push-pull, no pull | Low |
| PA15 | `GPS_LED` | GPIO output, push-pull, no pull | Low |
| PA4 | `MCU_ADC_Main_Power` | Analog input, ADC1_IN4 | Configured by ADC MSP initialization |

The pin macros are declared in `Core/Inc/main.h`. GPIO port clocks for GPIOA and GPIOD are enabled by `MX_GPIO_Init()`; GPIOF's clock is also enabled there, although no GPIOF pin is currently configured in that function.

## 5. ADC1 data path

ADC1 is initialized in `MX_ADC1_Init()` in `Core/Src/main.c`. The low-level peripheral and pin setup is in `HAL_ADC_MspInit()` in `Core/Src/stm32g0xx_hal_msp.c`.

Current ADC settings:

- Input: channel 4, mapped to PA4 / `MCU_ADC_Main_Power`.
- Resolution: 12-bit, right-aligned.
- Conversion: one regular conversion, software-triggered.
- Continuous conversion: disabled.
- DMA requests and oversampling: disabled.
- ADC clock prescaler: synchronous PCLK divided by 2.
- Sampling time: common setting 1, configured to 1.5 ADC cycles.

At a 64 MHz PCLK1, the configured synchronous ADC clock is nominally 32 MHz. Confirm that the actual clock source, ADC limits, input signal source impedance, and sampling time meet the device datasheet and board requirements.

**Important:** the firmware currently configures ADC1 but does not start conversions, read conversion values, convert raw readings to engineering units, or implement a measurement/control algorithm. The signal label alone does not define the electrical meaning or scaling of the sampled voltage.

## 6. Source responsibilities

| File | Responsibility |
| --- | --- |
| `Core/Src/main.c` | Application entry point, clock configuration, GPIO/ADC initialization, error handler, and main loop. |
| `Core/Inc/main.h` | HAL include, shared pin macros, and `Error_Handler()` declaration. |
| `Core/Src/stm32g0xx_hal_msp.c` | HAL MSP setup; enables ADC clock and configures PA4 as analog for ADC1. |
| `Core/Src/stm32g0xx_it.c` | Cortex-M exception handlers and SysTick handler, which increments the HAL tick. |
| `Core/Inc/stm32g0xx_it.h` | Interrupt handler declarations. |
| `Core/Inc/stm32g0xx_hal_conf.h` | HAL module selection and clock-related configuration constants, including `HSE_VALUE`. |
| `Core/Src/system_stm32g0xx.c` | CMSIS system initialization and core-clock support. |
| `Drivers/STM32G0xx_HAL_Driver/` | ST-provided STM32G0 HAL drivers. |
| `Drivers/CMSIS/` | CMSIS core and device support, where included in the repository. |

The repository files available for inspection did not include an STM32CubeMX `.ioc` file or a Keil `.uvprojx` project file at the expected root/subdirectory paths. Verify the actual local project files before relying on those paths or assuming how the project is built.

## 7. Error and interrupt behavior

- HAL initialization/configuration failures route to `Error_Handler()`; its current behavior is to disable interrupts and stop in an infinite loop.
- `HardFault_Handler()` and `NMI_Handler()` also stop in infinite loops.
- `SysTick_Handler()` calls `HAL_IncTick()`.
- No ADC interrupt handler, DMA transfer path, or application-specific interrupt processing is currently implemented.

## 8. Extension boundaries

When application functionality is added, keep hardware initialization separate from application behavior:

1. Preserve the existing PCB pin mappings and clock requirements.
2. Put ADC sampling and conversion logic in a clearly named application module when it grows beyond a small, local operation.
3. Keep LED status policy separate from low-level GPIO initialization.
4. Add peripheral drivers only for interfaces actually present in the hardware and required by the application.
5. Keep custom code inside the appropriate `USER CODE` regions when editing CubeMX-generated files, and verify generated changes before accepting them.

These are implementation guidelines for future work; they do not describe modules that already exist.

## 9. Build and verification

The project is intended to use STM32CubeMX for peripheral configuration/code generation and Keil MDK-ARM / µVision 5.32 for development. The repository snapshot inspected here did not expose the expected `.ioc` and `.uvprojx` files, so the exact build target, compiler settings, startup-file selection, and debugger configuration could not be verified from those files.

After firmware changes, build with the actual local Keil project and inspect compiler/linker output. Hardware validation should confirm the 64 MHz clock, LED output polarity and behavior, and ADC readings against a known input voltage.
