# Vũ Đăng Thành – Lab 3 – Đề 5
## Thiết kế mạch phát hiện té ngã sử dụng ESP32, MPU6050, FreeRTOS và MQTT

Repository chứa mã nguồn mô phỏng Wokwi, giao diện Web và báo cáo thực hành.

## Cấu trúc
- `sketch.ino`: chương trình ESP32, đọc MPU6050, chạy ba Task FreeRTOS và giao tiếp MQTT.
- `diagram.json`: sơ đồ kết nối phần cứng trên Wokwi.
- `libraries.txt`: các thư viện cần thiết trong Wokwi.
- `wokwi-project.txt`: liên kết dự án Wokwi.
- `web/canhbaotenga.html`: giao diện giám sát và điều khiển qua MQTT.
- `report/Bao-cao-Lab3-De-5.docx`: báo cáo thực hành.

## Chạy mô phỏng
1. Mở dự án Wokwi: https://wokwi.com/projects/480583280120644609
2. Cài đặt các thư viện được liệt kê trong `libraries.txt`.
3. Firmware hiện sử dụng Wi-Fi `Wokwi-GUEST`, MQTT broker `broker.hivemq.com:1883`.
4. Mở `web/canhbaotenga.html` bằng trình duyệt có Internet để tải MQTT.js và Chart.js từ CDN.
5. Firmware và giao diện dùng chung các topic:
   - `VuDangThanh_nhom5/fall/data` (ESP32 → Web)
   - `VuDangThanh_nhom5/fall/led` (Web → ESP32)
   - `VuDangThanh_nhom5/fall/buzzer` (Web → ESP32)

## Phần cứng / chân kết nối
- MPU6050: SDA GPIO21, SCL GPIO22, VCC 3V3, GND GND.
- LED: GPIO26 qua điện trở hạn dòng.
- Buzzer: GPIO27.

## Lưu ý
Broker MQTT là dịch vụ công cộng; topic có thể được người khác truy cập. Không dùng cho dữ liệu nhạy cảm. Ngưỡng phát hiện té ngã trong mô hình là 2,0 g hoặc góc trên 60°; đây là mô hình học tập, không phải thiết bị y tế.
