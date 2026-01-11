#pragma once

#include "objects/UObject.h"

namespace SDK {
    class UWorld;
	class AActor;

	class ULevelActorContainer : public UObject {
    public:
		DEFINE_PROPERTY_GETTER(ULevelActorContainer, TArray<AActor*>, Actors, _Actors)

    private:
		TArray<class AActor*> _Actors;
    };

    class ULevel : public UObject {
    public:
        DEFINE_PROPERTY_GETTER(ULevel, UWorld*, OwningWorld, _OwningWorld)
		DEFINE_PROPERTY_GETTER(ULevel, TArray<AActor*>, Actors, _Actors)

    private:
        /* 0x0028 */ char pad_0x0028[0x70];
		/* 0x0098 */ TArray<AActor*> _Actors;
		/* 0x00A8 */ char pad_0x00A8[0x10];
        /* 0x00B8 */ UWorld* _OwningWorld;
    };
}
