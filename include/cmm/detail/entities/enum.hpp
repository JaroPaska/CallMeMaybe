#ifndef CALLMEMAYBE_ENUM_HPP
#define CALLMEMAYBE_ENUM_HPP

#include <cstdint>
#include <span>
#include <string_view>
#include "cmm/info.hpp"
#include "cmm/detail/entities/entity.hpp"
#include "cmm/detail/entities/type.hpp"

namespace cmm {
namespace detail {

// Represents the Enum type itself like "enum class Color"
class Enum : public Type {
public:
    // Fast lookup for serializing without having to hit the global registry
    struct Entry {
        std::string_view name;
        std::int64_t value;
        cmm::info entity_id; // Link back to the Enumerator in the global registry
    };

    constexpr explicit Enum(std::string_view name) : Type(name) {
        flags_.is_enum = true; 
    }

    constexpr void set_enumerators(std::span<const Entry> s) { enumerators_ = s; }

    constexpr std::span<const Entry> enumerators() const { return enumerators_; }

    /*
    Runtime Reflection Fast-Paths
    */

    bool get_value_by_name(std::string_view name, std::int64_t& out_value) const {
        for (const auto& entry : enumerators_) {
            if (entry.name == name) {
                out_value = entry.value;
                return true;
            }
        }
        return false;
    }

    std::string_view get_name_by_value(std::int64_t value) const {
        for (const auto& entry : enumerators_) {
            if (entry.value == value) {
                return entry.name;
            }
        }
        return {}; // Returns empty string view if it's an invalid / unknown value
    }

private:
    std::span<const Entry> enumerators_;
};

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_ENUM_HPP