#ifndef CALLMEMAYBE_THUNK_HPP
#define CALLMEMAYBE_THUNK_HPP

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>
#include <meta>

#include "cmm/error.hpp"
#include "cmm/info.hpp"
#include "cmm/value.hpp"
#include "cmm/detail/entities/function.hpp"
#include "cmm/detail/hash/info_hash.hpp"

namespace cmm {
namespace detail {

// Checks if the provided runtime argument matches the compile-time parameter requirements
constexpr bool is_compatible_argument(
    cmm::info arg_base_id, cmm::Value::Policy arg_policy, 
    cmm::info param_base_id, bool param_is_ref, bool param_is_const, bool param_is_rvalue_ref) 
{
    if (arg_base_id != param_base_id) return false;
    if (!param_is_ref) return true; 
    if (param_is_const) return true; 
    
    if (param_is_rvalue_ref) {
        return arg_policy == cmm::Value::Policy::Owned;
    }
    return arg_policy == cmm::Value::Policy::MutRef;
}

// Helper template that hides all the repeated std::meta queries for checking a single parameter
template <std::meta::info ParamTypeRefl>
constexpr bool is_argument_valid(const cmm::Value& arg) {
    constexpr cmm::info p_base_id = cmm::detail::hash_entity(std::meta::decay(ParamTypeRefl));
    constexpr bool p_is_ref = std::meta::is_reference_type(ParamTypeRefl);
    constexpr bool p_is_const = std::meta::is_const_type(std::meta::remove_reference(ParamTypeRefl));
    constexpr bool p_is_rvalue_ref = std::meta::is_rvalue_reference_type(ParamTypeRefl);

    return is_compatible_argument(arg.type_id(), arg.policy(), p_base_id, p_is_ref, p_is_const, p_is_rvalue_ref);
}

// Property access thunks
template <typename T>
struct PropertyThunks {
    static Value get(const void* inst, std::ptrdiff_t offset) {
        return Value(*reinterpret_cast<const T*>(static_cast<const char*>(inst) + offset));
    }
    static Value get_ref(void* inst, std::ptrdiff_t offset) {
        return Value::ref(*reinterpret_cast<T*>(static_cast<char*>(inst) + offset));
    }
    static cmm::Error set(void* inst, std::ptrdiff_t offset, const Value& val) {
        const T* typed = val.get_if<T>();
        if (!typed) return cmm::Error::TypeMismatch;
        *reinterpret_cast<T*>(static_cast<char*>(inst) + offset) = *typed;
        return cmm::Error::Success;
    }
};

template <typename T>
struct StaticThunks {
    static Value get(const void* address) {
        return Value(*reinterpret_cast<const T*>(address));
    }
    static Value get_ref(void* address) {
        return Value::ref(*reinterpret_cast<T*>(address));
    }
    static cmm::Error set(void* address, const Value& val) {
        const T* typed = val.get_if<T>();
        if (!typed) return cmm::Error::TypeMismatch;
        *reinterpret_cast<T*>(const_cast<void*>(address)) = *typed;
        return cmm::Error::Success;
    }
};

#define CMM_EXTRACT_ARG(param_refl, value)                                        \
    static_cast<typename[:std::meta::type_of(param_refl):]>(                   \
        *static_cast<std::remove_cvref_t<typename[:std::meta::type_of(param_refl):]>*>( \
            (value).data()))


// Generates a thunk for both free functions and member functions
template <std::meta::info FuncRefl>
InvokerFn create_thunk() {
    return [](std::vector<Value>& args, Value& out) -> cmm::Error {
        static constexpr auto params = std::define_static_array(std::meta::parameters_of(FuncRefl));
        constexpr std::size_t num_params = params.size();
        constexpr bool is_member = std::meta::is_class_member(FuncRefl) && !std::meta::is_static_member(FuncRefl);
        constexpr std::size_t arg_offset = is_member ? 1 : 0;

        if (args.size() != num_params + arg_offset) {
            return cmm::Error::InvalidArgumentCount;
        }

        // Have to validate the instance pointer if it's a member function
        if constexpr (is_member) {
            constexpr cmm::info expected_instance_type = cmm::detail::hash_entity(
                std::meta::add_pointer(std::meta::parent_of(FuncRefl))
            );
            if (args[0].type_id() != expected_instance_type) {
                return cmm::Error::InvalidArgumentType;
            }
        }

        // Validate the arguments and invoke the function
        return []<std::size_t... Is>(std::vector<Value>& args, Value& out, std::index_sequence<Is...>) -> cmm::Error {
            bool args_valid = (is_argument_valid<std::meta::type_of(params[Is])>(args[Is + arg_offset]) && ...);
            if (!args_valid) {
                return cmm::Error::InvalidArgumentType;
            }

            auto do_invoke = [&]() {
                if constexpr (is_member) {
                    using ClassType = typename[:std::meta::parent_of(FuncRefl):];
                    auto* instance_ptr = *static_cast<ClassType**>(args[0].data());
                    return std::invoke(&[:FuncRefl:], instance_ptr, CMM_EXTRACT_ARG(params[Is], args[Is + 1])...);
                } else {
                    return std::invoke([:FuncRefl:], CMM_EXTRACT_ARG(params[Is], args[Is])...);
                }
            };

            using ReturnType = typename[:std::meta::return_type_of(FuncRefl):];
            if constexpr (std::is_void_v<ReturnType>) {
                do_invoke();
                out = Value{}; 
            } else {
                out = Value(do_invoke());
            }

            return cmm::Error::Success;

        }(args, out, std::make_index_sequence<num_params>{});
    };
}


// Generates a thunk specifically for constructors
// It's special because it has to allocate a new instance and return it
template <std::meta::info ConstructorRefl>
InvokerFn create_constructor_thunk() {
    return [](std::vector<Value>& args, Value& out) -> cmm::Error {
        static constexpr auto params = std::define_static_array(std::meta::parameters_of(ConstructorRefl));
        constexpr std::size_t num_params = params.size();

        if (args.size() != num_params) {
            return cmm::Error::InvalidArgumentCount;
        }

        return []<std::size_t... Is>(std::vector<Value>& args, Value& out, std::index_sequence<Is...>) -> cmm::Error {
            bool args_valid = (is_argument_valid<std::meta::type_of(params[Is])>(args[Is]) && ...);
            if (!args_valid) {
                return cmm::Error::InvalidArgumentType;
            }

            using ClassType = typename[:std::meta::parent_of(ConstructorRefl):];
            
            // The allocation of the new instance
            out = Value(new ClassType(CMM_EXTRACT_ARG(params[Is], args[Is])...));
            return cmm::Error::Success;
            
        }(args, out, std::make_index_sequence<num_params>{});
    };
}

#undef CMM_EXTRACT_ARG

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_THUNK_HPP