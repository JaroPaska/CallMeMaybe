#ifndef CMM_ERROR_HPP
#define CMM_ERROR_HPP

namespace cmm {

enum class Error {
    Success = 0,
    TypeMismatch,
    NullValue,
    EntityNotFound,
    MethodNotFound,
    InvalidArgumentCount,
    InvalidArgumentType,
    NotInvocable,
    ThunkNotInitialized,
    ConstViolation,
    StaticMismatch,
    BitFieldUnsupported
};

inline const char* to_string(Error err) noexcept {
    switch (err) {
        case Error::Success:              return "Success";
        case Error::TypeMismatch:         return "Type mismatch during extraction";
        case Error::NullValue:            return "Attempted to access a null value";
        case Error::EntityNotFound:       return "Entity not found in the registry";
        case Error::MethodNotFound:       return "Method not found on the target class";
        case Error::InvalidArgumentCount: return "Incorrect number of arguments provided for invocation";
        case Error::InvalidArgumentType:  return "Argument type mismatch during invocation";
        case Error::NotInvocable:         return "Target entity is not a function or constructor";
        case Error::ThunkNotInitialized:  return "Function thunk was not initialized";
        case Error::ConstViolation:       return "Attempted to modify a const or constexpr entity";
        case Error::StaticMismatch:       return "Static/non-static member access mismatch";
        case Error::BitFieldUnsupported:  return "Bit-fields are not supported for dynamic access";
        default:                          return "Unknown cmm::Error";
    }
}

} // namespace cmm

#endif // CMM_ERROR_HPP