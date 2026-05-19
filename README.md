# 🚗 STM32 Line Follower Robot

An automatic line-following car using the STM32 Nucleo F401RE, wireless control via HC-05 Bluetooth, wheel speed measurement with Hall encoders, and line tracking with 5 infrared sensors. The control algorithm uses a PD (Proportional-Derivative) controller.

<!-- car photo -->
<img width="1920" height="2560" alt="xe2" src="https://github.com/user-attachments/assets/cd0d2844-94d0-4f89-8171-23c87f3ed8c6" />
<img width="1920" height="2560" alt="xe1" src="https://github.com/user-attachments/assets/f10fdf91-c032-4efe-802e-c00e83b104ef" />

---

## Table of Contents

- [Features](#features)
- [Hardware](#hardware)
- [Wiring Diagram](#wiring-diagram)
- [Software Configuration](#software-configuration)
- [Control Algorithm](#control-algorithm)
- [Bluetooth Commands](#bluetooth-commands)
- [Technical Notes](#technical-notes)

---

## Features

- **AUTO mode** — the car follows the line automatically and adjusts the speed of both wheels using a PD controller
- **MANUAL mode** — manual control via Bluetooth (W/S/A/D), supports key auto-repeat from terminal software
- **STOP mode** — complete stop, disables the driver STBY pin
- **Auto stop on all-black** — detects the finish line after 40 consecutive stable cycles
- **Encoder report every 1 second** — prints LeftPPS / RightPPS and the status of 5 sensors through both UART2 (USB debug) and HC-05 (Bluetooth)
- **UART ring buffer without DMA** — Bluetooth RX/TX is fully interrupt-driven and non-blocking
- **Mechanical bias compensation** — `LEFT_PWM_BIAS` / `RIGHT_PWM_BIAS` compensate for real speed differences between the two wheels

---

## Hardware

| Component | Model / Specification |
|---|---|
| Microcontroller | STM32 Nucleo F401RE (STM32F401RET6, 84 MHz) |
| Motor | DC Servo GA12 N20, 12 V, gear ratio 1:150 |
| Wheels | 43 mm rubber wheels |
| Encoder | Hall sensor integrated in GA12 N20, 7 pulses/channel/motor revolution |
| Motor driver | TB6612FNG (VM ≤ 15 V, Iout ≤ 1.2 A continuous) |
| Line sensor | 5 infrared sensors (active LOW on black line) |
| Bluetooth | HC-05 (baud rate 9600, Slave mode) |
| Power supply | 12 V DC battery |

**Detailed encoder specifications:**
- 7 pulses / channel / motor shaft revolution (before gearbox)
- Gear ratio 1:150 → 1,050 pulses / wheel revolution (per channel)
- The current code uses 1 channel with rising edge detection → PPS = revolutions/second × 1,050
- Encoder supply: 3.3 V or 5 V — **never use 12 V**

---

## Wiring Diagram

### TB6612FNG ↔ STM32

| TB6612 | STM32 Pin | Description |
|---|---|---|
| STBY | PA6 | Driver enable (HIGH = ON) |
| AIN1 | PB1 | Motor A direction (left wheel) |
| AIN2 | PA8 | Motor A direction (left wheel) |
| PWMA | PB10 (TIM2_CH3) | Motor A PWM |
| BIN1 | PC7 | Motor B direction (right wheel) |
| BIN2 | PB6 | Motor B direction (right wheel) |
| PWMB | PA7 (TIM3_CH2) | Motor B PWM |
| VM | 12 V | Motor power |
| VCC | 3.3 V | Logic power |
| GND | GND | Common ground |

### Infrared sensors (5 sensors)

| Sensor | STM32 Pin | Physical position |
|---|---|---|
| S0 (far left) | PA0 | Leftmost |
| S1 | PA1 | |
| S2 (center) | PA4 | Center |
| S3 | PB0 | |
| S4 (far right) | PC1 | Rightmost |

> Logic: `0` = no line, `1` = black line detected (active LOW, sensor pulls down to GND on black)

### Encoder

| Signal | STM32 Pin | Interrupt |
|---|---|---|
| Left encoder (channel A) | PA15 | EXTI15 (rising edge) |
| Right encoder (channel A) | PB5 | EXTI9_5 (rising edge) |

### HC-05 Bluetooth

| HC-05 | STM32 Pin | UART |
|---|---|---|
| TXD | PA10 (USART1_RX) | USART1 |
| RXD | PA9 (USART1_TX) | USART1 |
| VCC | 5 V | |
| GND | GND | |

### UART2 Debug (USB → MobaXterm)

| Signal | STM32 Pin |
|---|---|
| TX | PA2 (USART2_TX) |
| RX | PA3 (USART2_RX) |

---

## Software Configuration

### Clock

```txt
HSI 16 MHz → PLL → SYSCLK = 84 MHz
APB1 = 42 MHz  (TIM2, TIM3 timer clock = 84 MHz due to timer x2 multiplier)
APB2 = 84 MHz  (USART1)
```

### PWM

| Parameter | Value |
|---|---|
| Timer | TIM2 (left wheel, CH3 - PB10) \| TIM3 (right wheel, CH2 - PA7) |
| Prescaler | 3 → timer input clock = 84 MHz / 4 = 21 MHz |
| Period | 1499 |
| PWM frequency | 21 MHz / 1500 ≈ 14 kHz |
| PWM_MAX (tick) | 1499 → 100% duty = 12 V |
| PWM_MIN_RUN | 250 → duty ~16.7% (minimum threshold for motor startup) |

### UART

| UART | Used for | Baud rate | Mode |
|---|---|---|---|
| USART1 | HC-05 Bluetooth | 9600 | Interrupt TX + Interrupt RX |
| USART2 | USB debug | 115200 | Blocking TX |

---

## Control Algorithm

### Line position calculation (weighted average)

The 5 sensors are assigned position weights of 0, 1000, 2000, 3000, and 4000:

```txt
position = Σ(i × 1000 × sensor[i]) / Σ(sensor[i])
```

The center position is 2000. If no sensor is active, the system switches to line-search mode (`searchLine`).

### Noise filtering

```txt
filteredPosition = 0.45 × filteredPosition + 0.55 × rawPosition
```

### PD controller

```c
error      = (filteredPosition - 2000) / 1000        // normalized unit: -2..+2
dError     = error - lastError
correction = KP × error + KD × dError
correction = clamp(correction, -MAX_CORRECTION, +MAX_CORRECTION)

leftSpeed  = BASE_SPEED + correction
rightSpeed = BASE_SPEED - correction
```

Both wheels are clamped within `[MIN_AUTO_SPEED, MAX_SPEED]`.

### Sharp turn handling

If `filteredPosition ≤ 500` or `≥ 3500` (line is near the outer edge):

- `filteredPosition ≤ 500` → left = 30%, right = MAX_SPEED *(sharp left turn)*
- `filteredPosition ≥ 3500` → left = MAX_SPEED, right = 30% *(sharp right turn)*

### Line search when lost

- `lastSeenSide < 0` → turn left (left = -SEARCH_REVERSE, right = +SEARCH_FORWARD)
- `lastSeenSide > 0` → turn right (left = +SEARCH_FORWARD, right = -SEARCH_REVERSE)
- unknown → move slowly forward (30%)

### Stop on all-black (finish line)

If all 5 sensors continuously detect the line for **40 control cycles** (40 × 3 ms = 120 ms):
1. Switch to `CAR_MODE_STOP`
2. Disable STBY
3. Print a message through both UART2 and Bluetooth

---

## Bluetooth Commands

Connect to HC-05 using MobaXterm or any serial terminal (baud rate 9600).

### Mode commands

| Command | Function |
|---|---|
| `U` | Switch to AUTO mode — line following |
| `M` | Switch to MANUAL mode — manual control |
| `X` | STOP — stop the car and disable the driver |
| `H` | Print HELP — list of commands |
| `T` | Print STATUS — current mode, position, PWM, sensors |

### MANUAL commands *(only active in MANUAL mode)*

| Command | Function | Auto-stop timeout |
|---|---|---|
| `W` | Forward | 250 ms after the last character |
| `S` | Backward | 500 ms after the last character |
| `A` | Turn left | 120 ms after the last character |
| `D` | Turn right | 120 ms after the last character |

> **Timeout note:** Terminal software does not send a signal when the key is released. A quick press = the car moves in one short pulse and then stops automatically. Holding the key = the terminal auto-repeats the character, so the car keeps moving until the key is released.

Commands are case-insensitive (`w` = `W`).

---

## Technical Notes

### Mechanical speed limit (1:150 motor)

The GA12 N20 motor with a 1:150 gear ratio at 12 V no-load typically reaches about 100–150 RPM after the gearbox. With 43 mm wheels:

```txt
Theoretical maximum speed ≈ π × 0.043 m × 150 RPM / 60 ≈ 0.34 m/s
```

This is the physical upper limit. If higher speed is needed, use a motor with a smaller gear ratio such as 1:50 or 1:30.

### Bluetooth ring buffer

Both Bluetooth TX and RX use interrupt-driven ring buffers:
- RX: `BT_RX_BUF_SIZE = 64` bytes — enough for 1-character commands
- TX: `BT_TX_BUF_SIZE = 256` bytes — enough for one full report line
- Non-blocking — the `main()` loop never stalls waiting for UART

### Interrupt priority

| Interrupt | Priority | Reason |
|---|---|---|
| USART1 (HC-05) | 0 (highest) | Prevent Bluetooth byte loss |
| EXTI15_10 (left encoder) | 1 | Accurate pulse counting |
| EXTI9_5 (right encoder) | 1 | Accurate pulse counting |

### Notes when using MobaXterm / serial terminal

The terminal does not send a separate byte when a key is released, so MANUAL mode uses timeout-based auto-stop. If the car feels jerky during manual control, adjust these constants:

```c
#define MANUAL_FWD_RELEASE_TIMEOUT_MS   250U
#define MANUAL_BWD_RELEASE_TIMEOUT_MS   500U
#define MANUAL_TURN_RELEASE_TIMEOUT_MS  120U
```

Increase the timeout → the car moves longer after each key press.  
Decrease the timeout → the car stops faster after key release.

---

## License

MIT License — free to use, modify, and distribute.
