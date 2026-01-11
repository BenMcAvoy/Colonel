#pragma once

#include "objects/UObject.h"
#include "basetypes.h"

namespace SDK {
    class USceneComponent : public UObject {
    public:
        DEFINE_PROPERTY_GETTER(USceneComponent, FVector, RelativeLocation, _RelativeLocation)

    private:
        char pad_0x0028[0x124];
        FVector _RelativeLocation;
    };
}
