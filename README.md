# LoRa Mesh -- Zephyr Firmware

Firmware thử nghiệm mạng LoRa Mesh đơn giản trên **Seeed Wio-E5 Mini
(STM32WLE5JC)**, sử dụng Zephyr RTOS và backend LoRaMac-node.

Mục tiêu hiện tại: - Truyền packet giữa các node LoRa. - Định tuyến
unicast cơ bản và chuyển tiếp qua node trung gian. - Giới hạn số hop
bằng TTL. - Phát hiện packet trùng lặp và packet echo do chính node
phát. - PING/PONG để kiểm tra kết nối và đo RTT. - CLI phục vụ cấu hình
và kiểm thử.

> CLI hiện là công cụ debug/test, không phải gateway hoặc Master node
> thực thụ.

## 1. Phần cứng và môi trường

  Thành phần      Thông tin
  --------------- -------------------------------------
  Board           Seeed Wio-E5 Mini
  MCU / Radio     STM32WLE5JC, Sub-GHz radio tích hợp
  RTOS            Zephyr
  Radio backend   LoRaMac-node
  Debug / nạp     ST-LINK V2

Cấu hình RF kiểm thử hiện tại:

  Tham số            Giá trị
  ------------------ --------------------------------
  Frequency          920500000 Hz
  Bandwidth          125 kHz
  Spreading Factor   SF8
  Coding Rate        4/5
  Preamble           8
  TX power           4 dBm (theo cấu hình hiện tại)
  Public network     `false`
  IQ inversion       `false`

Các tùy chọn Zephyr liên quan:

``` ini
CONFIG_LORA=y
CONFIG_LORA_MODULE_BACKEND_LORAMAC_NODE=y
CONFIG_LORA_INIT_PRIORITY=90
CONFIG_LORA_STM32WL_SUBGHZ_RADIO=y
```

Cần kiểm tra quy định tần số và công suất phát tại khu vực triển khai
trước khi sử dụng thực tế.

## 2. Kiến trúc tổng thể

Radio được quản lý tập trung bởi một **LoRa Owner Thread**. Các module
khác gửi yêu cầu qua queue thay vì trực tiếp thao tác radio.

``` text
                 CLI
          lora_id / lora_ping
                  |
          PING handler
                  |
          lora_app_send_packet()
                  |
              TX Queue
                  |
          LoRa Owner Thread
                  |
               Radio
                  |
            RX callback
       (copy data + metadata)
                  |
               RX Queue
                  |
        Packet Processor Thread
                  |
          Decode raw packet
                  |
             Mesh Router
            /     |      \
         LOCAL  FORWARD   DROP
           |       |
           v       v
       Dispatcher TX Queue
           |
       PING/PONG handler
```

### 2.1. LoRa Owner Thread

Đây là thread duy nhất trực tiếp quản lý radio: 1. Lấy packet từ TX
queue. 2. Dừng async RX trước khi phát nếu RX đang hoạt động. 3. Chuyển
radio sang TX và gửi packet. 4. Khôi phục chế độ RX và async receive sau
khi phát. 5. Được đánh thức bằng semaphore khi có công việc.

### 2.2. RX callback

Callback chỉ thực hiện các thao tác ngắn: - Nhận dữ liệu và metadata từ
radio. - Copy dữ liệu vào RX queue. - Không decode packet, không xử lý
mesh và không chạy logic PING/PONG.

### 2.3. Packet Processor Thread

Với mỗi packet từ RX queue: 1. Decode byte array thành
`protocol_packet_t`. 2. Gọi Mesh Router. 3. `MESH_PACKET_LOCAL`: chuyển
packet đến Packet Dispatcher. 4. `MESH_PACKET_FORWARD`: router đã đưa
packet đã cập nhật TTL vào TX queue. 5. `MESH_PACKET_DROP`: bỏ packet.

### 2.4. Queue và đồng bộ

  Thành phần              Vai trò
  ----------------------- -----------------------------------------------------
  RX queue                Chuyển dữ liệu từ RX callback sang Packet Processor
  TX queue                Lưu packet chờ Owner Thread phát
  `lora_owner_wake_sem`   Đánh thức Owner Thread
  Ping mutex              Bảo vệ trạng thái PING pending
  Delayable work          Xử lý timeout PING, không cần thread riêng

TX queue hiện có sức chứa 8 packet; kích thước packet tối đa là 255
bytes.

## 3. Định dạng packet

Packet truyền qua radio là byte array. Khi nhận, chương trình decode
thành `protocol_packet_t`.

### 3.1. Packet layout

      Offset   Kích thước Trường    Ý nghĩa
  ---------- ------------ --------- --------------------
           0       1 byte VERSION   Phiên bản protocol
           1       1 byte TYPE      Loại packet
        2--3      2 bytes SRC_ID    Node ID nguồn
        4--5      2 bytes DST_ID    Node ID đích
        6--7      2 bytes SEQ       Sequence number
           8       1 byte TTL       Số hop còn lại
    9 trở đi        Tùy ý PAYLOAD   Dữ liệu ứng dụng

Các trường 16-bit sử dụng **Big-endian**.

``` text
+---------+------+--------+--------+------+-----+---------+
| VERSION | TYPE | SRC_ID | DST_ID | SEQ  | TTL | PAYLOAD |
|  1 byte |1 byte|2 bytes |2 bytes |2 byte|1 B  | N bytes |
+---------+------+--------+--------+------+-----+---------+
```

### 3.2. Giới hạn và giá trị mặc định

  Hằng số                           Giá trị
  ----------------------------- -----------
  `PROTOCOL_HEADER_SIZE`            9 bytes
  `PROTOCOL_MAX_PACKET_SIZE`      255 bytes
  `PROTOCOL_MAX_PAYLOAD_SIZE`     246 bytes
  `PROTOCOL_DEFAULT_TTL`                  3
  `PROTOCOL_MAX_TTL`                     15

### 3.3. Node ID

  Giá trị                Ý nghĩa
  ---------------------- ----------------
  `0x0000`               Reserved
  `0x0001` -- `0xFFFE`   Node ID hợp lệ
  `0xFFFF`               Broadcast

Node ID được cấu hình runtime; mặc định là `1`. CLI `lora_id` thay đổi
ID trong lúc chạy.

### 3.4. Packet type

      Type Tên    Mục đích
  -------- ------ --------------------------
    `0x01` PING   Yêu cầu kiểm tra kết nối
    `0x02` PONG   Phản hồi PING

## 4. TTL và forwarding

TTL giới hạn số lần packet được chuyển tiếp qua node trung gian.

Quy tắc hiện tại: - Nếu `DST_ID` là Node ID local: xử lý local. - Nếu
packet không dành cho node local và `TTL <= 1`: drop, không forward. -
Nếu cần forward và `TTL > 1`: giảm TTL đi 1, sau đó enqueue packet để
phát. - Packet mới được tạo sẽ khởi tạo TTL bằng `PROTOCOL_DEFAULT_TTL`.

Ví dụ:

``` text
Node 1                 Node 2                 Node 3
  |                       |                      |
  | PING, TTL=3           |                      |
  +---------------------->|                      |
  |                       | PING, TTL=2          |
  |                       +--------------------->|
  |                       |                      |
  |                       |              DST matches Node 3
  |                       |              Process locally
```

Packet gốc không bị sửa; router giảm TTL trên bản sao dùng để forward.
Các trường SRC, DST, TYPE, SEQ và payload được giữ nguyên.

## 5. Packet filtering và Duplicate Detection

Router thực hiện hai bước lọc độc lập trước khi quyết định
LOCAL/FORWARD/DROP.

### 5.1. Self-originated Packet Filtering

Một node có thể thu lại packet do chính nó phát (echo/self-reception).
Packet đó có `SRC_ID` trùng với Node ID local.

Cách xử lý: nếu

``` c
packet->src_id == protocol_get_node_id()
```

thì drop packet ngay trong `mesh_router_process()`.

Kiểm tra này nên đặt trước Duplicate Detection. Nó ngăn node xử lý hoặc
forward lại packet do chính nó tạo ra.

Điều kiện: mỗi node trong mạng phải có Node ID duy nhất. Nếu hai node
dùng chung ID, chúng có thể loại bỏ packet của nhau.

### 5.2. Duplicate Detection

Duplicate cache phát hiện cùng một packet xuất hiện nhiều lần, ví dụ do
nhiều đường truyền hoặc do packet bị relay lặp.

Khóa nhận diện:

``` text
SRC_ID + TYPE + SEQ
```

  Tham số                             Giá trị
  ----------------------------- -------------
  `MESH_DUPLICATE_CACHE_SIZE`      16 entries
  `MESH_DUPLICATE_TIMEOUT_MS`        30000 ms
  Cơ chế                          Ring buffer

Nguyên tắc: 1. Kiểm tra packet có khóa trùng với entry còn hiệu lực
không. 2. Nếu trùng: trả về `true`, router drop packet. 3. Nếu không
trùng: thêm khóa vào cache và trả về `false`. 4. Entry hết hạn sau 30
giây.

`mesh_duplicate_init()` cần được gọi một lần trước khi xử lý packet.
Cache nằm trong RAM nên sẽ mất khi reset. Thiết kế hiện tại giả định một
Packet Processor Thread truy cập cache.

### 5.3. Thứ tự xử lý trong Router

Thứ tự đề xuất:

``` text
Packet received
      |
      v
Packet is NULL?
      | yes -> DROP
      v no
SRC_ID == local Node ID?
      | yes -> DROP (self-echo)
      v no
Duplicate cache hit?
      | yes -> DROP
      v no
DST_ID == local Node ID?
      | yes -> LOCAL
      v no
TTL <= 1?
      | yes -> DROP
      v no
Decrease TTL and enqueue
      |
    FORWARD
```

Self-originated filtering giải quyết packet echo của chính node;
duplicate cache giải quyết packet lặp đến từ mạng. Hai cơ chế bổ trợ
nhau, không thay thế nhau.

## 6. Luồng PING/PONG

PING/PONG dùng để kiểm tra liên lạc và đo Round-Trip Time (RTT).

### 6.1. Gửi PING

Khi CLI gọi `lora_ping <dst_id> [payload]`: 1. Kiểm tra Node ID đích và
payload. 2. Tạo packet TYPE=PING. 3. Gán SRC_ID bằng Node ID local,
DST_ID bằng node đích. 4. Tăng sequence number. 5. Gán TTL mặc định. 6.
Lưu trạng thái pending: destination, sequence và timestamp gửi. 7. Đưa
packet vào TX queue. 8. Lên lịch timeout 3000 ms bằng delayable work.

Hiện tại chỉ hỗ trợ một PING pending tại một thời điểm.

### 6.2. Nhận PING và tạo PONG

Khi PING đến đúng node đích: 1. Packet Dispatcher chuyển packet đến PING
handler. 2. Handler tạo packet PONG. 3. PONG có `SRC_ID` là node trả
lời, `DST_ID` là SRC_ID của PING. 4. Giữ nguyên sequence number và copy
payload. 5. Khởi tạo TTL của PONG bằng `PROTOCOL_DEFAULT_TTL`. 6. Đưa
PONG vào TX queue.

### 6.3. Nhận PONG và tính RTT

PONG được chấp nhận nếu: - SRC_ID khớp node đích đang pending. - DST_ID
khớp Node ID local. - SEQ khớp sequence đang pending.

Khi khớp, handler tính RTT, xóa trạng thái pending và hủy timeout. Nếu
timeout xảy ra trước khi nhận PONG, PING được xem là thất bại.

### 6.4. Ví dụ qua node relay

``` text
Node 1                 Node 2                 Node 3
  |                       |                      |
  | PING: 1 -> 3          |                      |
  +---------------------->|                      |
  |                       | Forward PING         |
  |                       +--------------------->|
  |                       |                      |
  |                       |              Create PONG
  |                       |                      |
  |                       | PONG: 3 -> 1         |
  |                       |<---------------------+
  | PONG: 3 -> 1          |                      |
  |<----------------------|                      |
  |                       |                      |
  | Calculate RTT         |                      |
```

Đây là sơ đồ minh họa khi Node 2 là relay. Đường truyền thực tế phụ
thuộc khả năng thu phát RF; một node có thể nghe trực tiếp nhiều node
hoặc nhận nhiều bản sao của cùng packet.

## 7. Mesh Router và Packet Dispatcher

Router trả về một trong ba kết quả:

``` c
typedef enum {
    MESH_PACKET_LOCAL,
    MESH_PACKET_FORWARD,
    MESH_PACKET_DROP
} mesh_packet_result_t;
```

-   `MESH_PACKET_LOCAL`: packet dành cho node hiện tại; Packet Processor
    gọi Dispatcher.
-   `MESH_PACKET_FORWARD`: packet đã giảm TTL và được đưa vào TX queue.
-   `MESH_PACKET_DROP`: packet bị loại bỏ.

Dispatcher phân phối packet local theo TYPE: - PING (`0x01`) → PING
handler. - PONG (`0x02`) → PING handler để kiểm tra phản hồi và tính
RTT.

Broadcast hiện được nhận diện là packet local theo logic đang triển
khai; cơ chế broadcast flooding chưa được xem là hoàn thiện.

## 8. CLI kiểm thử

  Lệnh                             Mục đích
  -------------------------------- --------------------------------------
  `lora_id <node_id>`              Đặt Node ID runtime
  `lora_ping <dst_id> [payload]`   Gửi PING đến node đích
  `lora_send_p`                    Gửi packet debug theo cơ chế hiện có

Ví dụ:

``` text
lora_id 1
lora_ping 3
```

CLI là công cụ debug/test, không phải một Master node trong kiến trúc
mesh.

## 9. Các module chính

  -----------------------------------------------------------------------
  Module                              Trách nhiệm
  ----------------------------------- -----------------------------------
  `lora_radio`                        Wrapper Zephyr LoRa API, cấu hình
                                      mode, send và async receive

  `lora_app`                          Queue TX/RX và giao tiếp với Owner
                                      Thread

  LoRa Owner Thread                   Độc quyền quản lý radio

  `protocol_packet`                   Cấu trúc packet, encode/decode

  `protocol_config`                   Node ID local

  `mesh_router`                       LOCAL/FORWARD/DROP, TTL và packet
                                      filtering

  `mesh_duplicate`                    Duplicate cache

  `packet_dispatcher`                 Phân phối packet local theo TYPE

  `ping_handler`                      PING/PONG, pending state và RTT

  CLI                                 Cấu hình và kiểm thử
  -----------------------------------------------------------------------

Tên file thực tế có thể thay đổi theo quá trình phát triển; bảng mô tả
trách nhiệm logic của từng module.

<!-- ## 10. Giới hạn hiện tại và hướng phát triển

### Đã có trong phạm vi hiện tại

-   LoRa TX/RX trên Wio-E5 Mini.
-   Quản lý radio tập trung qua Owner Thread.
-   RX/TX queue.
-   Protocol packet có VERSION, TYPE, SRC, DST, SEQ, TTL và payload.
-   Router cơ bản với LOCAL/FORWARD/DROP.
-   TTL decrement khi forward.
-   Self-originated packet filtering.
-   Duplicate cache.
-   PING/PONG và RTT.
-   CLI đặt Node ID và kiểm thử.

### Chưa nên xem là hoàn thiện

-   Broadcast flooding và chống broadcast storm.
-   Routing table hoặc định tuyến tối ưu.
-   Bảo mật, xác thực và mã hóa packet.
-   Quản lý topology và phát hiện node lân cận.
-   Fragmentation cho dữ liệu lớn hơn giới hạn packet.
-   Nhiều PING pending đồng thời.
-   ACK/retry cho dữ liệu ứng dụng.
-   Lưu Node ID qua reset.
-   Kiểm soát va chạm và lập lịch truyền khi nhiều node cùng phát.

### Hướng phát triển

1.  Hoàn thiện kiểm thử unicast qua một node relay.
2.  Hoàn thiện broadcast forwarding có duplicate suppression và TTL.
3.  Bổ sung packet type cho ứng dụng thực tế.
4.  Thêm ACK/retry nếu ứng dụng cần độ tin cậy.
5.  Chỉ cân nhắc routing table hoặc thuật toán phức tạp hơn sau khi
    luồng packet cơ bản ổn định.

## 11. Ghi chú thiết kế

-   Chỉ LoRa Owner Thread trực tiếp thao tác radio.
-   RX callback cần ngắn, không chứa business logic.
-   Router quyết định packet local, forward hay drop.
-   Dispatcher chỉ phân phối packet local.
-   Handler ứng dụng gửi packet qua `lora_app`, không thao tác radio
    trực tiếp.
-   Khi forward, giữ nguyên SRC, DST, TYPE, SEQ và payload; chỉ giảm
    TTL.
-   PONG là packet mới và có TTL khởi tạo riêng.
-   Self-originated filtering loại packet có SRC_ID bằng Node ID local.
-   Duplicate cache hiện dùng `SRC_ID + TYPE + SEQ`; nếu sau này có
    nhiều luồng packet hoặc sequence number quay vòng nhanh, cần xem xét
    mở rộng khóa nhận diện. -->
