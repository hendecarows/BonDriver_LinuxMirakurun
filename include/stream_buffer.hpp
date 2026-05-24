// SPDX-License-Identifier: MIT
//
// stream buffer with lock
//

#pragma once

#include <stdint.h>

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace BonDriver_LinuxMirakurun
{

class StreamBuffer
{
public:
	StreamBuffer(size_t capacity);
	~StreamBuffer() = default;

	// cannot copy
	StreamBuffer(const StreamBuffer &) = delete;
	StreamBuffer &operator=(const StreamBuffer &) = delete;

	// cannot move
	StreamBuffer(StreamBuffer &&) = delete;
	StreamBuffer &operator=(StreamBuffer &&) noexcept = delete;

	enum class ReadMode {
		Strict, // 要求サイズ以上のデータがなければ 0 を返す
		Partial // 現在バッファにある分だけ返す
	};

	size_t Write(const uint8_t *data, size_t data_size, size_t &lost_size);
	size_t Read(uint8_t *data, size_t data_size, ReadMode mode = ReadMode::Strict);
	void Clear();
	size_t UsedSpace() const;
	size_t FreeSpace() const;
	size_t Capacity() const;

private:
	const size_t capacity_; // バッファ容量
	mutable std::mutex mutex_;
	size_t head_ = 0; // データの書き込み位置
	size_t tail_ = 0; // データの読み込み位置
	size_t size_ = 0; // データサイズ
	std::vector<uint8_t> buffer_;
};

inline StreamBuffer::StreamBuffer(size_t capacity) : capacity_(capacity), buffer_(capacity)
{
	if (capacity_ == 0) {
		throw std::invalid_argument("invalid argument capacity = 0");
	}
}

inline size_t StreamBuffer::Write(const uint8_t *data, size_t data_size, size_t &lost_size)
{
	std::lock_guard<std::mutex> lock(mutex_);

	lost_size = 0;
	if (!data || !data_size) {
		return 0;
	}

	// データサイズ ≧ バッファ容量 の場合はバッファ容量分のみのデータを残す
	if (data_size >= capacity_) {
		lost_size = size_ + (data_size - capacity_);
		auto p = data + (data_size - capacity_);
		std::copy(p, p + capacity_, buffer_.begin());
		head_ = tail_ = 0;
		size_ = capacity_;
		return data_size;
	}

	auto free_size = capacity_ - size_;
	auto is_overwrite = (data_size > free_size);

	// 書き込み位置からバッファ終端までのサイズとデータサイズを比較
	auto first_part = std::min(data_size, capacity_ - head_);
	std::copy(data, data + first_part, buffer_.begin() + head_);
	if (data_size > first_part) {
		// バッファ終端をまたいで残りを先頭からコピーする場合
		std::copy(data + first_part, data + data_size, buffer_.begin());
	}

	head_ = (head_ + data_size) % capacity_;
	size_ = std::min(capacity_, size_ + data_size);

	if (is_overwrite) {
		auto overwrite_size = data_size - free_size;
		tail_ = (tail_ + overwrite_size) % capacity_;
		lost_size = overwrite_size;
	}

	return data_size;
}

inline size_t StreamBuffer::Read(uint8_t *data, size_t data_size, ReadMode mode)
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (!data || !data_size || size_ == 0) {
		return 0;
	}

	// Strict モードかつデータが足りない場合は、何もせず 0 を返す
	if (mode == ReadMode::Strict && size_ < data_size) {
		return 0;
	}

	// 実際に読み出すサイズを決定（Partialならある分だけ、Strictなら要求サイズ）
	auto read_size = std::min(size_, data_size);
	auto first_part = std::min(read_size, capacity_ - tail_);
	std::copy(buffer_.begin() + tail_, buffer_.begin() + tail_ + first_part, data);

	if (read_size > first_part) {
		// バッファ終端をまたいで残りを先頭からコピーする場合
		auto second_part = read_size - first_part;
		std::copy(buffer_.begin(), buffer_.begin() + second_part, data + first_part);
	}

	tail_ = (tail_ + read_size) % capacity_;
	size_ -= read_size;

	return read_size;
}

inline void StreamBuffer::Clear()
{
	std::lock_guard<std::mutex> lock(mutex_);

	head_ = tail_ = size_ = 0;
}

inline size_t StreamBuffer::UsedSpace() const
{
	std::lock_guard<std::mutex> lock(mutex_);

	return size_;
}

inline size_t StreamBuffer::FreeSpace() const
{
	std::lock_guard<std::mutex> lock(mutex_);

	return capacity_ - size_;
}

inline size_t StreamBuffer::Capacity() const
{
	return capacity_;
}

} // namespace BonDriver_LinuxMirakurun