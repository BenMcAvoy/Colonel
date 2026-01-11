#pragma once

#include "objects/ActorBase.h"
#include "objects/Camera.h"

namespace SDK {
    class APlayerController : public AActor {
    public:
        DEFINE_PROPERTY_GETTER(APlayerController, APawn*, AcknowledgedPawn, _AcknowledgedPawn)
		DEFINE_PROPERTY_GETTER(APlayerController, APlayerCameraManager*, PlayerCameraManager, _PlayerCameraManager)

    private:
        /* 0x02D0 */ char pad_0x02D0[0xA8];
        /* 0x0378 */ APawn* _AcknowledgedPawn;
        /* 0x0380 */ char pad_0x0000[0x10];
        /* 0x0390 */ APlayerCameraManager* _PlayerCameraManager;
    };
}
