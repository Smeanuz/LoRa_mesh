# DEV_FLASH_DEBUG — Nạp và debug firmware LoRa_mesh trên Wio-E5 mini

Quy trình nạp/debug bằng **J-Link + VS Code**. Cài đặt môi trường lần đầu xem `DEV_SETUP.md`.

---

## 0. Thiết lập biến môi trường (làm 1 lần / máy)

Cấu hình `.vscode` không chứa đường dẫn tuyệt đối — mọi đường dẫn tool lấy từ biến môi trường.
Mở **PowerShell** và chạy `setx` với đường dẫn **trên máy của bạn**.

Ví dụ trên máy của Huy:

```powershell
setx ZEPHYR_BASE            "C:\zephyrproject\zephyr"
setx ZEPHYR_SDK_INSTALL_DIR "C:\Users\ADMIN\zephyr-sdk-1.0.1"
setx JLINK_DIR              "C:\Program Files\SEGGER\JLink_V996a"
```

Mở **PowerShell mới** để kiểm tra (tất cả phải ra `True`):

```powershell
Test-Path "$env:ZEPHYR_BASE\..\.venv\Scripts\west.exe"
Test-Path "$env:ZEPHYR_SDK_INSTALL_DIR\gnu\arm-zephyr-eabi\bin\arm-zephyr-eabi-gdb.exe"
Test-Path "$env:JLINK_DIR\JLink.exe"
Test-Path "$env:JLINK_DIR\JLinkGDBServerCL.exe"
```

> ⚠️ Sau `setx` phải **đóng hết mọi cửa sổ VS Code** rồi mở lại. *Reload Window* không đủ.

| Biến rỗng / sai | Triệu chứng |
|---|---|
| `JLINK_DIR` | `west flash`: `required program /JLink.exe not found` — Debug: `spawn JLinkGDBServerCL.exe ENOENT` |
| `ZEPHYR_SDK_INSTALL_DIR` | Debug: `spawn /gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb.exe ENOENT` |
| `ZEPHYR_BASE` | Task build: không tìm thấy `west.exe` |

---

## 1. Đấu dây J-Link (header 20 pin) ↔ Wio-E5 mini

| Wio-E5 mini | J-Link | Ghi chú |
|---|---|---|
| 3V3 | Pin 1 (VTref) | Bắt buộc — J-Link đo mức logic, không cấp nguồn |
| SWDIO (PA13) | Pin 7 (TMS) | |
| SWCLK (PA14) | Pin 9 (TCK) | |
| RST | **Pin 15 (RESET)** | **Không phải pin 3 (nTRST)** |
| GND | Pin 4 | |

Kit cấp nguồn qua **USB-C** (đồng thời là cổng COM log, 115200-8N1).

---

## 2. Kit mới — làm 1 lần cho mỗi kit

Kit xuất xưởng khoá **RDP level 1** → debugger kết nối được nhưng flash bị khoá, phải rút nguồn mới chạy.

1. Mở **J-Flash** → device `STM32WLE5JC`, SWD, **1000 kHz** → **Connect**.
2. Hộp thoại *"Active read and/or write protected STM32 device detected"* → **Yes** (không tick *Remember*).
   Toàn bộ flash bị xoá, kể cả firmware AT gốc của Seeed.
3. **Rút USB-C, cắm lại.**
4. Đóng J-Flash → VS Code: task **west flash (J-Link)**.
5. **Rút USB-C, cắm lại lần nữa** (xoá cờ EMPTY — xem mục 5).
6. Tera Term (115200) → bấm RST → phải thấy `System initialized`.

> Không gỡ RDP bằng STM32CubeProgrammer + J-Link (không ổn định). Muốn dùng CubeProgrammer thì dùng ST-LINK.

---

## 3. Nạp hằng ngày

Trước khi nạp: **đóng mọi chương trình đang giữ J-Link** (J-Flash, CubeProgrammer, phiên debug đang chạy → bấm Stop).

| Bước | VS Code (`Ctrl+Shift+P` → *Tasks: Run Task*) |
|---|---|
| Sửa file `.c/.h` | **west build (debug)** — hoặc `Ctrl+Shift+B` |
| Sửa `prj.conf`, `debug.conf`, `CMakeLists.txt`, overlay | **west build (pristine, debug)** |
| Nạp | **west flash (J-Link)** |

Nạp thành công khi log có `JLink version: 9.96a` và `Flashing file: ...zephyr.hex`, không có dòng `FATAL ERROR`.
Muốn log chi tiết: `west -v flash -r jlink --commander "$env:JLINK_DIR\JLink.exe"`.

### Nạp từ terminal (không dùng VS Code)

```powershell
& "$env:ZEPHYR_BASE\..\.venv\Scripts\Activate.ps1"
cd <thư mục LoRa_mesh>
west build -b lora_e5_mini
west flash -r jlink --commander "$env:JLINK_DIR\JLink.exe"
```

---

## 4. Debug

| Cấu hình (F5) | Làm gì |
|---|---|
| **Debug Wio-E5 (J-Link)** | Build → nạp → dừng ở `main()` |
| **Attach Wio-E5 (J-Link, không reset)** | Gắn vào firmware đang chạy, không nạp, không reset |

Kiểm tra đang chạy đúng code: khi dừng, *Debug Console* hiện địa chỉ `0x0800xxxx` (flash).
Nếu là `0x1FFFxxxx in ?? ()` → chip đang ở bootloader ROM (xem mục 5).

### Đặt breakpoint ở đâu

| Muốn xem | Breakpoint |
|---|---|
| Lệnh CLI đã gõ xong (sau Enter) | `cmd_execute()` trong `Core/CmdLine/cmd.c` |
| Gói sắp phát | `lora_radio_send(...)` trong `lora_owner_thread` (`Core/LoRa/lora_app.c`) |
| Gói vừa nhận & decode | `mesh_router_process()` trong `protocol/processor/packet_processor.c` |
| Quyết định của router | các `return MESH_PACKET_*` trong `protocol/mesh/mesh_router.c` |

**Tránh:**
- Breakpoint trong `task_cmdline_update()` → dừng ở **mỗi ký tự** gõ trên UART.
- Breakpoint trong `uart_cmd_isr()` / `lora_rx_callback()` → chạy trong ngữ cảnh ngắt, dễ mất dữ liệu.
- Bấm **Pause** → gần như luôn rơi vào `arch_cpu_idle()` (idle thread, WFI). Đây là **bình thường**; dùng breakpoint thay cho Pause.
- Step (F10) qua `k_msleep()` / `k_sem_take()` → thread đi ngủ, debugger rơi vào idle. Dùng F5 + breakpoint.

---

## 5. "Debug thì chạy, nạp xong reset thì không chạy"

Sau khi flash bị xoá (gỡ RDP, Erase Chip), STM32WL bật cờ **EMPTY** → mỗi lần reset đều nhảy vào **bootloader ROM** thay vì chạy flash.

- **Debug chạy được** vì debugger đặt PC thẳng vào code trong flash, bỏ qua cờ EMPTY.
- **Nạp + reset không chạy** vì chip đọc cờ EMPTY → vào bootloader → không có log UART.
- Cờ EMPTY **chỉ được xét lại khi mất nguồn hoàn toàn**; reset bằng `west flash`, nút RST hay chân RST đều không đủ.

**Cách sửa:** sau lần nạp đầu tiên kể từ khi xoá chip → **rút USB-C, cắm lại**. Các lần nạp sau không cần.

---

## 6. Sự cố thường gặp

| Triệu chứng | Cách xử lý |
|---|---|
| `InitTarget() returned with error code -1` | Chip còn RDP1 (mục 2), hoặc chưa nối RST pin 15 |
| `ERROR: Could not connect to target` | Kiểm tra VTref / SWDIO / SWCLK; giảm speed xuống 100 kHz |
| J-Link Configurator báo *Firmware Restoration* | Probe ở bootloader mode → bấm OK để khôi phục; không dùng chung CubeProgrammer với J-Link |
| `west flash` báo J-Link đang bận | Stop phiên debug / đóng J-Flash, CubeProgrammer |
| Không có log UART sau khi nạp | Rút cắm nguồn (mục 5); Tera Term đúng cổng COM, 115200 |
