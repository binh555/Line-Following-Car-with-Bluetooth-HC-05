# 🚗 STM32 Line Follower Robot

Xe dò line tự động dùng **STM32 Nucleo F401RE**, điều khiển không dây qua **HC-05 Bluetooth**, đọc tốc độ bánh xe bằng **encoder Hall**, và theo dõi đường bằng **5 cảm biến hồng ngoại**. Thuật toán điều khiển sử dụng bộ điều khiển **PD (Proportional-Derivative)**.

---

## Mục lục

- [Tính năng](#tính-năng)
- [Phần cứng](#phần-cứng)
- [Sơ đồ kết nối](#sơ-đồ-kết-nối)
- [Cấu hình phần mềm](#cấu-hình-phần-mềm)
- [Thuật toán điều khiển](#thuật-toán-điều-khiển)
- [Lệnh Bluetooth](#lệnh-bluetooth)
- [Cài đặt và nạp code](#cài-đặt-và-nạp-code)
- [Thông số có thể tinh chỉnh](#thông-số-có-thể-tinh-chỉnh)
- [Debug và theo dõi](#debug-và-theo-dõi)
- [Lịch sử thay đổi](#lịch-sử-thay-đổi)
- [Ghi chú kỹ thuật](#ghi-chú-kỹ-thuật)

---

## Tính năng

- **Chế độ AUTO** — xe tự dò line, điều chỉnh tốc độ hai bánh theo thuật toán PD
- **Chế độ MANUAL** — điều khiển thủ công qua Bluetooth (W/S/A/D), hỗ trợ giữ phím auto-repeat từ terminal
- **Chế độ STOP** — dừng hoàn toàn, tắt STBY driver
- **Dừng tự động khi all-black** — phát hiện vạch kết thúc đường đua (40 chu kỳ ổn định liên tiếp)
- **Báo cáo encoder mỗi 1 giây** — in LeftPPS / RightPPS và trạng thái 5 cảm biến qua cả UART2 (USB debug) và HC-05 (Bluetooth)
- **Ring buffer UART không dùng DMA** — nhận/gửi Bluetooth hoàn toàn bằng interrupt, không blocking
- **Bù bias cơ học** — `LEFT_PWM_BIAS` / `RIGHT_PWM_BIAS` bù chênh lệch tốc độ thực tế hai bánh

---

## Phần cứng

| Linh kiện | Model / Thông số |
|---|---|
| Vi điều khiển | STM32 Nucleo F401RE (STM32F401RET6, 84 MHz) |
| Motor | DC Servo GA12 N20, 12 V, tỉ số truyền 1:150 |
| Bánh xe | Bánh cao su 43 mm đường kính |
| Encoder | Hall sensor tích hợp trong GA12 N20, 7 xung/kênh/vòng motor |
| Motor driver | TB6612FNG |
| Cảm biến line | Cảm biến hồng ngoại 5 mắt TCRT5000 |
| Bluetooth | HC-05 (baudrate 9600, chế độ Slave) |
| Nguồn | Pin 12 V DC |

### Thông số encoder chi tiết

- 7 xung / kênh / vòng **trục motor** (trước hộp giảm tốc)
- Tỉ số 1:150 → **1.050 xung / vòng bánh xe** (mỗi kênh)
- Code hiện dùng **1 kênh rising edge** → PPS = vòng/giây × 1.050
- Nguồn encoder: **3.3 V hoặc 5 V** — tuyệt đối không cấp 12 V

---

## Sơ đồ kết nối

### TB6612FNG ↔ STM32

| TB6612 | STM32 Pin | Mô tả |
|---|---|---|
| STBY | PA6 | Kích hoạt driver (HIGH = ON) |
| AIN1 | PB1 | Hướng motor A (bánh trái) |
| AIN2 | PA8 | Hướng motor A (bánh trái) |
| PWMA | PB10 (TIM2_CH3) | PWM motor A |
| BIN1 | PC7 | Hướng motor B (bánh phải) |
| BIN2 | PB6 | Hướng motor B (bánh phải) |
| PWMB | PA7 (TIM3_CH2) | PWM motor B |
| VM | 12 V | Nguồn motor |
| VCC | 3.3 V | Nguồn logic |
| GND | GND | Mass chung |

### Cảm biến hồng ngoại (5 mắt)

| Cảm biến | STM32 Pin | Vị trí vật lý |
|---|---|---|
| S0 (ngoài trái) | PA0 | Trái nhất |
| S1 | PA1 | |
| S2 (giữa) | PA4 | Trung tâm |
| S3 | PB0 | |
| S4 (ngoài phải) | PC1 | Phải nhất |

> Logic: `0` = không có line, `1` = phát hiện line đen (active LOW, cảm biến kéo xuống GND khi gặp đen)

### Encoder

| Tín hiệu | STM32 Pin | Ngắt |
|---|---|---|
| Encoder trái (kênh A) | PA15 | EXTI15 (rising edge) |
| Encoder phải (kênh A) | PB5 | EXTI9\_5 (rising edge) |

### HC-05 Bluetooth

| HC-05 | STM32 Pin | UART |
|---|---|---|
| TXD | PA10 (USART1_RX) | USART1 |
| RXD | PA9 (USART1_TX) | USART1 |
| VCC | 5 V | |
| GND | GND | |

### UART2 Debug (USB → MobaXterm)

| Tín hiệu | STM32 Pin |
|---|---|
| TX | PA2 (USART2_TX) |
| RX | PA3 (USART2_RX) |

---

## Cấu hình phần mềm

### Clock

```
HSI 16 MHz → PLL → SYSCLK = 84 MHz
APB1 = 42 MHz  (TIM2, TIM3 timer clock = 84 MHz do nhân đôi)
APB2 = 84 MHz  (USART1)
```

### PWM

```
Timer: TIM2 (bánh trái, CH3 - PB10) | TIM3 (bánh phải, CH2 - PA7)
Prescaler = 3  →  clock vào timer = 84 MHz / 4 = 21 MHz
Period    = 1499
PWM frequency = 21 MHz / 1500 ≈ 14 kHz
PWM_MAX (tick) = 1499  →  duty 100% = 12 V
PWM_MIN_RUN    = 250   →  duty ~16.7% (ngưỡng motor bắt đầu quay)
```

### UART

| UART | Dùng cho | Baudrate | Chế độ |
|---|---|---|---|
| USART1 | HC-05 Bluetooth | 9600 | Interrupt TX + Interrupt RX |
| USART2 | Debug qua USB | 115200 | Blocking TX |

---

## Thuật toán điều khiển

### Tính vị trí line (weighted average)

5 cảm biến được đánh trọng số vị trí 0, 1000, 2000, 3000, 4000:

```
position = Σ(i × 1000 × sensor[i]) / Σ(sensor[i])
```

Vị trí trung tâm = **2000**. Nếu không có cảm biến nào active → chạy chế độ tìm kiếm (`searchLine`).

### Lọc nhiễu

```c
filteredPosition = 0.45 × filteredPosition + 0.55 × rawPosition
```

### Bộ điều khiển PD

```
error      = (filteredPosition - 2000) / 1000        // đơn vị hoá -2..+2
dError     = error - lastError
correction = KP × error + KD × dError
correction = clamp(correction, -MAX_CORRECTION, +MAX_CORRECTION)

leftSpeed  = BASE_SPEED + correction
rightSpeed = BASE_SPEED - correction
```

Cả hai bánh đều được clamp trong `[MIN_AUTO_SPEED, MAX_SPEED]`.

### Xử lý góc cua gắt

Nếu `filteredPosition ≤ 500` hoặc `≥ 3500` (line gần sát mép ngoài):

```
filteredPosition ≤ 500  →  left = 30%,  right = MAX_SPEED  (rẽ trái gắt)
filteredPosition ≥ 3500 →  left = MAX_SPEED, right = 30%   (rẽ phải gắt)
```

### Tìm kiếm line khi mất

```
lastSeenSide < 0  →  quay trái  (left = -SEARCH_REVERSE, right = +SEARCH_FORWARD)
lastSeenSide > 0  →  quay phải  (left = +SEARCH_FORWARD, right = -SEARCH_REVERSE)
không rõ          →  tiến thẳng chậm (30%)
```
