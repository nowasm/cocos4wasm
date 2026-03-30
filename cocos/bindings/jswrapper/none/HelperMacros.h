#pragma once

#include "../ValueArrayPool.h"
#include "../config.h"
#include "base/Log.h"
#include "base/Macros.h"

#include <typeinfo>

#if SCRIPT_ENGINE_TYPE == SCRIPT_ENGINE_NONE

    #ifndef CC_DEBUG
        #undef RECORD_JSB_INVOKING
    #endif

    #define JsbInvokeScope(arg) \
        do {                    \
        } while (0)

template <typename T, typename STATE>
constexpr inline T *SE_THIS_OBJECT(STATE &s) { // NOLINT(readability-identifier-naming)
    return reinterpret_cast<T *>(s.nativeThisObject());
}

template <typename T>
constexpr typename std::enable_if<std::is_enum<T>::value, char *>::type SE_UNDERLYING_TYPE_NAME() { // NOLINT
    return const_cast<char *>(typeid(std::underlying_type_t<T>).name());
}

template <typename T>
constexpr typename std::enable_if<!std::is_enum<T>::value, char *>::type SE_UNDERLYING_TYPE_NAME() { // NOLINT
    return const_cast<char *>(typeid(T).name());
}

    #ifdef __GNUC__
        #define SE_UNUSED __attribute__((unused))
        #define SE_HOT    __attribute__((hot))
    #else
        #define SE_UNUSED
        #define SE_HOT
    #endif

    #define SAFE_INC_REF(obj) \
        if (obj != nullptr) obj->incRef()
    #define SAFE_DEC_REF(obj)   \
        if ((obj) != nullptr) { \
            (obj)->decRef();    \
            (obj) = nullptr;    \
        }

    #define _SE(name) name##Registry // NOLINT(readability-identifier-naming, bugprone-reserved-identifier)

    #define SE_DECLARE_FUNC(funcName) \
        bool funcName##Registry(se::State &s)

    #define SE_BIND_FUNC(funcName) \
        bool funcName##Registry(se::State &s) { return funcName(s); }

    #define SE_BIND_FUNC_FAST(funcName) \
        bool funcName##Registry(se::State &s) { \
            funcName(s.nativeThisObject());     \
            return true;                        \
        }

    #define SE_BIND_FINALIZE_FUNC(funcName)              \
        void funcName##Registry(se::Object *thisObject) { \
            if (thisObject == nullptr)                    \
                return;                                  \
            se::State state(thisObject);                  \
            funcName(state);                              \
        }

    #define SE_DECLARE_FINALIZE_FUNC(funcName) \
        void funcName##Registry(se::Object *thisObject);

    #define SE_BIND_CTOR(funcName, cls, finalizeCb) \
        bool funcName##Registry(se::State &s) { return funcName(s); }

    #define SE_BIND_PROP_GET_IMPL(funcName, postFix) \
        bool funcName##postFix##Registry(se::State &s) { return funcName(s); }

    #define SE_BIND_PROP_GET(funcName)         SE_BIND_PROP_GET_IMPL(funcName, )
    #define SE_BIND_FUNC_AS_PROP_GET(funcName) SE_BIND_PROP_GET_IMPL(funcName, _asGetter)

    #define SE_BIND_PROP_SET_IMPL(funcName, postFix) \
        bool funcName##postFix##Registry(se::State &s) { return funcName(s); }

    #define SE_BIND_PROP_SET(funcName)         SE_BIND_PROP_SET_IMPL(funcName, )
    #define SE_BIND_FUNC_AS_PROP_SET(funcName) SE_BIND_PROP_SET_IMPL(funcName, _asSetter)

    #define SE_TYPE_NAME(t) typeid(t).name()

    #define SE_QUOTEME_(x) #x // NOLINT
    #define SE_QUOTEME(x)  SE_QUOTEME_(x)

    #define SE_REPORT_ERROR(fmt, ...) selogMessage(cc::LogLevel::ERR, "[SE_ERROR]", (" (%s, %d): " fmt), __FILE__, __LINE__, ##__VA_ARGS__)

    #if CC_DEBUG > 0
        #define SE_ASSERT(cond, fmt, ...)                                                                                   \
            do {                                                                                                            \
                if (!(cond)) {                                                                                              \
                    selogMessage(cc::LogLevel::ERR, "[SE_ASSERT]", (" (%s, %d): " fmt), __FILE__, __LINE__, ##__VA_ARGS__); \
                    CC_ABORT();                                                                                             \
                }                                                                                                           \
            } while (false)
    #else
        #define SE_ASSERT(cond, fmt, ...)
    #endif

#endif // SCRIPT_ENGINE_TYPE == SCRIPT_ENGINE_NONE
