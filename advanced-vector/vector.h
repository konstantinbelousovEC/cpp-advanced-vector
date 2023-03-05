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

    RawMemory(size_t capacity);
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept;
    RawMemory& operator=(RawMemory&& rhs) noexcept;
    ~RawMemory();

    T* operator+(size_t offset) noexcept;
    const T* operator+(size_t offset) const noexcept;
    const T& operator[](size_t index) const noexcept;
    T& operator[](size_t index) noexcept;

    void Swap(RawMemory& other) noexcept;
    const T* GetAddress() const noexcept;
    T* GetAddress() noexcept;
    size_t Capacity() const noexcept;

private:
    static T* Allocate(size_t n);
    static void Deallocate(T* buf) noexcept;

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;
    Vector(size_t size);
    Vector(const Vector& other);
    Vector(Vector&& other) noexcept;
    ~Vector();

    Vector& operator=(const Vector& rhs);
    Vector& operator=(Vector&& rhs) noexcept;

    const T& operator[](size_t index) const noexcept;
    T& operator[](size_t index) noexcept;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    template <typename... Args>
    T& EmplaceBack(Args&&... args);

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args);

    void PushBack(const T& value);
    void PushBack(T&& value);
    iterator Insert(const_iterator pos, const T& value);
    iterator Insert(const_iterator pos, T&& value);

    void PopBack() noexcept;
    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>);

    void Swap(Vector& other) noexcept;

    size_t Size() const noexcept;
    size_t Capacity() const noexcept;
    void Reserve(size_t new_capacity);
    void Resize(size_t new_size);

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void ReplaceElementsInMemory(iterator old_memory, iterator new_memory, size_t size);
};

template <typename T>
Vector<T>::Vector(size_t size) : data_(size), size_(size) {
    std::uninitialized_value_construct_n(begin(), size);
}

template<typename T>
Vector<T>::Vector(const Vector& other) : data_(other.size_) , size_(other.size_) {
    std::uninitialized_copy_n(other.begin(), size_, begin());
}

template<typename T>
Vector<T>::Vector(Vector&& other) noexcept {
    Swap(other);
}

template<typename T>
Vector<T>::~Vector() {
    std::destroy_n(begin(), size_);
}

template<typename T>
Vector<T>& Vector<T>::operator=(const Vector& rhs) {
    if (this != &rhs) {
        if (rhs.size_ > data_.Capacity()) {
            Vector rhs_copy(rhs);
            Swap(rhs_copy);
        } else {
            if (size_ > rhs.size_) {
                std::copy(rhs.begin(), rhs.end(), begin());
                std::destroy_n(begin() + rhs.size_, size_ - rhs.size_);
            } else {
                std::copy(rhs.begin(), rhs.data_ + size_, begin());
                std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_ + size_);
            }
            size_ = rhs.size_;
        }
    }
    return *this;
}

template<typename T>
Vector<T>& Vector<T>::operator=(Vector&& rhs) noexcept {
    if (this != &rhs) {
        Swap(rhs);
    }
    return *this;
}

template<typename T>
const T& Vector<T>::operator[](size_t index) const noexcept {
    return const_cast<Vector&>(*this)[index];
}

template<typename T>
T& Vector<T>::operator[](size_t index) noexcept {
    assert(index < size_);
    return data_[index];
}

template<typename T>
typename Vector<T>::iterator Vector<T>::begin() noexcept { return data_.GetAddress(); }

template<typename T>
typename Vector<T>::iterator Vector<T>::end() noexcept { return data_ + size_; }

template<typename T>
typename Vector<T>::const_iterator Vector<T>::begin() const noexcept { return cbegin(); }

template<typename T>
typename Vector<T>::const_iterator Vector<T>::end() const noexcept { return cend(); }

template<typename T>
typename Vector<T>::const_iterator Vector<T>::cbegin() const noexcept { return data_.GetAddress(); }

template<typename T>
typename Vector<T>::const_iterator Vector<T>::cend() const noexcept { return data_ + size_; }

template<typename T>
template<typename... Args>
T &Vector<T>::EmplaceBack(Args&&... args) {
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

template<typename T>
template<typename... Args>
typename Vector<T>::iterator Vector<T>::Emplace(const_iterator pos, Args&&... args) {
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

template<typename T>
void Vector<T>::PushBack(const T& value) {
    EmplaceBack(value);
}

template<typename T>
void Vector<T>::PushBack(T&& value) {
    EmplaceBack(std::move(value));
}

template<typename T>
typename Vector<T>::iterator Vector<T>::Insert(const_iterator pos, const T& value) {
    return Emplace(pos, value);
}

template<typename T>
typename Vector<T>::iterator Vector<T>::Insert(const_iterator pos, T&& value) {
    return Emplace(pos, std::move(value));
}

template<typename T>
void Vector<T>::PopBack() noexcept {
    std::destroy_at(end() - 1);
    size_--;
}

template<typename T>
typename Vector<T>::iterator Vector<T>::Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
    if (pos == cend()) {
        PopBack();
        return end();
    }
    size_t position_index = std::distance(cbegin(), pos);
    std::move(data_ + position_index + 1, end(), data_ + position_index);
    PopBack();
    return data_ + position_index;
}

template<typename T>
void Vector<T>::Swap(Vector& other) noexcept {
    std::swap(size_, other.size_);
    std::swap(data_, other.data_);
}

template<typename T>
size_t Vector<T>::Size() const noexcept {
    return size_;
}

template<typename T>
size_t Vector<T>::Capacity() const noexcept {
    return data_.Capacity();
}

template<typename T>
void Vector<T>::Reserve(size_t new_capacity) {
    if (new_capacity <= data_.Capacity()) return;
    RawMemory<T> new_data{new_capacity};
    ReplaceElementsInMemory(begin(), new_data.GetAddress(), size_);
    data_.Swap(new_data);
}

template<typename T>
void Vector<T>::Resize(size_t new_size) {
    if (new_size > size_) {
        Reserve(new_size);
        std::uninitialized_value_construct_n(begin() + size_, new_size - size_);
    } else if (new_size < size_) {
        std::destroy_n(begin() + new_size, size_ - new_size);
    }
    size_ = new_size;
}

template<typename T>
void Vector<T>::ReplaceElementsInMemory(iterator old_memory, iterator new_memory, size_t size) {
    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
        std::uninitialized_move_n(old_memory, size, new_memory);
    } else {
        std::uninitialized_copy_n(old_memory, size, new_memory);
    }
    std::destroy_n(old_memory, size);
}

// -------------------- RawMemory definitions --------------------

template <typename T>
RawMemory<T>::RawMemory(size_t capacity)
        : buffer_(Allocate(capacity)) , capacity_(capacity) {}

template <typename T>
RawMemory<T>::RawMemory(RawMemory&& other) noexcept {
    Swap(other);
    Deallocate(other.buffer_);
    other.capacity_ = 0;
}

template <typename T>
RawMemory<T>& RawMemory<T>::operator=(RawMemory&& rhs) noexcept {
    if (this != &rhs) {
        Swap(rhs);
        Deallocate(rhs.buffer_);
        rhs.capacity_ = 0;
    }
    return *this;
}

template <typename T>
RawMemory<T>::~RawMemory() {
    if (buffer_ != nullptr) {
        Deallocate(buffer_);
    }
}

template <typename T>
T* RawMemory<T>::operator+(size_t offset) noexcept {
    assert(offset <= capacity_);
    return buffer_ + offset;
}

template <typename T>
const T* RawMemory<T>::operator+(size_t offset) const noexcept {
    return const_cast<RawMemory&>(*this) + offset;
}

template <typename T>
const T& RawMemory<T>::operator[](size_t index) const noexcept {
    return const_cast<RawMemory&>(*this)[index];
}

template <typename T>
T& RawMemory<T>::operator[](size_t index) noexcept {
    assert(index < capacity_);
    return buffer_[index];
}

template <typename T>
void RawMemory<T>::Swap(RawMemory& other) noexcept {
    std::swap(buffer_, other.buffer_);
    std::swap(capacity_, other.capacity_);
}

template <typename T>
const T* RawMemory<T>::GetAddress() const noexcept {
    return buffer_;
}

template <typename T>
T* RawMemory<T>::GetAddress() noexcept {
    return buffer_;
}

template <typename T>
size_t RawMemory<T>::Capacity() const noexcept {
    return capacity_;
}

template <typename T>
T* RawMemory<T>::Allocate(size_t n) {
    return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
}

template <typename T>
void RawMemory<T>::Deallocate(T* buf) noexcept {
    operator delete(buf);
}