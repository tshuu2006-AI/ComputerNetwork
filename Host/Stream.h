#pragma once
#include <string>

// 📸 Hàm chụp màn hình toàn bộ desktop và lưu vào file .jpg
void capture_screen(const std::string& filename);

/**
 * @brief Ghi hình từ webcam mặc định trong một khoảng thời gian và lưu ra file.
 * @param output_filename Tên file video đầu ra (ví dụ: "temp_video.mp4").
 * @param duration_seconds Số giây ghi hình.
 * @return true nếu ghi hình và lưu file thành công, false nếu có lỗi.
 */

bool record_webcam(const std::string& output_filename, int duration_seconds);