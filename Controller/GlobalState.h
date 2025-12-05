#pragma once
#include <vector>
#include <mutex>
#include <SFML/Graphics.hpp>
struct GlobalState {
private:
	std::vector<char> currentBuffer;
	std::mutex imageMutex;

public:

	bool isConnected = false;
	bool isImageUpdated = false;

	void setImageBuffer(std::vector<char>& data) {
		std::lock_guard<std::mutex> lock(imageMutex);
		this->currentBuffer = data;
		isImageUpdated = true;
	}

	bool loadToSFMLImage(sf::Image& outImage) {
		std::lock_guard<std::mutex> lock(imageMutex); // Khóa

		// Load trực tiếp từ buffer nội bộ vào Image của SFML
		if (outImage.loadFromMemory(currentBuffer.data(), currentBuffer.size())) {
			isImageUpdated = false;
			return true;
		}
		return false;
	} // Mở khóa
};