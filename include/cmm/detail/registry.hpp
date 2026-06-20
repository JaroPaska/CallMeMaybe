#ifndef CALLMEMAYBE_REGISTRY_HPP
#define CALLMEMAYBE_REGISTRY_HPP

#include <flat_map>
#include <iostream>
#include <ranges>
#include <string_view>
#include <type_traits>
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

// Forward declaration — defined after Registry
template <std::size_t N, std::size_t M>
struct RegistryData;

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

    // User-facing registration entry point
    // Protected by a constraint so invalid entities fail instantly at compile time
    template <std::meta::info EntityRefl>
    requires (is_registerable_entity(EntityRefl))
    consteval cmm::Error register_entity() {
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
    std::flat_map<cmm::info, EntityVariant> entity_registry_;
    std::flat_map<std::string_view, cmm::info> top_level_entities_;

    /*
    Type registrations
    */

    template <std::meta::info TypeRefl>
    consteval cmm::info ensure_type_registered() {
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
            std::string_view name = std::meta::display_string_of(canonicalize_type(TypeRefl));
            if (!name.empty()) {
                top_level_entities_.emplace(name, id);
            }
        }

        return id;
    }

    template <std::meta::info TypeRefl>
    consteval Type make_type() {
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

    // Lightweight placeholder used when a class type is first encountered as a dependency
    template <std::meta::info ClassRefl>
    static consteval Class make_class_stub() {
        Class cls(std::meta::display_string_of(ClassRefl));
        using T = typename[:ClassRefl:];
        if constexpr (!std::is_void_v<T>) {
            cls.set_size(sizeof(T));
            cls.set_alignment(alignof(T));
        }
        cls.set_flags(make_type_flags<ClassRefl>());
        return cls;
    }

    // Full class build: computes all member/base spans. Used only in register_class.
    template <std::meta::info ClassRefl>
    static consteval Class make_class_full() {
        Class cls = make_class_stub<ClassRefl>();

        static constexpr auto base_ids = std::define_static_array(
            std::meta::bases_of(ClassRefl, std::meta::access_context::unchecked())
            | std::views::transform([](std::meta::info b) { return hash_entity(std::meta::type_of(b)); })
        );
        cls.set_bases(base_ids);

        static constexpr auto nonstatic_ids = std::define_static_array(
            std::meta::members_of(ClassRefl, std::meta::access_context::unchecked())
            | std::views::filter([](std::meta::info m) { return cmm::is_reflectable(m) && std::meta::is_nonstatic_data_member(m); })
            | std::views::transform([](std::meta::info m) { return hash_entity(m); })
        );
        cls.set_nonstatic_data_members(nonstatic_ids);

        static constexpr auto static_ids = std::define_static_array(
            std::meta::members_of(ClassRefl, std::meta::access_context::unchecked())
            | std::views::filter([](std::meta::info m) { return cmm::is_reflectable(m) && std::meta::is_static_member(m) && !std::meta::is_function(m); })
            | std::views::transform([](std::meta::info m) { return hash_entity(m); })
        );
        cls.set_static_data_members(static_ids);

        static constexpr auto function_ids = std::define_static_array(
            std::meta::members_of(ClassRefl, std::meta::access_context::unchecked())
            | std::views::filter([](std::meta::info m) { return cmm::is_reflectable(m) && std::meta::is_function(m) && !std::meta::is_constructor(m) && !std::meta::is_destructor(m); })
            | std::views::transform([](std::meta::info m) { return hash_entity(m); })
        );
        cls.set_functions(function_ids);

        static constexpr auto constructor_ids = std::define_static_array(
            std::meta::members_of(ClassRefl, std::meta::access_context::unchecked())
            | std::views::filter([](std::meta::info m) { return cmm::is_reflectable(m) && std::meta::is_constructor(m); })
            | std::views::transform([](std::meta::info m) { return hash_entity(m); })
        );
        cls.set_constructors(constructor_ids);

        static constexpr auto destructor_id = []() consteval {
            cmm::info id = cmm::invalid_info;
            template for (constexpr std::meta::info m : std::define_static_array(std::meta::members_of(ClassRefl, std::meta::access_context::unchecked())))
                if (std::meta::is_destructor(m))
                    id = hash_entity(m);
            return id;
        }();
        static constexpr auto all_member_ids = []() consteval {
            constexpr bool has_dtor = (destructor_id != cmm::invalid_info);
            constexpr std::size_t N = constructor_ids.size() + (has_dtor ? 1 : 0) + function_ids.size() + nonstatic_ids.size() + static_ids.size();
            std::array<cmm::info, N> result{};
            std::size_t i = 0;
            for (auto id : constructor_ids) result[i++] = id;
            if constexpr (has_dtor) result[i++] = destructor_id;
            for (auto id : function_ids) result[i++] = id;
            for (auto id : nonstatic_ids) result[i++] = id;
            for (auto id : static_ids) result[i++] = id;
            return result;
        }();
        cls.set_members(all_member_ids);

        // std::string_view is not structural so define_static_array can't store pairs;
        // use the consteval lambda + std::array approach instead
        static constexpr auto member_names = []() consteval {
            constexpr std::size_t N = []() consteval {
                std::size_t n = 0;
                template for (constexpr std::meta::info m : std::define_static_array(std::meta::members_of(ClassRefl, std::meta::access_context::unchecked())))
                    if ((cmm::is_reflectable(m) || std::meta::is_destructor(m)) && std::meta::has_identifier(m))
                        ++n;
                return n;
            }();
            std::array<std::pair<std::string_view, cmm::info>, N> result{};
            std::size_t i = 0;
            template for (constexpr std::meta::info m : std::define_static_array(std::meta::members_of(ClassRefl, std::meta::access_context::unchecked())))
                if ((cmm::is_reflectable(m) || std::meta::is_destructor(m)) && std::meta::has_identifier(m))
                    result[i++] = {std::meta::identifier_of(m), hash_entity(m)};
            return result;
        }();
        cls.set_member_names(member_names);

        return cls;
    }

    template <std::meta::info EnumRefl>
    static consteval Enum make_enum_stub() {
        Enum e(std::meta::display_string_of(EnumRefl));
        using T = typename[:EnumRefl:];
        e.set_size(sizeof(T));
        e.set_alignment(alignof(T));
        e.set_flags(make_type_flags<EnumRefl>());
        return e;
    }

    template <std::meta::info TypeRefl>
    static consteval TypeFlags make_type_flags() {
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
    consteval cmm::Error register_free_function(cmm::info id) {
        Function func(std::meta::identifier_of(FuncRefl));
        register_function_signature<FuncRefl>(func, id);
        func.set_thunk(cmm::detail::create_thunk<FuncRefl>());

        entity_registry_.emplace(id, std::move(func));
        top_level_entities_.insert_or_assign(std::meta::identifier_of(FuncRefl), id);
        return cmm::Error::Success;
    }

    template <std::meta::info FuncRefl>
    consteval void register_function_signature(Function& func, cmm::info func_id) {
        if constexpr (!std::meta::is_constructor(FuncRefl) && !std::meta::is_destructor(FuncRefl)) {
            constexpr std::meta::info ret_type_refl = std::meta::return_type_of(FuncRefl);
            func.set_return_type_id(ensure_type_registered<ret_type_refl>());
        }

        static constexpr auto param_ids = std::define_static_array(
            std::meta::parameters_of(FuncRefl)
            | std::views::transform([](std::meta::info p) { return hash_entity(p); })
        );
        func.set_parameter_ids(param_ids);

        std::size_t idx = 0;
        template for (constexpr std::meta::info p : std::define_static_array(std::meta::parameters_of(FuncRefl))) {
            register_parameter<FuncRefl, p>(func_id, idx);
            ++idx;
        }
    }

    template <std::meta::info FuncRefl, std::meta::info ParamRefl>
    consteval void register_parameter(cmm::info func_id, std::size_t idx) {
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
    }

    /*
    Global Variable registration
    */

    template <std::meta::info VarRefl>
    consteval cmm::Error register_variable(cmm::info var_id) {
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
        top_level_entities_.insert_or_assign(std::meta::identifier_of(VarRefl), var_id);
        return cmm::Error::Success;
    }

    /*
    Enum registration
    */

    template <std::meta::info EnumRefl>
    consteval cmm::Error register_enum(cmm::info enum_id) {
        ensure_type_registered<EnumRefl>(); 
        
        auto& e = std::get<Enum>(entity_registry_.at(enum_id));
        constexpr std::meta::info underlying = std::meta::underlying_type(EnumRefl);
        e.set_underlying_type_id(ensure_type_registered<underlying>());

        // [:enumerator:] splicer requires a constexpr value, so entries are built
        // via a consteval lambda rather than a pipeline
        static constexpr auto entries = []() consteval {
            constexpr std::size_t N = std::meta::enumerators_of(EnumRefl).size();
            std::array<Enum::Entry, N> result{};
            std::size_t i = 0;
            template for (constexpr std::meta::info e : std::define_static_array(std::meta::enumerators_of(EnumRefl)))
                result[i++] = {std::meta::identifier_of(e), static_cast<std::int64_t>([:e:]), hash_entity(e)};
            return result;
        }();
        e.set_enumerators(entries);

        template for (constexpr std::meta::info enumerator : std::define_static_array(std::meta::enumerators_of(EnumRefl))) {
            cmm::info enumerator_id = cmm::detail::hash_entity(enumerator);

            std::string_view enumerator_name = std::meta::identifier_of(enumerator);
            auto val = static_cast<std::int64_t>([:enumerator:]);

            if (!entity_registry_.contains(enumerator_id)) {
                Enumerator en(enumerator_name, static_cast<std::int64_t>(val));
                entity_registry_.emplace(enumerator_id, std::move(en));
            }
        }
        
        top_level_entities_.insert_or_assign(std::meta::display_string_of(EnumRefl), enum_id);
        return cmm::Error::Success;
    }

    /*
    Class registration
    */

    template <std::meta::info ClassRefl>
    consteval cmm::Error register_class(cmm::info class_id) {
        ensure_type_registered<ClassRefl>();
        Class cls = make_class_full<ClassRefl>();

        template for (constexpr std::meta::info base : std::define_static_array(std::meta::bases_of(ClassRefl, std::meta::access_context::unchecked()))) {
            ensure_type_registered<std::meta::type_of(base)>();
        }

        template for (constexpr std::meta::info member : std::define_static_array(std::meta::members_of(ClassRefl, std::meta::access_context::unchecked()))) {
            if constexpr (!cmm::is_reflectable(member) && !std::meta::is_destructor(member)) {
                CMM_REG_LOG("  (skipped) Not reflectable: " << std::meta::display_string_of(member) << "\n");
                continue;
            }

            cmm::info member_id = cmm::detail::hash_entity(member);
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
            } else if constexpr (std::meta::is_constructor(member)) {
                Function ctor(std::meta::display_string_of(member), true, false);
                ctor.set_is_constructor(true);
                ctor.set_parent_id(class_id);
                register_function_signature<member>(ctor, member_id);
                ctor.set_thunk(cmm::detail::create_constructor_thunk<member>());
                entity_registry_.emplace(member_id, std::move(ctor));
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
            }
        }

        entity_registry_.insert_or_assign(class_id, std::move(cls));
        top_level_entities_.insert_or_assign(std::meta::display_string_of(ClassRefl), class_id);
        
        return cmm::Error::Success;
    }

public:
    consteval std::size_t entity_count() const { return entity_registry_.size(); }
    consteval std::size_t name_count() const { return top_level_entities_.size(); }

    template <std::size_t N, std::size_t M>
    consteval void fill(RegistryData<N, M>& out) const;
};

// Constexpr-compatible registry storage: two sorted arrays, no heap allocation.
// TODO: consider SoA layout (separate key/value arrays) — binary search only touches keys,
// and EntityVariant is large, so AoS wastes cache lines during lookup.
template <std::size_t N, std::size_t M>
struct RegistryData {
    using EntityVariant = Registry::EntityVariant;
    std::array<std::pair<cmm::info, EntityVariant>, N> entities;
    std::array<std::pair<std::string_view, cmm::info>, M> names;
};

// Type-erased runtime view — holds spans into a constexpr RegistryData.
class RegistryView {
public:
    using EntityVariant = Registry::EntityVariant;

    RegistryView() = default;

    template <std::size_t N, std::size_t M>
    constexpr explicit RegistryView(const RegistryData<N, M>& data) noexcept
        : entities_(data.entities), names_(data.names) {}

    bool contains(cmm::info id) const {
        if (id == cmm::invalid_info) return false;
        auto it = std::lower_bound(entities_.begin(), entities_.end(), id,
            [](const auto& p, cmm::info i) { return p.first < i; });
        return it != entities_.end() && it->first == id;
    }

    const EntityVariant& get_entity(cmm::info id) const {
        auto it = std::lower_bound(entities_.begin(), entities_.end(), id,
            [](const auto& p, cmm::info i) { return p.first < i; });
        return it->second;
    }

    std::string_view get_entity_name(cmm::info id) const {
        if (!contains(id)) return {};
        return std::visit([](const auto& e) -> std::string_view { return e.name(); }, get_entity(id));
    }

    cmm::info get_id_by_name(std::string_view name) const {
        auto it = std::lower_bound(names_.begin(), names_.end(), name,
            [](const auto& p, std::string_view n) { return p.first < n; });
        if (it != names_.end() && it->first == name) return it->second;
        return cmm::invalid_info;
    }

private:
    std::span<const std::pair<cmm::info, EntityVariant>> entities_;
    std::span<const std::pair<std::string_view, cmm::info>> names_;
};

// Out-of-class definition of Registry::fill (needs RegistryData to be complete)
template <std::size_t N, std::size_t M>
consteval void Registry::fill(RegistryData<N, M>& out) const {
    std::size_t i = 0;
    for (const auto& [k, v] : entity_registry_)
        out.entities[i++] = {k, v};
    i = 0;
    for (const auto& [k, v] : top_level_entities_)
        out.names[i++] = {k, v};
}

// Two-pass consteval builder: phase 1 counts using transient Registry (flat_map freed on
// return), phase 2 fills fixed-size RegistryData arrays (no heap allocation in result).
template <std::meta::info... EntityRefls>
consteval auto build_registry() {
    constexpr auto counts = []() consteval {
        Registry reg;
        (reg.register_entity<EntityRefls>(), ...);
        return std::pair{reg.entity_count(), reg.name_count()};
    }();
    constexpr std::size_t N = counts.first;
    constexpr std::size_t M = counts.second;

    RegistryData<N, M> result{};
    {
        Registry reg;
        (reg.register_entity<EntityRefls>(), ...);
        reg.fill(result);
    }
    return result;
}

} // namespace detail
} // namespace cmm

#endif // CALLMEMAYBE_REGISTRY_HPP