#ifndef CALLMEMAYBE_ENTITY_HPP
#define CALLMEMAYBE_ENTITY_HPP

#include <string_view>
#include "cmm/info.hpp"

namespace cmm {
namespace detail {

/*
Parent class for types and values
*/
class Entity {
public:
    constexpr Entity() = default; // required: Type (first variant alternative) must be default-constructible
    constexpr explicit Entity(std::string_view name) : name_(name) {}

    constexpr std::string_view name() const {
        return name_;
    }

protected:
    std::string_view name_;
};

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_ENTITY_HPP