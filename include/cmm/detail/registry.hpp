#ifndef CALLMEMAYBE_REGISTRY_HPP
#define CALLMEMAYBE_REGISTRY_HPP

#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <meta>

#include "cmm/annotations.hpp"
#include "cmm/info.hpp"
#include "cmm/error.hpp"
#include "cmm/detail/hash/info_hash.hpp"
#include "cmm/detail/entities/class.hpp"
#include "cmm/detail/entities/data_member.hpp"
#include "cmm/detail/entities/enum.hpp"
#include "cmm/detail/entities/enumerator.hpp"
#include "cmm/detail/entities/function.hpp"
#include "cmm/detail/entities/parameter.hpp"
#include "cmm/detail/entities/type.hpp"
#include "cmm/detail/entities/variable.hpp"
#include "cmm/detail/invocation/thunk.hpp"

// Define CMM_ENABLE_REGISTRY_LOGS to see full registration prints
#ifdef CMM_ENABLE_REGISTRY_LOGS
    #define CMM_REG_LOG(x) do { std::cout << x; } while(0)
#else
    #define CMM_REG_LOG(x) do {} while(0)
#endif

namespace cmm {
namespace detail {

// Make sure devs don't accidentally try to register unsupported entities
consteval bool is_registerable_entity(std::meta::info entity) {
    return std::meta::is_function(entity) ||
           std::meta::is_variable(entity) ||
           std::meta::is_type(entity);
}

class Registry {
public:
    using EntityVariant = std::variant<Type,
                                       Class,
                                       Enum,
                                       Variable,
                                       DataMember,
                                       Function,
                                       Parameter,
                                       Enumerator>;

    // Single global runtime reflection registry
    static Registry& instance() {
        static Registry inst;
        return inst;
    }

    // User-facing registration entry point
    // Protected by a constraint so invalid entities fail instantly at compile time
    template <std::meta::info EntityRefl>
    requires (is_registerable_entity(EntityRefl))
    cmm::Error register_entity() {
        cmm::info id = cmm::detail::hash_entity(EntityRefl);

        if (entity_registry_.contains(id)) return cmm::Error::Success;

        CMM_REG_LOG("Registering entity: " << std::meta::display_string_of(EntityRefl) << "\n");

        if constexpr (std::meta::is_function(EntityRefl)) {
            CMM_REG_LOG(" Registering free function: " << std::meta::display_string_of(EntityRefl) << "\n");
            return register_free_function<EntityRefl>(id);
        } else if constexpr (std::meta::is_variable(EntityRefl)) {
            CMM_REG_LOG(" Registering variable: " << std::meta::display_string_of(EntityRefl) << "\n");
            return register_variable<EntityRefl>(id);
        } else if constexpr (std::meta::is_class_type(EntityRefl) || std::meta::is_union_type(EntityRefl)) {
            CMM_REG_LOG(" Registering class: " << std::meta::display_string_of(EntityRefl) << "\n");
            return register_class<EntityRefl>(id);
        } else if constexpr (std::meta::is_enum_type(EntityRefl)) {
            CMM_REG_LOG(" Registering enum: " << std::meta::display_string_of(EntityRefl) << "\n");
            return register_enum<EntityRefl>(id);
        } else if constexpr (std::meta::is_type(EntityRefl)) {
            CMM_REG_LOG(" Registering type: " << std::meta::display_string_of(EntityRefl) << "\n");
            ensure_type_registered<EntityRefl>();
            return cmm::Error::Success;
        }

        // Should be unreachable from requires clause
        return cmm::Error::EntityNotFound; 
    }

    cmm::info get_id_by_name(std::string_view name) const {
        if (auto it = top_level_entities_.find(name); it != top_level_entities_.end()) {
            return it->second;
        }
        return cmm::invalid_info;
    }

    bool contains(cmm::info id) const {
        return entity_registry_.contains(id);
    }

    EntityVariant& get_entity(cmm::info id) {
        return entity_registry_.at(id);
    }

    const EntityVariant& get_entity(cmm::info id) const {
        return entity_registry_.at(id);
    }

    std::string_view get_entity_name(cmm::info id) const {
        if (id == cmm::invalid_info) return {};
        auto it = entity_registry_.find(id);
        if (it == entity_registry_.end()) return {};
        return std::visit([](auto&& arg) -> std::string_view { return arg.name(); }, it->second);
    }

private:
    // Transparent hash so callers can look up by std::string_view without
    //  constructing a temporary std::string on every query
    struct TransparentStringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
    };

    std::unordered_map<cmm::info, EntityVariant> entity_registry_;
    std::unordered_map<std::string, cmm::info, TransparentStringHash, std::equal_to<>> top_level_entities_;

    /*
    Type registrations
    */

    template <std::meta::info TypeRefl>
    cmm::info ensure_type_registered() {
        cmm::info id = cmm::detail::hash_entity(TypeRefl);
        if (entity_registry_.contains(id)) return id;

        // Create the correct type entity
        if constexpr (std::meta::is_class_type(TypeRefl) || std::meta::is_union_type(TypeRefl)) {
            entity_registry_.emplace(id, make_class_stub<TypeRefl>());
        } else if constexpr (std::meta::is_enum_type(TypeRefl)) {
            entity_registry_.emplace(id, make_enum_stub<TypeRefl>());
        } else {
            entity_registry_.emplace(id, make_type<TypeRefl>());
        }

        if constexpr (std::meta::is_fundamental_type(TypeRefl) || 
                      std::meta::is_enum_type(TypeRefl) ||
                      std::meta::is_class_type(TypeRefl)) {
            std::string name(std::meta::display_string_of(canonicalize_type(TypeRefl)));
            if (!name.empty()) {
                top_level_entities_.emplace(std::move(name), id);
            }
        }

        return id;
    }

    template <std::meta::info TypeRefl>
    Type make_type() {
        Type t(std::meta::display_string_of(TypeRefl));
        using T = typename[:TypeRefl:];
        using DecayedT = std::remove_cvref_t<T>;

        if constexpr (!std::is_void_v<DecayedT> && !std::is_function_v<DecayedT> && !std::is_reference_v<T>) {
            t.set_size(sizeof(T));
            t.set_alignment(alignof(T));
        }
        t.set_flags(make_type_flags<TypeRefl>());

        if constexpr (std::meta::is_pointer_type(TypeRefl)) {
            t.set_underlying_type_id(ensure_type_registered<std::meta::remove_pointer(TypeRefl)>());
        } else if constexpr (std::meta::is_reference_type(TypeRefl)) {
            t.set_underlying_type_id(ensure_type_registered<std::meta::remove_reference(TypeRefl)>());
        } else if constexpr (std::meta::is_array_type(TypeRefl)) {
            t.set_underlying_type_id(ensure_type_registered<std::meta::remove_extent(TypeRefl)>());
            if constexpr (std::meta::is_bounded_array_type(TypeRefl)) {
                t.set_array_extent(std::meta::extent(TypeRefl, 0));
            }
        }
        return t;
    }

    template <std::meta::info ClassRefl>
    static Class make_class_stub() {
        Class cls(std::meta::display_string_of(ClassRefl));
        using T = typename[:ClassRefl:];
        if constexpr (!std::is_void_v<T>) {
            cls.set_size(sizeof(T));
            cls.set_alignment(alignof(T));
        }
        cls.set_flags(make_type_flags<ClassRefl>());
        return cls;
    }

    template <std::meta::info EnumRefl>
    static Enum make_enum_stub() {
        Enum e(std::meta::display_string_of(EnumRefl));
        using T = typename[:EnumRefl:];
        e.set_size(sizeof(T));
        e.set_alignment(alignof(T));
        e.set_flags(make_type_flags<EnumRefl>());
        return e;
    }

    template <std::meta::info TypeRefl>
    static TypeFlags make_type_flags() {
        TypeFlags flags{};
        flags.is_void = std::meta::is_void_type(TypeRefl);
        flags.is_integral = std::meta::is_integral_type(TypeRefl);
        flags.is_floating_point = std::meta::is_floating_point_type(TypeRefl);
        flags.is_pointer = std::meta::is_pointer_type(TypeRefl);
        flags.is_reference = std::meta::is_reference_type(TypeRefl);
        flags.is_class = std::meta::is_class_type(TypeRefl);
        flags.is_enum = std::meta::is_enum_type(TypeRefl);
        flags.is_array = std::meta::is_array_type(TypeRefl);
        flags.is_const = std::meta::is_const_type(TypeRefl);
        return flags;
    }

    /*
    Function registration
    */

    template <std::meta::info FuncRefl>
    cmm::Error register_free_function(cmm::info id) {
        Function func(std::meta::identifier_of(FuncRefl));
        register_function_signature<FuncRefl>(func, id);
        func.set_thunk(cmm::detail::create_thunk<FuncRefl>());

        entity_registry_.emplace(id, std::move(func));
        top_level_entities_.insert_or_assign(std::string(std::meta::identifier_of(FuncRefl)), id);
        return cmm::Error::Success;
    }

    template <std::meta::info FuncRefl>
    void register_function_signature(Function& func, cmm::info func_id) {
        if constexpr (!std::meta::is_constructor(FuncRefl) && !std::meta::is_destructor(FuncRefl)) {
            constexpr std::meta::info ret_type_refl = std::meta::return_type_of(FuncRefl);
            func.set_return_type_id(ensure_type_registered<ret_type_refl>());
        }

        std::size_t idx = 0;
        template for (constexpr std::meta::info p : std::define_static_array(std::meta::parameters_of(FuncRefl))) {
            register_parameter<FuncRefl, p>(func_id, func, idx);
            ++idx;
        }
    }

    template <std::meta::info FuncRefl, std::meta::info ParamRefl>
    void register_parameter(cmm::info func_id, Function& func, std::size_t idx) {
        constexpr std::meta::info p_type_refl = std::meta::type_of(ParamRefl);
        cmm::info p_type_id = ensure_type_registered<p_type_refl>();

        constexpr std::meta::info p_decayed_refl = std::meta::remove_cvref(p_type_refl);
        cmm::info p_decayed_id = ensure_type_registered<p_decayed_refl>();

        cmm::info p_id = cmm::detail::hash_entity(ParamRefl);
        std::string_view p_name;
        if constexpr (std::meta::has_identifier(ParamRefl)) {
            p_name = std::meta::identifier_of(ParamRefl);
        }

        Parameter p(p_name, p_type_id, func_id, idx);
        p.set_decayed_type_id(p_decayed_id);
        
        entity_registry_.emplace(p_id, std::move(p));
        func.add_parameter_id(p_id);
    }

    /*
    Global Variable registration
    */

    template <std::meta::info VarRefl>
    cmm::Error register_variable(cmm::info var_id) {
        constexpr std::meta::info var_type_refl = std::meta::type_of(VarRefl);
        cmm::info type_id = ensure_type_registered<var_type_refl>();

        Variable var(std::meta::identifier_of(VarRefl), type_id);
        var.set_is_const(std::meta::is_const_type(var_type_refl));
        
        constexpr void* var_address =
            const_cast<void*>(static_cast<const void*>(&[:VarRefl:]));
        var.set_address(var_address);

        using VarT = std::remove_cvref_t<typename[:var_type_refl:]>;
        var.set_getter_thunk(&cmm::detail::StaticThunks<VarT>::get);
        var.set_ref_getter_thunk(&cmm::detail::StaticThunks<VarT>::get_ref);
        var.set_setter_thunk(&cmm::detail::StaticThunks<VarT>::set);

        entity_registry_.emplace(var_id, std::move(var));
        top_level_entities_.insert_or_assign(std::string(std::meta::identifier_of(VarRefl)), var_id);
        return cmm::Error::Success;
    }

    /*
    Enum registration
    */

    template <std::meta::info EnumRefl>
    cmm::Error register_enum(cmm::info enum_id) {
        ensure_type_registered<EnumRefl>(); 
        
        auto& e = std::get<Enum>(entity_registry_.at(enum_id));
        constexpr std::meta::info underlying = std::meta::underlying_type(EnumRefl);
        e.set_underlying_type_id(ensure_type_registered<underlying>());

        template for (constexpr std::meta::info enumerator : std::define_static_array(std::meta::enumerators_of(EnumRefl))) {
            cmm::info enumerator_id = cmm::detail::hash_entity(enumerator);

            std::string_view enumerator_name = std::meta::identifier_of(enumerator);
            auto val = static_cast<std::int64_t>([:enumerator:]);

            if (!entity_registry_.contains(enumerator_id)) {
                Enumerator en(enumerator_name, static_cast<std::int64_t>(val));
                entity_registry_.emplace(enumerator_id, std::move(en));
            }
            e.add_enumerator(enumerator_name, static_cast<std::int64_t>(val), enumerator_id);
        }
        
        top_level_entities_.insert_or_assign(std::string(std::meta::display_string_of(EnumRefl)), enum_id);
        return cmm::Error::Success;
    }

    /*
    Class registration
    */

    template <std::meta::info ClassRefl>
    cmm::Error register_class(cmm::info class_id) {
        ensure_type_registered<ClassRefl>();
        Class cls = make_class_stub<ClassRefl>();

        template for (constexpr std::meta::info base : std::define_static_array(std::meta::bases_of(ClassRefl, std::meta::access_context::unchecked()))) {
            cls.add_base(ensure_type_registered<std::meta::type_of(base)>());
        }

        template for (constexpr std::meta::info member : std::define_static_array(std::meta::members_of(ClassRefl, std::meta::access_context::unchecked()))) {
            if constexpr (!cmm::is_reflectable(member) && !std::meta::is_destructor(member)) {
                CMM_REG_LOG("  (skipped) Not reflectable: " << std::meta::display_string_of(member) << "\n");
                continue; 
            }

            cmm::info member_id = cmm::detail::hash_entity(member);
            if constexpr (std::meta::has_identifier(member)) {
                cls.add_member_name(std::meta::identifier_of(member), member_id);
            }
            CMM_REG_LOG(" Registering class member: " << std::meta::display_string_of(member) << "\n");

            if constexpr (std::meta::is_nonstatic_data_member(member)) {
                cmm::info mem_type_id = ensure_type_registered<std::meta::type_of(member)>();
                DataMember dm(std::meta::identifier_of(member), /*is_static=*/false);
                dm.set_type_id(mem_type_id);
                dm.set_parent_id(class_id);
                dm.set_offset_bytes(std::meta::offset_of(member).bytes);
                
                // Flag bitfields so dynamic invocation knows not to try retrieving a byte-address reference
                dm.set_is_bit_field(std::meta::is_bit_field(member));

                using MemT = std::remove_cvref_t<typename[:std::meta::type_of(member):]>;
                dm.set_getter_thunk(&cmm::detail::PropertyThunks<MemT>::get);
                dm.set_ref_getter_thunk(&cmm::detail::PropertyThunks<MemT>::get_ref);
                dm.set_setter_thunk(&cmm::detail::PropertyThunks<MemT>::set);

                entity_registry_.emplace(member_id, std::move(dm));
                cls.add_nonstatic_data_member(member_id);
            } else if constexpr (std::meta::is_constructor(member)) {
                Function ctor(std::meta::display_string_of(member), true, false);
                ctor.set_is_constructor(true);
                ctor.set_parent_id(class_id);
                register_function_signature<member>(ctor, member_id);
                ctor.set_thunk(cmm::detail::create_constructor_thunk<member>());
                entity_registry_.emplace(member_id, std::move(ctor));
                cls.add_constructor(member_id);
            } else if constexpr (std::meta::is_destructor(member)) {
                Function dtor(std::meta::display_string_of(member), true, false);
                dtor.set_is_destructor(true);
                dtor.set_parent_id(class_id);
                entity_registry_.emplace(member_id, std::move(dtor));
                cls.set_destructor(member_id);
            } else if constexpr (std::meta::is_function(member)) {
                constexpr bool is_static = std::meta::is_static_member(member);
                std::string_view fn_name;
                if constexpr (std::meta::has_identifier(member)) {
                    fn_name = std::meta::identifier_of(member);
                } else {
                    fn_name = std::meta::display_string_of(member);
                }

                Function fn(fn_name, true, is_static);
                fn.set_parent_id(class_id);
                register_function_signature<member>(fn, member_id);
                fn.set_thunk(cmm::detail::create_thunk<member>());
                entity_registry_.emplace(member_id, std::move(fn));
                cls.add_function(member_id);
            } else if constexpr (std::meta::is_static_member(member)) {
                cmm::info mem_type_id = ensure_type_registered<std::meta::type_of(member)>();
                DataMember dm(std::meta::identifier_of(member), /*is_static=*/true);
                dm.set_type_id(mem_type_id);
                dm.set_parent_id(class_id);

                constexpr void* mem_address =
                    const_cast<void*>(static_cast<const void*>(&[:member:]));
                dm.set_address(mem_address);

                using MemT = std::remove_cvref_t<typename[:std::meta::type_of(member):]>;
                dm.set_static_getter_thunk(&cmm::detail::StaticThunks<MemT>::get);
                dm.set_static_ref_getter_thunk(&cmm::detail::StaticThunks<MemT>::get_ref);
                dm.set_static_setter_thunk(&cmm::detail::StaticThunks<MemT>::set);

                entity_registry_.emplace(member_id, std::move(dm));
                cls.add_static_data_member(member_id);
            }
        }

        entity_registry_.insert_or_assign(class_id, std::move(cls));
        top_level_entities_.insert_or_assign(std::string(std::meta::display_string_of(ClassRefl)), class_id);
        
        return cmm::Error::Success;
    }
};

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_REGISTRY_HPP