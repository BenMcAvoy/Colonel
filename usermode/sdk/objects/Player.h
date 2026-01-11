#pragma once

#include "objects/UObject.h"
#include "objects/PlayerController.h"
#include "basetypes.h"

namespace SDK {
    class APlayerState : public AInfo {
    public:
        DEFINE_PROPERTY_GETTER(APlayerState, float, Score, _Score)
        DEFINE_PROPERTY_GETTER(APlayerState, int32_t, PlayerId, _PlayerId)
        DEFINE_PROPERTY_GETTER(APlayerState, uint8_t, Ping, _Ping)
        DEFINE_PROPERTY_GETTER(APlayerState, APawn*, Pawn, _Pawn)
        DEFINE_PROPERTY_GETTER(APlayerState, FString, PlayerNamePrivate, _PlayerNamePrivate)

    private:
		/* 0x02D0 */ char pad_0x02D0[0x10];
        /* 0x02E0 */ float _Score;
		/* 0x02E4 */ int32_t _PlayerId;
        /* 0x02E8 */ uint8_t _Ping;  
		/* 0x02E9 */ char pad_0x02E9[0x5F];
		/* 0x0348 */ APawn* _Pawn;
        /* 0x0350 */ char pad_0x0348[0x78];
        /* 0x03C8 */ FString _PlayerNamePrivate;
    };

    class AModularPlayerState : public APlayerState {}; // No reflected members
    
    // Farlight84 specific
    class ASolarPlayerState : public AModularPlayerState {
    public:
		DEFINE_PROPERTY_GETTER(ASolarPlayerState, FString, NickName, _NickName)

    private:
		/* 0x03D8 */ char pad_0x03D8[0xD0];
		/* 0x04A8 */ FString _NickName;
    };

    class UPlayer : public UObject {
    public:
        DEFINE_PROPERTY_GETTER(UPlayer, APlayerController*, PlayerController, _PlayerController)

    private:
        APlayerController* _PlayerController;
    };

    class ULocalPlayer : public UPlayer {
    public:
        DEFINE_PROPERTY_GETTER(ULocalPlayer, APlayerController*, PlayerController, _PlayerController)

    private:
        APlayerController* _PlayerController;
    };
}
