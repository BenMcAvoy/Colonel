#pragma once

#include "objects/UObject.h"
#include "objects/Player.h"
#include "basetypes.h"

namespace SDK {
    class AGameStateBase : public UObject {
    public:
		DEFINE_PROPERTY_GETTER(AGameStateBase, TArray<APlayerState*>, PlayerArray, _PlayerArray)

    private:
        /* 0x0000 */ char pad_0x0028[0x2F0];
		/* 0x0318 */ TArray<APlayerState*> _PlayerArray;
    };
}

