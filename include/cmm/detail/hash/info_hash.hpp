#ifndef CALLMEMAYBE_INFO_HASH_HPP
#define CALLMEMAYBE_INFO_HASH_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <meta>
#include "cmm/info.hpp"

namespace cmm {

namespace detail {

// FNV-1a 64-bit hash
// Creates a new hash or updates an existing hash with a new string value
// Available compile-time / runtime
constexpr cmm::info hash_string(std::string_view str, cmm::info hash = 0xcbf29ce484222325) {
    for (char c : str) {
        hash ^= static_cast<cmm::info>(c);
        hash *= 0x100000001b3;
    }
    return hash;
}

// Canonicalization helper to strip aliases.
// keep CV/Refs so that int, const int, and int& hash differently. This
// preserves signature fidelity (e.g. an int& out-parameter stays distinct
// from a by-value int); display_string_of already spells these uniquely.
consteval std::meta::info canonicalize_type(std::meta::info type_info) {
    std::meta::info t = type_info;
    std::meta::info prev;
    
    // Keep collapsing typedefs / using-aliases until we hit the real type
    do {
        prev = t;
        if (std::meta::is_type_alias(t)) {
            t = std::meta::dealias(t);
        }
    } while (t != prev);
    
    return t;
}

// Accepts any std::meta::info and generates the canonical FNV-1a hash
consteval cmm::info hash_entity(std::meta::info entity) {
    // Types, cvref qualifiers intact
    if (std::meta::is_type(entity)) {
        return hash_string(std::meta::display_string_of(canonicalize_type(entity)));
    }

    // Functions (also works for constructors, destructors, and operators)
    if (std::meta::is_function(entity)
        || std::meta::is_constructor(entity)
        || std::meta::is_destructor(entity)) {
        return hash_string(std::meta::display_string_of(entity));
    }

    // Remaining named entities (data members, parameters, variables, enumerators)
    // Combine the canonical type hash with the identifier
    cmm::info type_hash = hash_string(
        std::meta::display_string_of(canonicalize_type(std::meta::type_of(entity))));
    if (std::meta::has_identifier(entity)) {
        return hash_string(std::meta::identifier_of(entity), type_hash);
    }
    return type_hash;

    // NOTE: gcc display string includes namespaces, etc... but this function
    //  will lead to hash collisions with the old experimental clang.
    // The workaround would be to traverse entities manually, traverse parent namespaces
    //  etc to mangle them into the final hash, but this is unique in gcc and looks really nice
}

} // namespace detail

} // namespace cmm

#endif // CALLMEMAYBE_INFO_HASH_HPP
