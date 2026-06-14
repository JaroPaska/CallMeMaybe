#ifndef CALLMEMAYBE_CLASS_HPP
#define CALLMEMAYBE_CLASS_HPP

#include <string_view>
#include <vector>
#include "cmm/info.hpp"
#include "cmm/detail/entities/type.hpp"

namespace cmm {
namespace detail {

class Class : public Type {
public:
    explicit Class(std::string_view name) : Type(name) {
        flags_.is_class = true; // Auto-set base class flags
    }

    void add_nonstatic_data_member(cmm::info id) { nonstatic_data_members_.push_back(id); }
    void add_static_data_member(cmm::info id) { static_data_members_.push_back(id); }
    void add_function(cmm::info id) { functions_.push_back(id); }
    void add_constructor(cmm::info id) { constructors_.push_back(id); }
    void add_base(cmm::info id) { bases_.push_back(id); }
    void set_destructor(cmm::info id) { destructor_ = id; }

    const std::vector<cmm::info>& nonstatic_data_members() const { return nonstatic_data_members_; }
    const std::vector<cmm::info>& static_data_members() const { return static_data_members_; }
    const std::vector<cmm::info>& functions() const { return functions_; }
    const std::vector<cmm::info>& constructors() const { return constructors_; }
    const std::vector<cmm::info>& bases() const { return bases_; }
    cmm::info destructor() const { return destructor_; }

    // members: union of every class member kind, in registration order.
    // Mirrors std::meta::members_of for class types.
    std::vector<cmm::info> members() const {
        std::vector<cmm::info> out;
        out.reserve(constructors_.size() + functions_.size()
                  + nonstatic_data_members_.size() + static_data_members_.size()
                  + (destructor_ != cmm::invalid_info ? 1 : 0));
        
        out.insert(out.end(), constructors_.begin(), constructors_.end());
        
        if (destructor_ != cmm::invalid_info) {
            out.push_back(destructor_);
        }
        
        out.insert(out.end(), functions_.begin(), functions_.end());
        out.insert(out.end(), nonstatic_data_members_.begin(), nonstatic_data_members_.end());
        out.insert(out.end(), static_data_members_.begin(), static_data_members_.end());
        
        return out;
    }

private:
    std::vector<cmm::info> bases_;
    std::vector<cmm::info> constructors_;
    cmm::info destructor_{cmm::invalid_info};
    std::vector<cmm::info> functions_;
    std::vector<cmm::info> static_data_members_;
    std::vector<cmm::info> nonstatic_data_members_;
};

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_CLASS_HPP