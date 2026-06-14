#ifndef CALLMEMAYBE_DATA_MEMBER_HPP
#define CALLMEMAYBE_DATA_MEMBER_HPP

#include <cstddef>
#include <string_view>

#include "cmm/error.hpp"
#include "cmm/info.hpp"
#include "cmm/detail/entities/entity.hpp"
#include "cmm/value.hpp"

namespace cmm {
namespace detail {

// Thunk signatures for dynamic property access
using PropertyGetterFn = Value (*)(const void* instance, std::ptrdiff_t offset);
using PropertyRefGetterFn = Value (*)(void* instance, std::ptrdiff_t offset);
using PropertySetterFn = cmm::Error (*)(void* instance, std::ptrdiff_t offset, const Value& value);

using StaticGetterFn = Value (*)(const void* address);
using StaticRefGetterFn = Value (*)(void* address);
using StaticSetterFn = cmm::Error (*)(void* address, const Value& value);

// A class data member (static or non-static).
class DataMember : public Entity {
public:
    DataMember(std::string_view name, bool is_static = false)
        : Entity(name), is_static_(is_static) {}

    /*
    Dynamic Property Access
    */

    // For non-static members
    cmm::Error get_value(const void* instance, Value& out) const {
        if (is_static_) return cmm::Error::StaticMismatch;
        if (!instance) return cmm::Error::NullValue;
        if (is_bit_field_) return cmm::Error::BitFieldUnsupported;
        if (!getter_) return cmm::Error::ThunkNotInitialized;
        
        out = getter_(instance, offset_bytes_);
        return cmm::Error::Success;
    }

    cmm::Error get_ref(void* instance, Value& out) const {
        if (is_static_) return cmm::Error::StaticMismatch;
        if (!instance) return cmm::Error::NullValue;
        if (is_bit_field_) return cmm::Error::BitFieldUnsupported;
        if (!ref_getter_) return cmm::Error::ThunkNotInitialized;
        
        out = ref_getter_(instance, offset_bytes_);
        return cmm::Error::Success;
    }

    cmm::Error set_value(void* instance, const Value& value) const {
        if (is_static_) return cmm::Error::StaticMismatch;
        if (!instance) return cmm::Error::NullValue;
        if (is_bit_field_) return cmm::Error::BitFieldUnsupported;
        if (!setter_) return cmm::Error::ThunkNotInitialized;
        
        return setter_(instance, offset_bytes_, value);
    }

    // For static members
    cmm::Error get_value(Value& out) const {
        if (!is_static_) return cmm::Error::StaticMismatch;
        if (!address_) return cmm::Error::NullValue;
        if (!static_getter_) return cmm::Error::ThunkNotInitialized;
        
        out = static_getter_(address_);
        return cmm::Error::Success;
    }

    cmm::Error get_ref(Value& out) const {
        if (!is_static_) return cmm::Error::StaticMismatch;
        if (!address_) return cmm::Error::NullValue;
        if (!static_ref_getter_) return cmm::Error::ThunkNotInitialized;
        
        out = static_ref_getter_(address_);
        return cmm::Error::Success;
    }

    cmm::Error set_value(const Value& value) const {
        if (!is_static_) return cmm::Error::StaticMismatch;
        if (!address_) return cmm::Error::NullValue;
        if (!static_setter_) return cmm::Error::ThunkNotInitialized;
        
        return static_setter_(address_, value);
    }

    /*
    Accessors & Mutators
    */

    cmm::info type_id() const { return type_id_; }
    cmm::info parent_id() const { return parent_id_; }
    std::ptrdiff_t offset_bytes() const { return offset_bytes_; }
    std::ptrdiff_t offset_bits() const { return offset_bits_; }
    bool is_static() const { return is_static_; }
    bool is_bit_field() const { return is_bit_field_; }
    void* address() const { return address_; }

    void set_type_id(cmm::info id) { type_id_ = id; }
    void set_parent_id(cmm::info id) { parent_id_ = id; }
    void set_offset_bytes(std::ptrdiff_t o) { offset_bytes_ = o; }
    void set_offset_bits(std::ptrdiff_t o) { offset_bits_ = o; }
    void set_is_static(bool v) { is_static_ = v; }
    void set_is_bit_field(bool v) { is_bit_field_ = v; }
    void set_address(void* ptr) { address_ = ptr; }

    void set_getter_thunk(PropertyGetterFn fn) { getter_ = fn; }
    void set_ref_getter_thunk(PropertyRefGetterFn fn) { ref_getter_ = fn; }
    void set_setter_thunk(PropertySetterFn fn) { setter_ = fn; }

    void set_static_getter_thunk(StaticGetterFn fn) { static_getter_ = fn; }
    void set_static_ref_getter_thunk(StaticRefGetterFn fn) { static_ref_getter_ = fn; }
    void set_static_setter_thunk(StaticSetterFn fn) { static_setter_ = fn; }

private:
    cmm::info type_id_{cmm::invalid_info};
    cmm::info parent_id_{cmm::invalid_info};
    
    std::ptrdiff_t offset_bytes_{0};
    std::ptrdiff_t offset_bits_{0};
    void* address_{nullptr};

    bool is_static_{false};
    bool is_bit_field_{false};

    // Vtable for property access
    PropertyGetterFn getter_{nullptr};
    PropertyRefGetterFn ref_getter_{nullptr};
    PropertySetterFn setter_{nullptr};

    StaticGetterFn static_getter_{nullptr};
    StaticRefGetterFn static_ref_getter_{nullptr};
    StaticSetterFn static_setter_{nullptr};
};

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_DATA_MEMBER_HPP