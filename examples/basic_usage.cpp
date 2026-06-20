//g++-16 -std=c++26 -freflection -fno-exceptions -fno-rtti -I include basic_usage.cpp -o basic_usage

#include <array>
#include <iostream>
#include <string>
#include <vector>
#include "cmm/meta.hpp"

/*
Test code for different scenarios
*/

enum class PlayerStatus {
    Offline = 0,
    Online = 1,
    Banned = 99
};

int g_max_players = 100;

using AgeAlias = int;

// Free function (No overloads)
int add(int a, int b) { return a + b; }

// Duplicated classes in different namespaces to test scoping resolution
namespace name1 {
    class Player {
    public:
        Player(std::string name, int level) : name_(name), level_(level) {}
    private:
        std::string name_;
        int level_;
    };
}

namespace name2 {
    class Player {
    public:
        Player(std::string name, int level) : name_(name), level_(level) {}
    private:
        std::string name_;
        int level_;
    };
}

class Player {
public:
    // Annotated Constructor
    [[=cmm::reflectable]] Player(std::string name, int age) : name_(std::move(name)), age_(age) {}

    // Annotated Methods
    [[=cmm::reflectable]] std::string get_name() const { return name_; }
    [[=cmm::reflectable]] int get_age() const { return age_; }
    [[=cmm::reflectable]] void set_age(int age) { age_ = age; }
    
    [[=cmm::reflectable]] std::string greet(const std::string& other) const {
        return name_ + " says: hello, " + other + "!";
    }

    // Unannotated method won't be registered
    void internal_helper() const {
        std::cout << "This should not appear in the registry.\n";
    }

private:
    // Annotated Data Members
    [[=cmm::reflectable]] std::string name_;
    [[=cmm::reflectable]] int age_;
    
    // Unannotated member won't be registered
    int cache_hash_ = 0; 
};

void random_function() {
    std::cout << "Random function called!\n";
}

struct random_struct {
    int a;
    int b;
};

/*
Testing different reflection queries / capabilities
*/

void test_top_level_lookup() {
    std::cout << "\nTest 1: String Lookup & Type Info\n";
    
    cmm::info p_id = cmm::reflect_name("Player");
    cmm::info int_id = cmm::reflect_name("int");

    std::cout << "Player ID valid: " << (p_id != cmm::invalid_info) << "\n";
    std::cout << "Player is class: " << cmm::is_class_type(p_id) << "\n";
    std::cout << "Player size: " << cmm::size_of(p_id) << "\n";
    std::cout << "'int' size: " << cmm::size_of(int_id) << "\n";
    std::cout << "'int' is integral: " << cmm::is_integral_type(int_id) << "\n";
}

void test_dynamic_instantiation_and_invocation() {
    std::cout << "\nTest 2: Dynamic Instantiation & Methods\n";
    
    cmm::info p_id = cmm::reflect_name("Player");
    std::cout << "Player ID: " << p_id << "\n";

    // Find the Constructor (std::string, int)
    cmm::info ctor_id = cmm::lookup::get_constructor<std::string, int>(p_id); 
    std::cout << "Found constructor ID: " << ctor_id << "\n";

    cmm::Value player_val = cmm::invoke(ctor_id, std::string("Alice"), 25);
    Player* p_ptr = player_val.get<Player*>(); 

    cmm::info greet_id = cmm::lookup::get_member(p_id, "greet"); 
    std::cout << "Found 'greet' method. Calling it dynamically...\n";
    std::string response = cmm::invoke<std::string>(greet_id, p_ptr, std::string("Bob"));
    std::cout << "  Result: " << response << "\n";

    cmm::info set_age_id = cmm::lookup::get_member(p_id, "set_age");
    cmm::info get_age_id = cmm::lookup::get_member(p_id, "get_age");
    
    std::cout << "Mutating state dynamically...\n";
    std::cout << "  Age before: " << cmm::invoke<int>(get_age_id, p_ptr) << "\n";
    cmm::invoke<void>(set_age_id, p_ptr, 99);
    std::cout << "  Age after:  " << cmm::invoke<int>(get_age_id, p_ptr) << "\n";

    // Clean up heap allocation from constructor thunk
    delete p_ptr;
}

void test_property_iteration() {
    std::cout << "\nTest 3: Property Iteration & Memory Offsets\n";
    
    cmm::info p_id = cmm::reflect_name("Player");
    Player p("Charlie", 30);
    
    for (cmm::info mem : cmm::nonstatic_data_members_of(p_id)) {
        std::string_view name = cmm::identifier_of(mem);
        std::size_t offset = cmm::offset_of(mem);
        cmm::info type_id = cmm::type_of(mem);
        
        std::cout << "Field: " << name 
                  << "\n  -> Type ID: " << type_id
                  << "\n  -> Offset: " << offset << " bytes\n";

        // Memory manipulation via standard offset math (mirroring std::meta)
        if (name == "age_") {
            int* age_ptr = reinterpret_cast<int*>(reinterpret_cast<char*>(&p) + offset);
            std::cout << "  -> Extracted value via raw offset memory: " << *age_ptr << "\n";
        }
    }
}

void test_global_variables_and_functions() {
    std::cout << "\nTest 4: Globals & Free Functions\n";
    
    // Free function
    cmm::info add_id = cmm::reflect_name("add");
    int sum = cmm::invoke<int>(add_id, 10, 20);
    std::cout << "10 + 20 dynamically invoked = " << sum << "\n";

    // Global variable
    cmm::info g_max_id = cmm::reflect_name("g_max_players");
    int* g_max_ptr = static_cast<int*>(cmm::address_of(g_max_id));
    
    std::cout << "g_max_players (before): " << *g_max_ptr << "\n";
    *g_max_ptr = 999; // dynamically set it
    std::cout << "g_max_players (after native read): " << g_max_players << "\n";
}

void test_enums() {
    std::cout << "\nTest 5: Enum Traversal & Lookup\n";
    
    cmm::info enum_id = cmm::reflect_name("PlayerStatus");
    
    std::cout << "All registered options for PlayerStatus:\n";
    for (cmm::info e : cmm::enumerators_of(enum_id)) {
        std::cout << "  " << cmm::display_string_of(e) << " = " << cmm::value_of(e) << "\n";
    }

    std::cout << "\nTesting Runtime String/Value Enum Conversions:\n";
    
    std::string_view name = cmm::lookup::enum_to_string(enum_id, 99);
    std::cout << "  Integer 99 maps to string: '" << name << "'\n";

    std::int64_t val = 0;
    if (cmm::lookup::string_to_enum(enum_id, "Online", val)) {
        std::cout << "  String 'Online' maps to integer: " << val << "\n";
    } else {
        std::cout << "  Failed to find 'Online'\n";
    }
}

void test_error_handling() {
    std::cout << "\nTest 6: Graceful Error Handling\n";

    // Setup a valid instance via safe invocation
    cmm::info p_id = cmm::reflect_name("Player");
    cmm::info ctor_id = cmm::lookup::get_constructor<std::string, int>(p_id);
    cmm::info greet_id = cmm::lookup::get_member(p_id, "greet");

    cmm::Value player_val;
    std::array<cmm::Value, 2> ctor_args{ cmm::Value(std::string("ErrorTester")), cmm::Value(10) };
    cmm::Error err = cmm::reflect_invoke(ctor_id, ctor_args, player_val);
    
    if (err != cmm::Error::Success) {
        std::cout << "  Failed to setup test: " << cmm::to_string(err) << "\n";
        return;
    }

    cmm::Value result;

    // Test Invalid Argument Count
    std::cout << "  Testing Invalid Argument Count...\n";
    // greet() requires (Instance*, const std::string&). We are skipping the string argument.
    std::array<cmm::Value, 1> too_few_args{ player_val };
    err = cmm::reflect_invoke(greet_id, too_few_args, result); 
    std::cout << "    Expected error, got: " << cmm::to_string(err) << "\n";

    // Test Invalid Argument Type
    std::cout << "  Testing Invalid Argument Type...\n";
    // Passing an int instead of a std::string to greet()
    std::array<cmm::Value, 2> wrong_type_args{ player_val, cmm::Value(42) };
    err = cmm::reflect_invoke(greet_id, wrong_type_args, result);
    std::cout << "    Expected error, got: " << cmm::to_string(err) << "\n";

    // Test Not Invocable Entity
    std::cout << "  Testing Not Invocable Entity...\n";
    // Trying to invoke the Player class ID itself instead of a function
    err = cmm::reflect_invoke(p_id, {}, result);
    std::cout << "    Expected error, got: " << cmm::to_string(err) << "\n";

    // Test Value Type Mismatch Extraction
    std::cout << "  Testing Value Type Mismatch Extraction...\n";
    cmm::Value int_val(100);
    std::string out_str;
    err = int_val.try_get<std::string>(out_str); // Attempting to get a string from an int value
    std::cout << "    Expected error, got: " << cmm::to_string(err) << "\n";

    delete player_val.get<Player*>();
}

CMM_BUILD_REGISTRY(
    ^^Player,
    ^^int,
    ^^random_function,
    ^^add,
    ^^random_struct,
    ^^name1::Player,
    ^^name2::Player,
    ^^PlayerStatus,
    ^^g_max_players
);

int main() {
    std::cout << "CallMeMaybe Reflection Test\n";

    test_top_level_lookup();
    test_dynamic_instantiation_and_invocation();
    test_property_iteration();
    test_global_variables_and_functions();
    test_enums();
    test_error_handling();

    std::cout << "\nAll tests completed successfully.\n";
    return 0;
}