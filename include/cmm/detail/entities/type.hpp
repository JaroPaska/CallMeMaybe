#ifndef CALLMEMAYBE_TYPE_HPP
#define CALLMEMAYBE_TYPE_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include "cmm/detail/entities/entity.hpp"
#include "cmm/info.hpp"

namespace cmm {
namespace detail {

// Bitfield flags mirroring the std::meta type predicates
struct TypeFlags {
    bool is_void : 1 {false};
    bool is_null_pointer : 1 {false};
    bool is_integral : 1 {false};
    bool is_floating_point : 1 {false};
    bool is_arithmetic : 1 {false};
    bool is_fundamental : 1 {false};
    bool is_pointer : 1 {false};
    bool is_lvalue_reference : 1 {false};
    bool is_rvalue_reference : 1 {false};
    bool is_reference : 1 {false};
    bool is_class : 1 {false};
    bool is_union : 1 {false};
    bool is_enum : 1 {false};
    bool is_scoped_enum : 1 {false};
    bool is_array : 1 {false};
    bool is_function_type : 1 {false};
    bool is_const : 1 {false};
    bool is_volatile : 1 {false};
    bool is_signed : 1 {false};
    bool is_unsigned : 1 {false};
};

// Primitives and base for other types (Classes, Enums)
class Type : public Entity {
public:
    explicit Type(std::string_view name) : Entity(name) {}

    std::size_t size() const { return size_; }
    std::size_t alignment() const { return alignment_; }
    const TypeFlags& flags() const { return flags_; }

    // Type Peeling so if this is an int*, it returns the ID for int for example.
    // If this is an int[5], we also just get the int ID
    cmm::info underlying_type_id() const { return underlying_type_id_; }
    
    // For arrays
    std::size_t array_extent() const { return array_extent_; }

    void set_size(std::size_t s) { size_ = s; }
    void set_alignment(std::size_t a) { alignment_ = a; }
    void set_flags(const TypeFlags& f) { flags_ = f; }
    void set_underlying_type_id(cmm::info id) { underlying_type_id_ = id; }
    void set_array_extent(std::size_t ext) { array_extent_ = ext; }

protected:
    std::size_t size_{0};
    std::size_t alignment_{0};
    TypeFlags flags_{};
    
    // For pointers, references, and arrays to point to their base type
    cmm::info underlying_type_id_{cmm::invalid_info}; 
    std::size_t array_extent_{0};
};

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_TYPE_HPP