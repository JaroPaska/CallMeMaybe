#ifndef CALLMEMAYBE_ENUMERATOR_HPP
#define CALLMEMAYBE_ENUMERATOR_HPP

#include <cstdint>
#include <string_view>
#include "cmm/info.hpp"
#include "cmm/detail/entities/entity.hpp"
#include "cmm/detail/entities/type.hpp"

namespace cmm {
namespace detail {

// Represents an individual enum value like Color::Red
// Inherits from Entity because it has a value and name, but isn't a type itself
class Enumerator : public Entity {
public:
    constexpr Enumerator(std::string_view name, std::int64_t value)
        : Entity(name), value_(value) {}

    constexpr std::int64_t value() const { return value_; }
    constexpr cmm::info parent_id() const { return parent_id_; }

    constexpr void set_value(std::int64_t v) { value_ = v; }
    constexpr void set_parent_id(cmm::info id) { parent_id_ = id; }

private:
    std::int64_t value_{0};
    cmm::info parent_id_{cmm::invalid_info};
};

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_ENUMERATOR_HPP