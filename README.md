[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0)
[![GitHub stars](https://img.shields.io/github/stars/LaurieWired/CallMeMaybe)](https://github.com/LaurieWired/CallMeMaybe/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/LaurieWired/CallMeMaybe)](https://github.com/LaurieWired/CallMeMaybe/network/members)
[![GitHub contributors](https://img.shields.io/github/contributors/LaurieWired/CallMeMaybe)](https://github.com/LaurieWired/CallMeMaybe/graphs/contributors)
[![Follow @lauriewired](https://img.shields.io/twitter/follow/lauriewired?style=social)](https://twitter.com/lauriewired)

# CallMeMaybe

CallMeMaybe (CMM) is a C++ runtime reflection library built on top of [P2996](https://isocpp.org/files/papers/P2996R13.html) static reflection introduced in C++26. CMM purposefully mirrors many of the std::meta functions to provide a uniform interface, but allows runtime introspection, dynamic invocation, and instantiation by building a runtime reflection registry. Class members can be automatically traversed and reflected by simply adding `[[=cmm::reflectable]]` as an annotation. CMM implements a custom type system to completely avoid RTTI requirements.

## Usage

The library is available in the `include/cmm/` directory. To use it, copy `include/cmm` into your project and `#include "cmm/meta.hpp"`. A full example showing different scenarios (dynamic invocation, global variable modification, property iteration, and enums) can be found in `examples/basic_usage.cpp`. To use CMM:

1. **Tag your class**: Mark the methods, members, or constructors you want available at runtime with the `[[=cmm::reflectable]]` annotation. Unannotated members are ignored.
2. **Register the entity**: At startup, register your types into the global runtime registry using `cmm::register_rrefl<^^Type>()`.
3. **Reflect and Invoke**: Look up entities by string, dynamically instantiate them, and invoke their methods.

### Example 1

```cpp
#include <iostream>
#include <string>
#include "cmm/meta.hpp"

class Player {
public:
    // Annotated Constructor
    [[=cmm::reflectable]] Player(std::string name, int age) 
        : name_(std::move(name)), age_(age) {}

    // Annotated Method
    [[=cmm::reflectable]] std::string greet(const std::string& other) const {
        return name_ + " says: hello, " + other + "!";
    }

    // Unannotated method won't be registered
    void internal_helper() const {}

private:
    [[=cmm::reflectable]] std::string name_;
    [[=cmm::reflectable]] int age_;
};

int main() {
    // Register the class into the global runtime registry
    cmm::register_rrefl<^^Player>();

    cmm::info p_id = cmm::reflect_name("Player");
    cmm::info ctor_id = cmm::lookup::get_constructor<std::string, int>(p_id); 
    cmm::Value player_val = cmm::invoke(ctor_id, std::string("Alice"), 25);
    Player* p_ptr = player_val.get<Player*>(); 

    // Look up a method by string and dynamically invoke it
    cmm::info greet_id = cmm::lookup::get_member(p_id, "greet"); 
    std::string response = cmm::invoke<std::string>(greet_id, p_ptr, std::string("Bob"));
    
    std::cout << response << "\n"; // Output: "Alice says: hello, Bob!"

    delete p_ptr;
    return 0;
}
```

### Example 2

```cpp
#include <iostream>
#include <string>
#include "cmm/meta.hpp"

class Player {
public:
    Player(std::string name, int age) : name_(std::move(name)), age_(age) {}

private:
    // Only these members will have metadata generated and stored
    [[=cmm::reflectable]] std::string name_;
    [[=cmm::reflectable]] int age_;
    
    // Ignored by the reflection registry
    int cache_hash_ = 0; 
};

int main() {
    cmm::register_rrefl<^^Player>();
    cmm::info p_id = cmm::reflect_name("Player");
    std::cout << "Iterating properties of Player...\n";

    // Iterate over registered non-static data members
    // (mirrors std::meta::nonstatic_data_members_of)
    for (cmm::info mem : cmm::nonstatic_data_members_of(p_id)) {
        std::string_view name = cmm::identifier_of(mem);
        std::size_t offset = cmm::offset_of(mem);
        cmm::info type_id = cmm::type_of(mem);
        
        std::cout << "Field: " << name 
                  << "\n  -> Type ID: " << type_id
                  << "\n  -> Offset: " << offset << " bytes\n";
    }

    return 0;
}
```

## Build Example
To build and run the example code, run the following:

```
CXX=g++-16 cmake -B build
cmake --build build
./build/bin/basic_usage
```

## Important Usage Notes

- Currently requires GCC 16.1 with the -freflection flag (the experimental Clang version may lead to hash collisions right now)
- Callers are responsible for manually deleting instances created via dynamic constructor invocations
- Standard library containers and complex generic types are not natively supported by the reflection registry yet
- Free function overloads sharing the same identifier are not automatically disambiguated
