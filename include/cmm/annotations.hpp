#ifndef CALLMEMAYBE_ANNOTATIONS_HPP
#define CALLMEMAYBE_ANNOTATIONS_HPP

#include <meta>

namespace cmm {

struct reflectable_t { };

// Tag members with [[=cmm::reflectable]] to have them be registered for refleciton
// Otherwise, they will be ignored
inline constexpr reflectable_t reflectable{};

consteval bool is_reflectable(std::meta::info entity) {
    return !std::meta::annotations_of_with_type(entity, ^^reflectable_t).empty();
}

} // namespace cmm

#endif // CALLMEMAYBE_ANNOTATIONS_HPP