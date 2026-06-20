#ifndef CALLMEMAYBE_META_HPP
#define CALLMEMAYBE_META_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>
#include <meta>
#include "cmm/error.hpp"
#include "cmm/info.hpp"
#include "cmm/detail/hash/info_hash.hpp"
#include "cmm/detail/registry.hpp"
#include "cmm/value.hpp"

namespace cmm {

// Note: For clarity, cmm uses the convention:
// "refl" suffix for static reflection (std::meta::info)
// "rrefl" suffix for runtime reflection (cmm::info)

/*
Top-Level Registration & Lookup
*/

// Mirroring std::meta::info
// Returns an opaque scalar handle into the runtime registry
template <std::meta::info EntityRefl>
consteval cmm::info get_id() {
    return detail::hash_entity(EntityRefl);
}

/*
Internal Helpers
*/
namespace detail {
    // Defined by CMM_BUILD_REGISTRY in exactly one translation unit.
    extern const RegistryView g_active_registry;

    inline const RegistryView& registry() { return g_active_registry; }

    inline bool valid(cmm::info i) {
        return i != invalid_info && registry().contains(i);
    }

    template <typename Visitor>
    inline auto visit_entity(cmm::info i, Visitor&& v) {
        return std::visit(std::forward<Visitor>(v), registry().get_entity(i));
    }
} // namespace detail

// Builds a constexpr registry from a list of reflections and defines the global registry.
// Use once at namespace scope in exactly one translation unit:
//   CMM_BUILD_REGISTRY(^^MyType, ^^AnotherType, ...);
// TODO: consider accepting plain names (CMM_BUILD_REGISTRY(MyType, ...)) and prepending ^^ in the macro expansion.
#define CMM_BUILD_REGISTRY(...)                                                      \
    static constexpr auto cmm_registry_data_ =                                      \
        cmm::detail::build_registry<__VA_ARGS__>();                                  \
    constexpr cmm::detail::RegistryView cmm::detail::g_active_registry {            \
        cmm_registry_data_ }

// Top-level entity lookup by name from the registry. Gotta have this, but it's like the
// equivalent for ^^ for reflecting the first entity. After this, callers can use
// the info handle for getting other entities and such.
inline cmm::info reflect_name(std::string_view name) {
    return detail::registry().get_id_by_name(name);
}


/*
Naming and Source-level Queries
*/

inline std::string_view identifier_of(cmm::info i) {
    if (!detail::valid(i)) return {};
    return detail::registry().get_entity_name(i);
}

inline std::string_view display_string_of(cmm::info i) {
    return identifier_of(i);
}

/*
Structural Traversal Queries
*/

inline cmm::info type_of(cmm::info i) {
    if (!detail::valid(i)) return invalid_info;
    return detail::visit_entity(i, [i](auto&& arg) -> cmm::info {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, detail::DataMember> ||
                      std::is_same_v<T, detail::Parameter> ||
                      std::is_same_v<T, detail::Variable>) {
            return arg.type_id();
        } else if constexpr (std::is_same_v<T, detail::Enumerator>) {
            return arg.parent_id(); // The type of an enumerator is its enum class
        } else if constexpr (std::is_base_of_v<detail::Type, T>) {
            return i; // A type *is* its own type
        } else {
            return invalid_info;
        }
    });
}

inline cmm::info parent_of(cmm::info i) {
    if (!detail::valid(i)) return invalid_info;
    return detail::visit_entity(i, [](auto&& arg) -> cmm::info {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, detail::DataMember> ||
                      std::is_same_v<T, detail::Function> ||
                      std::is_same_v<T, detail::Enumerator>) {
            return arg.parent_id();
        } else if constexpr (std::is_same_v<T, detail::Parameter>) {
            return arg.parent_id(); 
        } else {
            return invalid_info;
        }
    });
}

inline cmm::info underlying_type(cmm::info i) {
    if (!detail::valid(i)) return invalid_info;
    return detail::visit_entity(i, [](auto&& arg) -> cmm::info {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_base_of_v<detail::Type, T>) {
            return arg.underlying_type_id();
        } else {
            return invalid_info;
        }
    });
}

/*
Class and Enum Structure Queries
*/

inline std::span<const cmm::info> members_of(cmm::info i) {
    if (!detail::valid(i)) return {};
    const auto& entity = detail::registry().get_entity(i);
    if (auto* cls = std::get_if<detail::Class>(&entity)) return cls->members();
    return {};
}

inline std::span<const cmm::info> nonstatic_data_members_of(cmm::info i) {
    if (!detail::valid(i)) return {};
    const auto& entity = detail::registry().get_entity(i);
    if (auto* cls = std::get_if<detail::Class>(&entity)) return cls->nonstatic_data_members();
    return {};
}

inline std::span<const cmm::info> static_data_members_of(cmm::info i) {
    if (!detail::valid(i)) return {};
    const auto& entity = detail::registry().get_entity(i);
    if (auto* cls = std::get_if<detail::Class>(&entity)) return cls->static_data_members();
    return {};
}

inline std::span<const cmm::info> bases_of(cmm::info i) {
    if (!detail::valid(i)) return {};
    const auto& entity = detail::registry().get_entity(i);
    if (auto* cls = std::get_if<detail::Class>(&entity)) return cls->bases();
    return {};
}

inline std::vector<cmm::info> enumerators_of(cmm::info i) {
    if (!detail::valid(i)) return {};
    const auto& entity = detail::registry().get_entity(i);
    if (auto* e = std::get_if<detail::Enum>(&entity)) {
        std::vector<cmm::info> result;
        result.reserve(e->enumerators().size());
        for (const auto& entry : e->enumerators()) {
            result.push_back(entry.entity_id);
        }
        return result;
    }
    return {};
}

inline void* address_of(cmm::info i) {
    if (!detail::valid(i)) return nullptr;
    const auto& entity = detail::registry().get_entity(i);
    if (auto* var = std::get_if<detail::Variable>(&entity)) return var->address();
    return nullptr;
}

// Extracts the underlying integer value of a specific Enumerator ID
inline std::int64_t value_of(cmm::info i) {
    if (!detail::valid(i)) return 0;
    const auto& entity = detail::registry().get_entity(i);
    if (auto* enumerator = std::get_if<detail::Enumerator>(&entity)) {
        return enumerator->value();
    }
    return 0;
}

/*
Function Queries
*/

inline std::span<const cmm::info> parameters_of(cmm::info i) {
    if (!detail::valid(i)) return {};
    const auto& entity = detail::registry().get_entity(i);
    if (auto* func = std::get_if<detail::Function>(&entity)) return func->parameter_ids();
    return {};
}

inline cmm::info return_type_of(cmm::info i) {
    if (!detail::valid(i)) return invalid_info;
    const auto& entity = detail::registry().get_entity(i);
    if (auto* func = std::get_if<detail::Function>(&entity)) return func->return_type_id();
    return invalid_info;
}

/*
Layout & Identity Queries
*/

inline std::size_t size_of(cmm::info i) {
    if (!detail::valid(i)) return 0;
    return detail::visit_entity(i, [](auto&& arg) -> std::size_t {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_base_of_v<detail::Type, T>) return arg.size();
        return 0;
    });
}

inline std::size_t alignment_of(cmm::info i) {
    if (!detail::valid(i)) return 0;
    return detail::visit_entity(i, [](auto&& arg) -> std::size_t {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_base_of_v<detail::Type, T>) return arg.alignment();
        return 0;
    });
}

inline std::size_t offset_of(cmm::info i) {
    if (!detail::valid(i)) return 0;
    const auto& entity = detail::registry().get_entity(i);
    if (auto* dm = std::get_if<detail::DataMember>(&entity)) return dm->offset_bytes();
    return 0;
}

/*
Predicates (Mirroring std::meta::is_*)
*/

inline bool is_function(cmm::info i) {
    if (!detail::valid(i)) return false;
    return std::holds_alternative<detail::Function>(detail::registry().get_entity(i));
}

inline bool is_constructor(cmm::info i) {
    if (!is_function(i)) return false;
    return std::get<detail::Function>(detail::registry().get_entity(i)).is_constructor();
}

inline bool is_destructor(cmm::info i) {
    if (!is_function(i)) return false;
    return std::get<detail::Function>(detail::registry().get_entity(i)).is_destructor();
}

inline bool is_static_member(cmm::info i) {
    if (!detail::valid(i)) return false;
    return detail::visit_entity(i, [](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, detail::Function>) return arg.is_static_function();
        if constexpr (std::is_same_v<T, detail::DataMember>) return arg.is_static();
        return false;
    });
}

inline bool is_nonstatic_data_member(cmm::info i) {
    if (!detail::valid(i)) return false;
    const auto& e = detail::registry().get_entity(i);
    if (auto* dm = std::get_if<detail::DataMember>(&e)) return !dm->is_static();
    return false;
}

inline bool is_enumerator(cmm::info i) {
    if (!detail::valid(i)) return false;
    return std::holds_alternative<detail::Enumerator>(detail::registry().get_entity(i));
}

// Macro to quickly generate Type property predicates
#define CMM_DEFINE_TYPE_PREDICATE(name, flag)                          \
    inline bool name(cmm::info i) {                                    \
        if (!detail::valid(i)) return false;                           \
        return detail::visit_entity(i, [](auto&& arg) -> bool {        \
            using T = std::decay_t<decltype(arg)>;                     \
            if constexpr (std::is_base_of_v<detail::Type, T>) {        \
                return arg.flags().flag;                               \
            }                                                          \
            return false;                                              \
        });                                                            \
    }

CMM_DEFINE_TYPE_PREDICATE(is_void_type,             is_void)
CMM_DEFINE_TYPE_PREDICATE(is_null_pointer_type,     is_null_pointer)
CMM_DEFINE_TYPE_PREDICATE(is_integral_type,         is_integral)
CMM_DEFINE_TYPE_PREDICATE(is_floating_point_type,   is_floating_point)
CMM_DEFINE_TYPE_PREDICATE(is_arithmetic_type,       is_arithmetic)
CMM_DEFINE_TYPE_PREDICATE(is_fundamental_type,      is_fundamental)
CMM_DEFINE_TYPE_PREDICATE(is_pointer_type,          is_pointer)
CMM_DEFINE_TYPE_PREDICATE(is_lvalue_reference_type, is_lvalue_reference)
CMM_DEFINE_TYPE_PREDICATE(is_rvalue_reference_type, is_rvalue_reference)
CMM_DEFINE_TYPE_PREDICATE(is_reference_type,        is_reference)
CMM_DEFINE_TYPE_PREDICATE(is_class_type,            is_class)
CMM_DEFINE_TYPE_PREDICATE(is_union_type,            is_union)
CMM_DEFINE_TYPE_PREDICATE(is_enum_type,             is_enum)
CMM_DEFINE_TYPE_PREDICATE(is_scoped_enum_type,      is_scoped_enum)
CMM_DEFINE_TYPE_PREDICATE(is_array_type,            is_array)
CMM_DEFINE_TYPE_PREDICATE(is_function_type,         is_function_type)
CMM_DEFINE_TYPE_PREDICATE(is_const_type,            is_const)
CMM_DEFINE_TYPE_PREDICATE(is_volatile_type,         is_volatile)
CMM_DEFINE_TYPE_PREDICATE(is_signed_type,           is_signed)
CMM_DEFINE_TYPE_PREDICATE(is_unsigned_type,         is_unsigned)

#undef CMM_DEFINE_TYPE_PREDICATE


/*
Invocation (Mirroring std::meta::reflect_invoke)
*/

// Base invocation function. Takes a pre-packaged vector of Values and writes
// the result into out. Works for Functions, Methods (where args[0] is
// Instance*), and Constructors. Returns a cmm::Error saying the outcome
inline cmm::Error reflect_invoke(cmm::info target, std::span<Value> args, Value& out) {
    if (!detail::valid(target)) {
        return cmm::Error::EntityNotFound;
    }
    const auto& entity = detail::registry().get_entity(target);
    if (auto* func = std::get_if<detail::Function>(&entity)) {
        return func->invoke(args, out);
    }
    return cmm::Error::NotInvocable;
}

// User-friendly variadic wrapper. Automatically erases native C++ types into cmm::Values,
// performs the type-safe dispatch, and unerases the return type. Use reflect_invoke directly
// to inspect the cmm::Error
template <typename Ret = Value, typename... Args>
inline auto invoke(cmm::info target, Args&&... args) {
    Value result;
    cmm::Error err;

    if constexpr (sizeof...(Args) > 0) {
        std::array<Value, sizeof...(Args)> vals{ Value(std::forward<Args>(args))... };
        err = reflect_invoke(target, vals, result);
    } else {
        // Fast path for zero-argument functions
        err = reflect_invoke(target, {}, result);
    }

    assert(err == cmm::Error::Success && "cmm::invoke failed; use reflect_invoke to inspect the cmm::Error");
    (void)err;

    // Handle return type unpacking
    if constexpr (std::is_same_v<Ret, Value>) {
        return result; 
    } else if constexpr (std::is_void_v<Ret>) {
        return;
    } else {
        return result.template get<Ret>();
    }
}


/*
Helper Lookups. Cmm specific extensions that are just nice to have
when working with string names and such
*/
namespace lookup {

// Works for members with unique identifiers, otherwise use members_of directly
inline cmm::info get_member(cmm::info class_id, std::string_view name) {
    if (!detail::valid(class_id)) return invalid_info;
    const auto& entity = detail::registry().get_entity(class_id);
    
    if (auto* cls = std::get_if<detail::Class>(&entity)) {
        return cls->get_member_by_name(name);
    }
    return invalid_info;
}

// Finds a constructor matching exact parameter types (after cvref decay)
template <typename... Args>
inline cmm::info get_constructor(cmm::info class_id) {
    constexpr std::size_t N = sizeof...(Args);
    const cmm::info expected[] = { detail::hash_entity(^^std::remove_cvref_t<Args>)... };

    for (cmm::info m : members_of(class_id)) {
        if (!is_constructor(m)) continue;
        
        auto params = parameters_of(m);
        if (params.size() != N) continue;

        bool match = true;
        for (std::size_t i = 0; i < N; ++i) {
            auto param_decayed = detail::visit_entity(params[i], [](auto&& arg) -> cmm::info {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, detail::Parameter>) return arg.decayed_type_id();
                return invalid_info;
            });
            if (param_decayed != expected[i]) { match = false; break; }
        }
        if (match) return m;
    }
    return invalid_info;
}

// Converts an integer runtime value into its enum string name
inline std::string_view enum_to_string(cmm::info enum_type_id, std::int64_t value) {
    if (!detail::valid(enum_type_id)) return {};
    const auto& entity = detail::registry().get_entity(enum_type_id);
    if (auto* e = std::get_if<detail::Enum>(&entity)) {
        return e->get_name_by_value(value);
    }
    return {};
}

// Converts a string name back into the integer runtime enum value
inline bool string_to_enum(cmm::info enum_type_id, std::string_view name, std::int64_t& out_value) {
    if (!detail::valid(enum_type_id)) return false;
    const auto& entity = detail::registry().get_entity(enum_type_id);
    if (auto* e = std::get_if<detail::Enum>(&entity)) {
        return e->get_value_by_name(name, out_value);
    }
    return false;
}

} // namespace lookup

} // namespace cmm

#endif // CALLMEMAYBE_META_HPP