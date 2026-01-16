#pragma once

#include "objects/UObject.h"
#include "basetypes.h"

namespace SDK {
    class UActorComponent : public UObject {};

    class USceneComponent : public UActorComponent {
    public:
        DEFINE_PROPERTY_GETTER(USceneComponent, FVector, RelativeLocation, _RelativeLocation)
        DEFINE_PROPERTY_GETTER_AT(USkeletalMeshComponent, FTransform, ComponentToWorld, 0x280);

    private:
        char pad_0x0028[0x124];
        FVector _RelativeLocation;
    };
}
