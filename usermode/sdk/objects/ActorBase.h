#pragma once

#include "objects/UObject.h"
#include "objects/SceneComponent.h"

namespace SDK {
	// An actor is a base class for all objects that can be placed in a level
    class AActor : public UObject {
    public:
        DEFINE_PROPERTY_GETTER(AActor, USceneComponent*, RootComponent, _RootComponent)

    private:
        char pad_0x0028[0x188];
        USceneComponent* _RootComponent;
        char pad_0x01B8[0x118];
    };
    static_assert(sizeof(AActor) == 0x2D0, "AActor size mismatch");

    // A pawn is a type of actor that can be controlled by a player or AI
	class APawn : public AActor {};

	class AInfo : public AActor {}; // ???
}
