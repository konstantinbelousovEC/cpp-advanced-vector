#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    RawMemory(size_t capacity) : buffer_(Allocate(capacity)) , capacity_(capacity) {}

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
        Deallocate(other.buffer_);
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
            Deallocate(rhs.buffer_);
            rhs.capacity_ = 0;
        }
        return *this;
    }

    ~RawMemory() {
        if (buffer_ != nullptr) {
            Deallocate(buffer_);
        }
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    Vector(size_t size) : data_(size), size_(size) {
        std::uninitialized_value_construct_n(begin(), size);
    }

    Vector(const Vector& other) : data_(other.size_) , size_(other.size_) {
        std::uninitialized_copy_n(other.begin(), size_, begin());
    }

    Vector(Vector&& other) noexcept { Swap(other); };

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                if (size_ > rhs.size_) {
                    size_t i = 0;
                    for (; i != rhs.size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    std::destroy_n(begin() + i, size_ - i);
                } else {
                    size_t i = 0;
                    for (; i != size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    std::uninitialized_copy_n(rhs.begin(), rhs.size_ - i, begin());
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    ~Vector() { std::destroy_n(begin(), size_); }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    iterator begin() noexcept { return data_.GetAddress(); }
    iterator end() noexcept { return data_ + size_; }
    const_iterator begin() const noexcept { return cbegin(); }
    const_iterator end() const noexcept { return cend(); }
    const_iterator cbegin() const noexcept { return data_.GetAddress(); }
    const_iterator cend() const noexcept { return data_ + size_; }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ < data_.Capacity()) {
            new (data_ + size_) T{std::forward<Args>(args)...};
        } else {
            RawMemory<T> new_data{size_ == 0 ? 1 : size_ * 2};
            new (new_data + size_) T{std::forward<Args>(args)...};
            ReplaceElementsInMemory(data_.GetAddress(), new_data.GetAddress(), size_);
            data_.Swap(new_data);
        }
        size_++;
        return data_[size_ - 1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t position_index = std::distance(cbegin(), pos);
        if (size_ < data_.Capacity()) {
            if (pos == end()) return &EmplaceBack(std::forward<Args>(args)...);
            T tmp{std::forward<Args>(args)...};
            new (end()) T{std::move(data_[size_ - 1])};
            std::move_backward(data_ + position_index, end() - 1, end());
            data_[position_index] = std::move(tmp);
        } else {
            RawMemory<T> new_data{size_ == 0 ? 1 : size_ * 2};
            new (new_data + position_index) T{std::forward<Args>(args)...};
            try {
                ReplaceElementsInMemory(begin(), new_data.GetAddress(), position_index);
            } catch (...) {
                std::destroy_at(new_data + position_index);
                throw;
            }
            try {
                ReplaceElementsInMemory(begin() + position_index, new_data.GetAddress() + position_index + 1, size_ - position_index);
            } catch (...) {
                std::destroy_n(begin(), position_index + 1);
                throw;
            }
            data_.Swap(new_data);
        }
        size_++;
        return data_ + position_index;
    }

    void PushBack(const T& value) { EmplaceBack(value); }
    void PushBack(T&& value) { EmplaceBack(std::move(value)); }
    iterator Insert(const_iterator pos, const T& value) { return Emplace(pos, value); }
    iterator Insert(const_iterator pos, T&& value) { return Emplace(pos, std::move(value)); }

    void PopBack() noexcept {
        std::destroy_at(end() - 1);
        size_--;
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        if (pos == cend()) {
            PopBack();
            return end();
        }
        size_t position_index = std::distance(cbegin(), pos);
        std::destroy_at(pos);
        std::move(data_ + position_index + 1, end(), data_ + position_index);
        size_--;
        return data_ + position_index;
    }

    void Swap(Vector& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(data_, other.data_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) return;
        RawMemory<T> new_data{new_capacity};
        ReplaceElementsInMemory(begin(), new_data.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(begin() + size_, new_size - size_);
        } else if (new_size < size_) {
            std::destroy_n(begin() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void ReplaceElementsInMemory(iterator old_memory, iterator new_memory, size_t size) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(old_memory, size, new_memory);
        } else {
            std::uninitialized_copy_n(old_memory, size, new_memory);
        }
        std::destroy_n(old_memory, size);
    }
};