#ifndef CMM_VALUE_HPP
#define CMM_VALUE_HPP

#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <meta>

#include "cmm/error.hpp"
#include "cmm/info.hpp"
#include "cmm/detail/hash/info_hash.hpp"

/*
Tries to mirror std::any and other reflection libraries as much as possible to not reinvent the wheel, 
but uses internal cmm::info type hash to avoid rtti
*/

namespace cmm {
namespace detail {

// Small Buffer Optimization metrics
inline constexpr std::size_t SBO_SIZE = 32;
inline constexpr std::size_t SBO_ALIGN = alignof(std::max_align_t);

template <typename T>
inline constexpr bool UseSBO = (sizeof(T) <= SBO_SIZE) &&
                               (alignof(T) <= SBO_ALIGN) &&
                               std::is_nothrow_move_constructible_v<T>;

// Type-erased operations vtable
struct ValueOps {
    void (*destroy)(void* ptr);
    void* (*copy)(const void* src, void* inline_buffer);
    void* (*move)(void* src, void* inline_buffer);
};

template <typename T>
inline constexpr ValueOps value_ops = {
    [](void* ptr) {
        if constexpr (UseSBO<T>) {
            static_cast<T*>(ptr)->~T();
        } else {
            delete static_cast<T*>(ptr);
        }
    },
    [](const void* src, void* inline_buffer) -> void* {
        if constexpr (UseSBO<T>) {
            return new (inline_buffer) T(*static_cast<const T*>(src));
        } else {
            return new T(*static_cast<const T*>(src));
        }
    },
    [](void* src, void* inline_buffer) -> void* {
        if constexpr (UseSBO<T>) {
            return new (inline_buffer) T(std::move(*static_cast<T*>(src)));
        } else {
            return src; 
        }
    }
};

// Non-owning operations for reference / alias Values
inline constexpr ValueOps ref_ops = {
    [](void*) {},
    [](const void* src, void*) -> void* { return const_cast<void*>(src); },
    [](void* src, void*) -> void* { return src; }
};

} // namespace detail

class Value {
public:
    enum class Policy : uint8_t {
        Owned, // Decayed value (owns memory)
        MutRef, // Mutable reference alias
        ConstRef // Const reference alias
    };

    Value() = default;

    template <typename T, typename Decayed = std::decay_t<T>>
    requires (!std::is_same_v<Decayed, Value>)
    explicit Value(T&& val) {
        static_assert(std::is_copy_constructible_v<Decayed>, 
            "cmm::Value requires copy-constructible types.");

        type_id_ = cmm::detail::hash_entity(^^Decayed);
        policy_ = Policy::Owned;
        ops_ = &detail::value_ops<Decayed>;
        
        if constexpr (detail::UseSBO<Decayed>) {
            data_ = new (buffer_) Decayed(std::forward<T>(val));
            is_inline_ = true;
        } else {
            data_ = new Decayed(std::forward<T>(val));
            is_inline_ = false;
        }
    }

    template <typename T>
    static Value ref(T& val) {
        using Decayed = std::decay_t<T>;
        Value v;
        v.type_id_ = cmm::detail::hash_entity(^^Decayed);
        v.policy_ = Policy::MutRef;
        v.data_ = static_cast<void*>(std::addressof(val));
        v.ops_ = &detail::ref_ops;
        return v;
    }

    template <typename T>
    static Value cref(const T& val) {
        using Decayed = std::decay_t<T>;
        Value v;
        v.type_id_ = cmm::detail::hash_entity(^^Decayed);
        v.policy_ = Policy::ConstRef;
        v.data_ = const_cast<void*>(static_cast<const void*>(std::addressof(val)));
        v.ops_ = &detail::ref_ops;
        return v;
    }

    ~Value() { reset(); }

    Value(const Value& other) { copy_from(other); }

    Value& operator=(const Value& other) {
        if (this != &other) {
            reset();
            copy_from(other);
        }
        return *this;
    }

    Value(Value&& other) noexcept { move_from(std::move(other)); }

    Value& operator=(Value&& other) noexcept {
        if (this != &other) {
            reset();
            move_from(std::move(other));
        }
        return *this;
    }

    Policy policy() const { return policy_; }
    cmm::info type_id() const { return type_id_; }
    bool has_value() const { return data_ != nullptr; }
    void* data() { return data_; }
    const void* data() const { return data_; }

    /*
    Extraction APIs
    */

    // Similar to std::get_if, returns nullptr on failure
    template <typename T>
    T* get_if() noexcept {
        using Decayed = std::decay_t<T>;
        constexpr cmm::info req_id = cmm::detail::hash_entity(^^Decayed);
        
        if (req_id != type_id_ || !data_) {
            return nullptr;
        }

        if constexpr (!std::is_const_v<T>) {
            if (policy_ == Policy::ConstRef) {
                return nullptr; 
            }
        }
        
        return static_cast<Decayed*>(data_);
    }

    template <typename T>
    const T* get_if() const noexcept {
        using Decayed = std::decay_t<T>;
        constexpr cmm::info req_id = cmm::detail::hash_entity(^^Decayed);
        
        if (req_id != type_id_ || !data_) {
            return nullptr;
        }
        return static_cast<const Decayed*>(data_);
    }

    // Explicit Error Code getter - fills an out-parameter
    template <typename T>
    cmm::Error try_get(T& out_val) const noexcept {
        const T* ptr = get_if<T>();
        if (!ptr) {
            return cmm::Error::TypeMismatch;
        }
        out_val = *ptr;
        return cmm::Error::Success;
    }

    // Asserting Getter - for fast paths where you know for sure the type is correct
    template <typename T>
    T& get() {
        T* ptr = get_if<T>();
        assert(ptr != nullptr && "cmm::Value::get() failed: Type mismatch or empty value!");
        return *ptr;
    }

    template <typename T>
    const T& get() const {
        const T* ptr = get_if<T>();
        assert(ptr != nullptr && "cmm::Value::get() failed: Type mismatch or empty value!");
        return *ptr;
    }

private:
    Value(void* existing_ptr, cmm::info type_id) {
        type_id_ = type_id;
        data_ = existing_ptr;
        ops_ = &detail::ref_ops;
        is_inline_ = false; 
    }

    void reset() {
        if (data_ && ops_) {
            ops_->destroy(data_);
        }
        data_ = nullptr;
        ops_ = nullptr;
        type_id_ = cmm::invalid_info;
        policy_ = Policy::Owned;
        is_inline_ = false;
    }

    void copy_from(const Value& other) {
        type_id_ = other.type_id_;
        policy_ = other.policy_;
        ops_ = other.ops_;
        is_inline_ = other.is_inline_;
        
        if (other.data_ && ops_) {
            data_ = ops_->copy(other.data_, buffer_);
        } else {
            data_ = nullptr;
        }
    }

    void move_from(Value&& other) noexcept {
        type_id_ = other.type_id_;
        policy_ = other.policy_;
        ops_ = other.ops_;
        is_inline_ = other.is_inline_;
        
        if (other.data_ && ops_) {
            if (is_inline_) {
                data_ = ops_->move(other.data_, buffer_);
            } else {
                data_ = other.data_;
                other.data_ = nullptr; 
            }
        } else {
            data_ = nullptr;
        }
        other.reset();
    }

    cmm::info type_id_{cmm::invalid_info};
    Policy policy_{Policy::Owned};
    void* data_{nullptr};
    
    const detail::ValueOps* ops_{nullptr}; 
    bool is_inline_{false};
    alignas(detail::SBO_ALIGN) std::byte buffer_[detail::SBO_SIZE];
};

} // namespace cmm

#endif // CMM_VALUE_HPP