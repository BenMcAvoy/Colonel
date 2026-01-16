#pragma once

#include <cstdint>
#include "driver.h"
#include "sdk_globals.h"

#define DEFINE_PROPERTY_GETTER(ClassType, Type, PropName, MemberName) \
    __declspec(property(get = _get##PropName)) Type PropName; \
    Type _get##PropName() const { \
        uintptr_t pThis = reinterpret_cast<uintptr_t>(this); \
        if (pThis == 0) { \
            std::string errMsg = "Attempted to access property '" #PropName "' on a null pointer of type '" #ClassType "'."; \
            throw DriverException(DriverStatus::ReadFailed, errMsg); \
        } \
        size_t offset = offsetof(ClassType, MemberName); \
        uintptr_t pValue = pThis + offset; \
        auto value = dm.read<Type>(pValue); \
        return value; \
    }

#define DEFINE_PROPERTY_GETSET(ClassType, Type, PropName, MemberName) \
    __declspec(property(get = _get##PropName, put = _put##PropName)) Type PropName; \
    Type _get##PropName() const { \
        uintptr_t pThis = reinterpret_cast<uintptr_t>(this); \
        if (pThis == 0) { \
            std::string errMsg = "Attempted to access property '" #PropName "' on a null pointer of type '" #ClassType "'."; \
            throw DriverException(DriverStatus::ReadFailed, errMsg); \
        } \
        size_t offset = offsetof(ClassType, MemberName); \
        uintptr_t pValue = pThis + offset; \
        auto value = dm.read<Type>(pValue); \
        return value; \
    } \
    void _put##PropName(Type value) { \
        uintptr_t pThis = reinterpret_cast<uintptr_t>(this); \
        if (pThis == 0) { \
            std::string errMsg = "Attempted to set property '" #PropName "' on a null pointer of type '" #ClassType "'."; \
            throw DriverException(DriverStatus::WriteFailed, errMsg); \
        } \
        size_t offset = offsetof(ClassType, MemberName); \
        uintptr_t pValue = pThis + offset; \
        dm.write<Type>(pValue, value); \
    }

// for hardcoded offset properties
#define DEFINE_PROPERTY_GETTER_AT(ClassType, Type, PropName, Offset) \
    __declspec(property(get = _get##PropName)) Type PropName; \
    Type _get##PropName() const { \
        uintptr_t pThis = reinterpret_cast<uintptr_t>(this); \
        if (pThis == 0) { \
            std::string errMsg = "Attempted to access property '" #PropName "' on a null pointer of type '" #ClassType "'."; \
            throw DriverException(DriverStatus::ReadFailed, errMsg); \
        } \
        uintptr_t pValue = pThis + Offset; \
        auto value = dm.read<Type>(pValue); \
        return value; \
    }
