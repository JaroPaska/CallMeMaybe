#ifndef CALLMEMAYBE_VARIABLE_HPP
#define CALLMEMAYBE_VARIABLE_HPP

#include <string_view>

#include "cmm/error.hpp"
#include "cmm/info.hpp"
#include "cmm/detail/entities/entity.hpp"
#include "cmm/value.hpp"

namespace cmm {
namespace detail {

// Thunk signatures for dynamic global variable access
using VariableGetterFn = Value (*)(const void* address);
using VariableRefGetterFn = Value (*)(void* address);
using VariableSetterFn = cmm::Error (*)(void* address, const Value& value);

// Represents a global or namespace-scope variable.
// Like DataMember, it HAS a type (it is not a type itself), 
// so it inherits directly from Entity.
class Variable : public Entity {
public:
    constexpr Variable(std::string_view name, cmm::info type_id)
        : Entity(name), type_id_(type_id) {}

    /*
    Dynamic Property Access
    */

    cmm::Error get_value(Value& out) const {
        if (!address_) return cmm::Error::NullValue;
        if (!getter_) return cmm::Error::ThunkNotInitialized;
        
        out = getter_(address_);
        return cmm::Error::Success;
    }

    cmm::Error get_ref(Value& out) const {
        if (!address_) return cmm::Error::NullValue;
        if (!ref_getter_) return cmm::Error::ThunkNotInitialized;
        
        out = ref_getter_(address_);
        return cmm::Error::Success;
    }

    cmm::Error set_value(const Value& value) const {
        if (is_const_ || is_constexpr_) {
            return cmm::Error::ConstViolation;
        }
        if (!address_) return cmm::Error::NullValue;
        if (!setter_) return cmm::Error::ThunkNotInitialized;
        
        return setter_(address_, value);
    }

    /*
    Accessors & Mutators
    */

    constexpr cmm::info type_id() const { return type_id_; }

    // Absolute memory address of the global variable
    constexpr void* address() const { return address_; }
    constexpr void set_address(void* ptr) { address_ = ptr; }

    // Constness tracking is important to prevent the runtime 
    // library from attempting to write to read-only memory segments
    constexpr bool is_const() const { return is_const_; }
    constexpr void set_is_const(bool c) { is_const_ = c; }

    constexpr bool is_constexpr() const { return is_constexpr_; }
    constexpr void set_is_constexpr(bool ce) { is_constexpr_ = ce; }

    constexpr cmm::info parent_namespace_id() const { return parent_namespace_id_; }
    constexpr void set_parent_namespace_id(cmm::info id) { parent_namespace_id_ = id; }

    constexpr void set_getter_thunk(VariableGetterFn fn) { getter_ = fn; }
    constexpr void set_ref_getter_thunk(VariableRefGetterFn fn) { ref_getter_ = fn; }
    constexpr void set_setter_thunk(VariableSetterFn fn) { setter_ = fn; }

private:
    cmm::info type_id_{cmm::invalid_info};
    
    // Unlike DataMember which stores an offset_, globals store an absolute pointer
    void* address_{nullptr};

    bool is_const_{false};
    bool is_constexpr_{false};
    
    cmm::info parent_namespace_id_{cmm::invalid_info};

    // Vtable for property access
    VariableGetterFn getter_{nullptr};
    VariableRefGetterFn ref_getter_{nullptr};
    VariableSetterFn setter_{nullptr};
};

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_VARIABLE_HPP