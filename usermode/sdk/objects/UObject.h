#pragma once

#include <cstdint>

#include "property_macros.h"

#include "Strings.h"

namespace SDK {
    class UClass {};

    class UObject {
    public:
        __declspec(property(get = _getVTable)) uintptr_t* VTable;
        uintptr_t* _getVTable() const {
            return dm.read<uintptr_t*>(reinterpret_cast<uintptr_t>(const_cast<UObject*>(this)));
        }

        DEFINE_PROPERTY_GETTER(UObject, int32_t, ObjectFlags, _ObjectFlags)
        DEFINE_PROPERTY_GETTER(UObject, int32_t, ObjectIndex, _ObjectIndex)
        DEFINE_PROPERTY_GETTER(UObject, uintptr_t, Class, _ClassPrivate)
        DEFINE_PROPERTY_GETTER(UObject, FName, ObjectName, _NamePrivate)
		DEFINE_PROPERTY_GETTER(UObject, UObject*, Outer, _OuterPrivate)

        uintptr_t getVFunction(size_t index) const {
            return dm.read<uintptr_t>(reinterpret_cast<uintptr_t>(VTable) + index * sizeof(uintptr_t));
        }

    private:
        uintptr_t* _VTable;
        uint32_t _ObjectFlags;
        uint32_t _ObjectIndex;
        UClass* _ClassPrivate;
        FName _NamePrivate;
        UObject* _OuterPrivate;
    };
}
