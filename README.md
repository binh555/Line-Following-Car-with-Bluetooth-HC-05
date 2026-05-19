# 🚗 STM32 Line Follower Robot

Xe dò line tự động dùng STM32 Nucleo F401RE, điều khiển không dây qua HC-05 Bluetooth, đọc tốc độ bánh xe bằng encoder Hall, và theo dõi đường bằng 5 cảm biến hồng ngoại. Thuật toán điều khiển sử dụng bộ điều khiển PD (Proportional-Derivative).

<!-- ảnh chụp xe -->
<img width="1920" height="2560" alt="xe2" src="https://github.com/user-attachments/assets/cd0d2844-94d0-4f89-8171-23c87f3ed8c6" />
<img width="1920" height="2560" alt="xe1" src="https://github.com/user-attachments/assets/f10fdf91-c032-4efe-802e-c00e83b104ef" />
<!-- clip xe chạy dò line -->

https://github.com/user-attachments/assets/6d994322-eb4f-472c-b479-9fedd5353642


***

## Mục lục

- [Tính năng](#tính-năng)
- [Phần cứng](#phần-cứng)
- [Sơ đồ kết nối](#sơ-đồ-kết-nối)
- [Cấu hình phần mềm](#cấu-hình-phần-mềm)
- [Thuật toán điều khiển](#thuật-toán-điều-khiển)
- [Lệnh Bluetooth](#lệnh-bluetooth)
- [Ghi chú kỹ thuật](#ghi-chú-kỹ-thuật)

***

## Tính năng

- **Chế độ AUTO** — xe tự dò line, điều chỉnh tốc độ hai bánh theo thuật toán PD
- **Chế độ MANUAL** — điều khiển thủ công qua Bluetooth (W/S/A/D), hỗ trợ giữ phím auto-repeat từ terminal
- **Chế độ STOP** — dừng hoàn toàn, tắt STBY driver
- **Dừng tự động khi all-black** — phát hiện vạch kết thúc đường đua (40 chu kỳ ổn định liên tiếp)
- **Báo cáo encoder mỗi 1 giây** — in LeftPPS / RightPPS và trạng thái 5 cảm biến qua cả UART2 (USB debug) và HC-05 (Bluetooth)
- **Ring buffer UART không dùng DMA** — nhận/gửi Bluetooth hoàn toàn bằng interrupt, không blocking
- **Bù bias cơ học** — `LEFT_PWM_BIAS` / `RIGHT_PWM_BIAS` bù chênh lệch tốc độ thực tế hai bánh

***

## Phần cứng

| Linh kiện | Model / Thông số |
|---|---|
| Vi điều khiển | STM32 Nucleo F401RE (STM32F401RET6, 84 MHz) |
| Motor | DC Servo GA12 N20, 12 V, tỉ số truyền 1:150 |
| Bánh xe | Bánh cao su 43 mm đường kính |
| Encoder | Hall sensor tích hợp trong GA12 N20, 7 xung/kênh/vòng motor |
| Motor driver | TB6612FNG (VM ≤ 15 V, Iout ≤ 1.2 A liên tục) |
| Cảm biến line | 5 cảm biến hồng ngoại (active LOW khi gặp line đen) |
| Bluetooth | HC-05 (baudrate 9600, chế độ Slave) |
| Nguồn | Pin 12 V DC |

**Thông số encoder chi tiết:**
- 7 xung / kênh / vòng trục motor (trước hộp giảm tốc)
- Tỉ số 1:150 → 1.050 xung / vòng bánh xe (mỗi kênh)
- Code hiện dùng 1 kênh rising edge → PPS = vòng/giây × 1.050
- Nguồn encoder: 3.3 V hoặc 5 V — **tuyệt đối không cấp 12 V**

***

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
| Encoder phải (kênh A) | PB5 | EXTI9_5 (rising edge) |

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

***

## Cấu hình phần mềm

### Clock

```
HSI 16 MHz → PLL → SYSCLK = 84 MHz
APB1 = 42 MHz  (TIM2, TIM3 timer clock = 84 MHz do nhân đôi)
APB2 = 84 MHz  (USART1)
```

### PWM

| Tham số | Giá trị |
|---|---|
| Timer | TIM2 (bánh trái, CH3 - PB10) \| TIM3 (bánh phải, CH2 - PA7) |
| Prescaler | 3 → clock vào timer = 84 MHz / 4 = 21 MHz |
| Period | 1499 |
| PWM frequency | 21 MHz / 1500 ≈ 14 kHz |
| PWM_MAX (tick) | 1499 → duty 100% = 12 V |
| PWM_MIN_RUN | 250 → duty ~16.7% (ngưỡng motor bắt đầu quay) |

### UART

| UART | Dùng cho | Baudrate | Chế độ |
|---|---|---|---|
| USART1 | HC-05 Bluetooth | 9600 | Interrupt TX + Interrupt RX |
| USART2 | Debug qua USB | 115200 | Blocking TX |

***

## Thuật toán điều khiển

### Tính vị trí line (weighted average)

5 cảm biến được đánh trọng số vị trí 0, 1000, 2000, 3000, 4000:

```
position = Σ(i × 1000 × sensor[i]) / Σ(sensor[i])
```

Vị trí trung tâm = 2000. Nếu không có cảm biến nào active → chạy chế độ tìm kiếm (`searchLine`).

### Lọc nhiễu

```
filteredPosition = 0.45 × filteredPosition + 0.55 × rawPosition
```

### Bộ điều khiển PD

```c
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

- `filteredPosition ≤ 500` → left = 30%, right = MAX_SPEED *(rẽ trái gắt)*
- `filteredPosition ≥ 3500` → left = MAX_SPEED, right = 30% *(rẽ phải gắt)*

### Tìm kiếm line khi mất

- `lastSeenSide < 0` → quay trái (left = -SEARCH_REVERSE, right = +SEARCH_FORWARD)
- `lastSeenSide > 0` → quay phải (left = +SEARCH_FORWARD, right = -SEARCH_REVERSE)
- không rõ → tiến thẳng chậm (30%)

### Dừng khi all-black (vạch đích)

Nếu cả 5 cảm biến đều detect line liên tục trong **40 chu kỳ điều khiển** (40 × 3 ms = 120 ms):
1. Chuyển sang `CAR_MODE_STOP`
2. Tắt STBY
3. In thông báo qua cả UART2 và Bluetooth

***

## Lệnh Bluetooth

Kết nối HC-05 bằng MobaXterm hoặc bất kỳ serial terminal nào (baudrate 9600).

### Lệnh chế độ

| Lệnh | Chức năng |
|---|---|
| `U` | Chuyển sang chế độ AUTO — xe tự dò line |
| `M` | Chuyển sang chế độ MANUAL — điều khiển thủ công |
| `X` | STOP — dừng xe, tắt driver |
| `H` | In HELP — danh sách lệnh |
| `T` | In STATUS — chế độ hiện tại, vị trí, PWM, cảm biến |

### Lệnh MANUAL *(chỉ hoạt động khi đang ở chế độ MANUAL)*

| Lệnh | Chức năng | Timeout tự dừng |
|---|---|---|
| `W` | Tiến | 250 ms sau ký tự cuối |
| `S` | Lùi | 500 ms sau ký tự cuối |
| `A` | Rẽ trái | 120 ms sau ký tự cuối |
| `D` | Rẽ phải | 120 ms sau ký tự cuối |

> **Lưu ý timeout:** Terminal không gửi tín hiệu khi nhả phím. Bấm nhanh = xe di chuyển một xung ngắn rồi tự dừng. Giữ phím = terminal auto-repeat ký tự → xe chạy liên tục đến khi nhả.

Lệnh chấp nhận cả chữ thường và chữ hoa (`w` = `W`).

***

## Ghi chú kỹ thuật

### Giới hạn tốc độ cơ học (motor 1:150)

Motor GA12 N20 tỉ số 1:150 tại 12 V không tải đạt khoảng 100–150 RPM sau giảm tốc. Với bánh 43 mm:

```
Tốc độ lý thuyết tối đa ≈ π × 0.043 m × 150 RPM / 60 ≈ 0.34 m/s
```

Đây là trần vật lý — nếu tốc độ yêu cầu cao hơn, cần thay motor tỉ số truyền nhỏ hơn (1:50 hoặc 1:30).

### Ring buffer Bluetooth

TX và RX Bluetooth đều dùng ring buffer interrupt-driven:
- RX: `BT_RX_BUF_SIZE = 64` byte — đủ cho lệnh 1 ký tự
- TX: `BT_TX_BUF_SIZE = 256` byte — đủ cho 1 dòng report đầy đủ
- Không blocking — `main()` loop không bao giờ bị treo chờ UART

### Interrupt priority

| Interrupt | Priority | Lý do |
|---|---|---|
| USART1 (HC-05) | 0 (cao nhất) | Không mất byte Bluetooth |
| EXTI15_10 (encoder trái) | 1 | Đếm xung chính xác |
| EXTI9_5 (encoder phải) | 1 | Đếm xung chính xác |

### Lưu ý khi dùng MobaXterm / serial terminal

Terminal không gửi byte riêng khi nhả phím — xe MANUAL dùng timeout để tự dừng. Nếu xe giật cục khi điều khiển, điều chỉnh các hằng số:

```c
#define MANUAL_FWD_RELEASE_TIMEOUT_MS   250U
#define MANUAL_BWD_RELEASE_TIMEOUT_MS   500U
#define MANUAL_TURN_RELEASE_TIMEOUT_MS  120U
```

Tăng timeout → xe chạy lâu hơn mỗi lần bấm phím. Giảm timeout → xe dừng nhanh hơn sau khi nhả phím.

***

## License

MIT License — free to use, modify, and distribute.

