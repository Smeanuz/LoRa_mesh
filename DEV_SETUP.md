# DEV_SETUP — Môi trường phát triển LoRa_mesh (Zephyr + Wio-E5 mini)

Hướng dẫn cài đặt một lần cho máy **Windows 10/11** để build, nạp và debug firmware trên kit **Seeed Wio-E5 mini (STM32WLE5JC)** bằng **J-Link**.

| Thành phần | Phiên bản đã kiểm chứng |
|---|---|
| Zephyr | 4.5.99 (nhánh main) |
| Zephyr SDK | 1.0.1 |
| Python | 3.13.x (tối thiểu 3.12) |
| J-Link Software Pack | V9.24a |
| Board Zephyr | `lora_e5_mini` |

> Thời gian ước tính: 1–2 giờ (phần lớn là tải `west update` và SDK).

---

## 1. Cài công cụ nền

Mở **PowerShell (Run as Administrator)**:

```powershell
winget install Kitware.CMake Ninja-build.Ninja oss-winget.gperf Git.Git oss-winget.dtc wget 7zip.7zip Ccache.Ccache
winget install Python.Python.3.13
```

Thêm 7-Zip vào `PATH` của user (setup SDK cần `7z`):

```powershell
[Environment]::SetEnvironmentVariable("Path", [Environment]::GetEnvironmentVariable("Path","User") + ";C:\Program Files\7-Zip", "User")
```

**Đóng PowerShell, mở lại (không cần Admin)** cho các bước sau.

> ⚠️ **Python phải ≥ 3.12.** Zephyr mới dùng `Path.relative_to(walk_up=True)`, Python cũ sẽ báo
> `TypeError: ... unexpected keyword argument 'walk_up'` khi build.
> Kiểm tra bằng `py -0` và luôn tạo venv bằng `py -3.13`.

---

## 2. Tạo Zephyr workspace

Đặt workspace ở đường dẫn **ngắn, không dấu cách**: `C:\zephyrproject`.

```powershell
cd C:\
py -3.13 -m venv C:\zephyrproject\.venv
C:\zephyrproject\.venv\Scripts\Activate.ps1      # nếu bị chặn: Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
python --version                                  # phải ra 3.13.x
pip install west

west init C:\zephyrproject
cd C:\zephyrproject                               # các lệnh west phải chạy BÊN TRONG workspace
west update                                       # 10–30 phút
west packages pip --install                       # PHẢI chạy trước zephyr-export
west zephyr-export
```

> ⚠️ Thứ tự quan trọng: nếu chạy `west zephyr-export` trước `west packages pip --install`
> sẽ gặp `ModuleNotFoundError: No module named 'jsonschema'`.
> Nếu `west packages` lỗi: `pip install -r C:\zephyrproject\zephyr\scripts\requirements.txt`

### (Tuỳ chọn) Giảm dung lượng workspace

Mặc định `west update` tải HAL của mọi hãng (~20–30 GB tổng). Chỉ giữ thứ dự án cần:

```powershell
cd C:\zephyrproject
west config manifest.project-filter -- "-.*,+cmsis,+cmsis_6,+hal_stm32,+loramac-node"
west list    # xoá tay các thư mục trong modules\ không còn trong danh sách
```

Build báo thiếu module nào → thêm `+<tên>` vào filter rồi `west update`.

---

## 3. Cài Zephyr SDK (toolchain)

```powershell
cd C:\zephyrproject\zephyr
west sdk install -t arm-zephyr-eabi
```

SDK được cài vào `C:\Users\<user>\zephyr-sdk-<version>`.

- Lỗi `Zephyr SDK setup requires '7z'` → kiểm tra lại bước thêm 7-Zip vào `PATH`, mở terminal mới, rồi chạy
  `C:\Users\<user>\zephyr-sdk-<version>\setup.cmd`.
- Dòng `SKIPPED: Windows host tools are not available yet.` là **bình thường** (ta dùng J-Link, không cần OpenOCD của SDK).

---

## 4. Cài J-Link

1. Tải **J-Link Software and Documentation Pack** (Windows 64-bit) từ segger.com, cài mặc định (kèm driver USB).
2. Kiểm tra probe: mở **J-Link Configurator** → probe phải hiện trong *Connected via USB* có S/N và firmware.

> ⚠️ **Không dùng STM32CubeProgrammer với J-Link.** CubeProgrammer kèm J-Link DLL cũ (V7.x) riêng;
> dùng xen kẽ với phần mềm SEGGER V9 sẽ làm firmware probe bị ghi đè qua lại → probe rơi vào
> *bootloader mode* (J-Link Configurator báo "Firmware Restoration"). Khi đó bấm **OK** để khôi phục.
> Quy ước: **J-Link chỉ dùng với phần mềm SEGGER + `west`; ST-LINK mới dùng CubeProgrammer.**

---

## 5. Biến môi trường (bắt buộc — cấu hình `.vscode` đọc từ đây)

Thay đường dẫn cho đúng máy của bạn:

```powershell
setx ZEPHYR_BASE "C:\zephyrproject\zephyr"
setx ZEPHYR_SDK_INSTALL_DIR "C:\Users\<user>\zephyr-sdk-1.0.1"
setx JLINK_DIR "C:\Program Files\SEGGER\JLink_V924a"
```

| Biến | Dùng ở đâu |
|---|---|
| `ZEPHYR_BASE` | `west build` khi project nằm ngoài workspace; `tasks.json` tìm `west.exe` tại `%ZEPHYR_BASE%\..\.venv` |
| `ZEPHYR_SDK_INSTALL_DIR` | CMake tìm toolchain; `launch.json` tìm `arm-zephyr-eabi-gdb.exe` |
| `JLINK_DIR` | `launch.json` (GDB Server), `tasks.json` (`west flash --commander`) |

> Sau `setx` phải **đóng hẳn mọi cửa sổ VS Code / terminal** rồi mở lại mới nhận biến mới.
> Kiểm tra: `echo $env:JLINK_DIR`

### (Khuyến nghị) Windows Defender exclusion

Bước CMake configure có thể mất > 3 phút do Defender quét. Thêm vào
*Windows Security → Virus & threat protection → Manage settings → Exclusions*:
`C:\zephyrproject`, thư mục Zephyr SDK, thư mục repo.

---

## 6. VS Code

1. Mở **đúng thư mục `LoRa_mesh`** (File → Open Folder), không mở thư mục cha.
2. Cài extension được gợi ý (`.vscode/extensions.json`): **Cortex-Debug**, **C/C++**.
3. Nếu extension **CMake Tools** hỏi configure → chọn *Not now / Never*. Project này build bằng `west`, **không** dùng task "CMake: build".

### Các task (`Ctrl+Shift+P` → *Tasks: Run Task*)

| Task | Khi nào dùng |
|---|---|
| `west build (pristine, debug)` | Lần đầu; sau khi sửa `prj.conf`, `debug.conf`, `CMakeLists.txt`, overlay; sau khi build release |
| `west build (debug)` | Sửa file `.c/.h` — build tăng dần (`Ctrl+Shift+B`) |
| `west build (release)` | Bản không có `debug.conf` (tối ưu `-Os`, không debug info thread) |
| `west flash (J-Link)` | Nạp `build/zephyr/zephyr.hex` |

Build thành công khi cuối log có bảng `Memory region ... FLASH / RAM`.

IntelliSense (Ctrl+Click vào `#include <zephyr/...>`) chỉ hoạt động **sau lần build thành công đầu tiên** (cần `build/compile_commands.json`) → *Developer: Reload Window*.

### Debug

`F5` → **Debug Wio-E5 (J-Link)**: build → nạp → dừng ở `main()`.
**Attach Wio-E5** gắn vào board đang chạy, không reset.

---

## 7. Phần cứng

### Đấu J-Link (header 20 pin) ↔ Wio-E5 mini

| Wio-E5 mini | J-Link | Ghi chú |
|---|---|---|
| 3V3 | Pin 1 (VTref) | **Bắt buộc** — J-Link đo mức logic, không cấp nguồn |
| SWDIO (PA13) | Pin 7 | |
| SWCLK (PA14) | Pin 9 | |
| RST (NRST) | Pin 15 | **Nên nối** — cần khi chip đang ngủ/treo |
| GND | Pin 4 | |

Kit cấp nguồn qua **USB-C** (cũng là cổng COM log UART, 115200-8N1).
Dây SWD nên ngắn (< 15 cm). Nếu kết nối chập chờn, giảm `-speed` xuống `100`.

### Lần đầu với mỗi kit mới (chỉ làm 1 lần)

1. **Gỡ Read Protection (RDP)** — kit xuất xưởng khoá RDP level 1.
   **Thao tác này xoá vĩnh viễn firmware AT gốc của Seeed.**
   - J-Link: `& "$env:JLINK_DIR\JLink.exe" -device STM32WLE5JC -if SWD -speed 1000 -autoconnect 1` → đồng ý Unlock nếu được hỏi; hoặc **J-Flash Lite → Erase Chip**.
   - ST-LINK: `STM32_Programmer_CLI -c port=SWD mode=UR -rdu`
2. **Rút USB-C, cắm lại** (power cycle) để option byte có hiệu lực.
3. Nạp firmware: task `west flash (J-Link)`.
4. **Rút USB-C, cắm lại lần nữa.** Sau khi flash bị xoá, STM32WL đặt cờ *EMPTY* → luôn boot vào bootloader ROM (PC ở `0x1FFFxxxx`). Cờ này chỉ được đánh giá lại khi **mất nguồn hoàn toàn**; reset bằng debugger/nút RST không đủ.

### Kiểm tra firmware chạy

Mở Tera Term / PuTTY ở cổng COM của kit, 115200, bấm RST:

```
[LoRa] Initialization successful
[LoRa] Async RX started
System initialized
```

Tera Term: *Setup → Terminal* → New-line Transmit = **CR**, **tắt Local echo** (firmware tự echo).

Test 2 kit:

```
Kit A: lora_id 1
Kit B: lora_id 2
Kit A: lora_ping 2      → [PING] PONG received ... RTT=xx ms
```

---

## 8. Xử lý sự cố

| Triệu chứng | Nguyên nhân / Cách sửa |
|---|---|
| `no west workspace found` | Đang đứng ngoài `C:\zephyrproject` — `cd` vào workspace |
| `west: unknown command "build"` | Project ngoài workspace và thiếu `ZEPHYR_BASE` (mục 5) |
| `No module named 'jsonschema'` | Chạy `west packages pip --install` trước `west zephyr-export` |
| `unexpected keyword argument 'walk_up'` | venv dùng Python < 3.12 — tạo lại venv bằng `py -3.13` |
| `Could NOT find Ccache` | Chỉ là cảnh báo; `winget install Ccache.Ccache` để build lại nhanh hơn |
| `spawn JLinkGDBServerCL.exe ENOENT` | `JLINK_DIR` sai/chưa đặt, hoặc VS Code chưa mở lại sau `setx` |
| `required program ...\JLink_V796\JLink.exe not found` | Zephyr tìm nhầm bản J-Link cũ — task flash đã dùng `--commander %JLINK_DIR%\JLink.exe` |
| `InitTarget() returned with error code -1` | Chip còn RDP, hoặc đang ngủ / firmware chiếm SWD → nối dây RST (hoặc giữ nút RST, nhả khi hiện `Connecting to target...`) |
| `Selected interface (SWD) is not supported by the connected probe` | Firmware probe hỏng (bootloader mode) → J-Link Configurator khôi phục firmware |
| Debug dừng ở `0x1fff2430 in ?? ()` | Cờ EMPTY của STM32WL — rút cắm nguồn kit sau khi nạp (mục 7) |
| Pause luôn rơi vào `arch_cpu_idle()` | **Bình thường** — idle thread (WFI). Dùng breakpoint thay vì Pause |
| Gõ mỗi ký tự trên UART là debugger dừng | Có breakpoint ở `task_cmdline_update()` — đặt ở `cmd_execute()` thay thế |
| Gạch đỏ ở `#include <zephyr/...>` | Chưa build thành công lần nào → build rồi *Reload Window* |

---

## 9. Ghi chú về `debug.conf`

Bản debug (`-DEXTRA_CONF_FILE=debug.conf`) bật:

- `CONFIG_DEBUG_OPTIMIZATIONS` — `-Og`, step từng dòng chính xác
- `CONFIG_DEBUG_THREAD_INFO` — J-Link GDB Server thấy danh sách thread Zephyr
- `CONFIG_THREAD_NAME`, `CONFIG_THREAD_STACK_INFO`, `CONFIG_INIT_STACKS` — tên thread, đo stack

Có thể thêm `CONFIG_STM32_ENABLE_DEBUG_SLEEP_STOP=y` để debugger kết nối được khi CPU đang WFI mà không cần dây RST.
**Không** đưa các option này vào `prj.conf` — chúng tăng dòng tiêu thụ và kích thước firmware.
