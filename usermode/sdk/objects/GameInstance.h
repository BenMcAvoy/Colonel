#pragma once

#include "objects/UObject.h"
#include "objects/Player.h"
#include "basetypes.h"

namespace SDK {
    class UGameInstance : public UObject {
    public:
        DEFINE_PROPERTY_GETTER(UGameInstance, TArray<ULocalPlayer*>, LocalPlayers, _LocalPlayers)
    private:
        char pad_0x0028[0x10];
        TArray<ULocalPlayer*> _LocalPlayers;
    };
}
