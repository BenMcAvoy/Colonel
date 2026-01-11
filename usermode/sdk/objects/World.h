#pragma once

#include "sdk_offsets.h"
#include "sdk_globals.h"

#include "objects/UObject.h"
#include "objects/Level.h"
#include "objects/GameInstance.h"
#include "objects/GameState.h"

#include "basetypes.h"

namespace SDK {
    class UWorld : public UObject {
    public:
        static UWorld* Get() {
            uintptr_t address = dm.read<uintptr_t>(imgBase + Offsets::GWorld_Offset);
            return reinterpret_cast<UWorld*>(address);
        }

        DEFINE_PROPERTY_GETTER(UWorld, ULevel*, PersistentLevel, _PersistentLevel)
		DEFINE_PROPERTY_GETTER(UWorld, AGameStateBase*, GameState, _GameState)
        DEFINE_PROPERTY_GETTER(UWorld, TArray<ULevel*>, Levels, _Levels)
        DEFINE_PROPERTY_GETTER(UWorld, UGameInstance*, OwningGameInstance, _OwningGameInstance)

    private:
        /* 0x0028 */ char _pad_0x0028[0x8];
        /* 0x0030 */ ULevel* _PersistentLevel;
        /* 0x0038 */ char _pad_0x0038[0x168];
		/* 0x01A0 */ AGameStateBase* _GameState;
		/* 0x01A8 */ char _pad_0x01A8[0x10];
        /* 0x01B8 */ TArray<ULevel*> _Levels;
        /* 0x01C8 */ char _pad_0x01C8[0x30];
        /* 0x01E8 */ UGameInstance* _OwningGameInstance;
    };
}
