#ifndef CALLMEMAYBE_FUNCTION_HPP
#define CALLMEMAYBE_FUNCTION_HPP

#include <span>
#include <string_view>
#include "cmm/error.hpp"
#include "cmm/info.hpp"
#include "cmm/detail/entities/entity.hpp"
#include "cmm/value.hpp"

namespace cmm {
namespace detail {

// Raw function pointer for dynamic dispatch
using InvokerFn = cmm::Error (*)(std::span<Value>, Value&);

// Bitfield flags to save memory per function entity
struct FunctionFlags {
    bool is_member_function : 1 {false};
    bool is_static_function : 1 {false};
    bool is_constructor : 1 {false};
    bool is_destructor : 1 {false};
};

// Runtime representation of a function (free, member, static, constructor, destructor)
class Function : public Entity {
public:
    constexpr Function(std::string_view name,
             bool is_member_function = false,
             bool is_static_function = false)
        : Entity(name) {
        flags_.is_member_function = is_member_function;
        flags_.is_static_function = is_static_function;
    }

    constexpr void set_thunk(InvokerFn thunk) {
        thunk_ = thunk;
    }

    // Type-checked dynamic invocation. Writes the result into out and returns
    // a cmm::Error describing the outcome
    cmm::Error invoke(std::span<Value> args, Value& out) const {
        if (!thunk_) {
            return cmm::Error::ThunkNotInitialized;
        }
        return thunk_(args, out);
    }

    // Setters
    constexpr void set_is_member_function(bool v) { flags_.is_member_function = v; }
    constexpr void set_is_static_function(bool v) { flags_.is_static_function = v; }
    constexpr void set_is_constructor(bool v) { flags_.is_constructor = v; }
    constexpr void set_is_destructor(bool v) { flags_.is_destructor = v; }

    constexpr void set_parent_id(cmm::info id) { parent_id_ = id; }
    constexpr void set_return_type_id(cmm::info id) { return_type_id_ = id; }
    constexpr void set_parameter_ids(std::span<const cmm::info> ids) { parameter_ids_ = ids; }

    // Getters
    constexpr bool is_member_function() const { return flags_.is_member_function; }
    constexpr bool is_static_function() const { return flags_.is_static_function; }
    constexpr bool is_constructor() const { return flags_.is_constructor; }
    constexpr bool is_destructor() const { return flags_.is_destructor; }
    constexpr const FunctionFlags& flags() const { return flags_; }

    constexpr cmm::info parent_id() const { return parent_id_; }
    constexpr cmm::info return_type_id() const { return return_type_id_; }
    constexpr std::span<const cmm::info> parameter_ids() const { return parameter_ids_; }

private:
    FunctionFlags flags_{};

    // The class (for member functions / ctors / dtors) or namespace that
    // contains this function. invalid_info for top-level free functions.
    cmm::info parent_id_{cmm::invalid_info};

    // Type ids registered in the registry. return_type_id_ identifies the
    // Type for the return type; parameter_ids_ holds Parameter info
    // ids in declaration order (each Parameter then has its own type_id).
    cmm::info return_type_id_{cmm::invalid_info};
    std::span<const cmm::info> parameter_ids_;

    // The type-erased invoker. For non-static member functions, the 
    //  first Value in args should be the instance pointer (Class*)
    InvokerFn thunk_{nullptr};
};

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_FUNCTION_HPP