#ifndef CALLMEMAYBE_CLASS_HPP
#define CALLMEMAYBE_CLASS_HPP

#include <span>
#include <string_view>
#include <utility>
#include "cmm/info.hpp"
#include "cmm/detail/entities/type.hpp"

namespace cmm {
namespace detail {

class Class : public Type {
public:
    constexpr explicit Class(std::string_view name) : Type(name) {
        flags_.is_class = true; // Auto-set base class flags
    }

    constexpr void set_nonstatic_data_members(std::span<const cmm::info> s) { nonstatic_data_members_ = s; }
    constexpr void set_static_data_members(std::span<const cmm::info> s) { static_data_members_ = s; }
    constexpr void set_functions(std::span<const cmm::info> s) { functions_ = s; }
    constexpr void set_constructors(std::span<const cmm::info> s) { constructors_ = s; }
    constexpr void set_bases(std::span<const cmm::info> s) { bases_ = s; }
    constexpr void set_destructor(cmm::info id) { destructor_ = id; }
    constexpr void set_members(std::span<const cmm::info> s) { members_ = s; }

    constexpr std::span<const cmm::info> nonstatic_data_members() const { return nonstatic_data_members_; }
    constexpr std::span<const cmm::info> static_data_members() const { return static_data_members_; }
    constexpr std::span<const cmm::info> functions() const { return functions_; }
    constexpr std::span<const cmm::info> constructors() const { return constructors_; }
    constexpr std::span<const cmm::info> bases() const { return bases_; }
    constexpr cmm::info destructor() const { return destructor_; }
    constexpr std::span<const cmm::info> members() const { return members_; }

    constexpr void set_member_names(std::span<const std::pair<std::string_view, cmm::info>> s) { member_name_index_ = s; }

    // Just for perf, average O(1) lookup
    constexpr cmm::info get_member_by_name(std::string_view name) const {
        for (const auto& pair : member_name_index_) {
            if (pair.first == name) return pair.second;
        }
        return cmm::invalid_info;
    }

private:
    std::span<const cmm::info> bases_;
    std::span<const cmm::info> constructors_;
    cmm::info destructor_{cmm::invalid_info};
    std::span<const cmm::info> functions_;
    std::span<const cmm::info> static_data_members_;
    std::span<const cmm::info> nonstatic_data_members_;
    std::span<const cmm::info> members_;
    std::span<const std::pair<std::string_view, cmm::info>> member_name_index_;
};

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_CLASS_HPP