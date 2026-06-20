#ifndef CALLMEMAYBE_PARAMETER_HPP
#define CALLMEMAYBE_PARAMETER_HPP

#include <cstddef>
#include <string_view>
#include "cmm/info.hpp"
#include "cmm/detail/entities/entity.hpp"

namespace cmm {
namespace detail {

// Function arguments
class Parameter : public Entity {
public:
    constexpr Parameter(std::string_view name,
                 cmm::info type_id,
                 cmm::info parent_id,
                 std::size_t index)
        : Entity(name),
          type_id_(type_id),
          parent_id_(parent_id),
          index_(index) {}

    constexpr cmm::info type_id() const { return type_id_; }
    constexpr cmm::info parent_id() const { return parent_id_; }
    constexpr std::size_t index() const { return index_; }
    
    constexpr bool has_identifier() const { return !name_.empty(); }

    // Decayed (remove_cvref) ID of the parameter's type. Matches the ID a
    // Value reports for an argument, so overload / constructor resolution can
    // compare argument values against parameters safely
    constexpr cmm::info decayed_type_id() const { return decayed_type_id_; }
    constexpr void set_decayed_type_id(cmm::info id) { decayed_type_id_ = id; }

private:
    cmm::info type_id_{cmm::invalid_info};
    cmm::info parent_id_{cmm::invalid_info};
    std::size_t index_{0};
    
    cmm::info decayed_type_id_{cmm::invalid_info};
};

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_PARAMETER_HPP