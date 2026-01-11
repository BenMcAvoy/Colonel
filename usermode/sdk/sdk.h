#pragma once

#include <cstdint>
#include <string>
#include <optional>

#include "driver.h"
#include "basetypes.h"
#include "property_macros.h"
#include "sdk_offsets.h"
#include "sdk_globals.h"

#include "Strings.h"

#include "objects/UObject.h"
#include "objects/SceneComponent.h"
#include "objects/ActorBase.h"
#include "objects/PlayerController.h"
#include "objects/Player.h"
#include "objects/GameInstance.h"
#include "objects/Level.h"
#include "objects/World.h"

namespace SDK {
	inline DriverManager dm("colonelLink");
	inline uintptr_t imgBase = 0;
	inline UWorld* GWorld = nullptr;
	inline FNamePool* GNamePool = nullptr;

	inline void Init() {
		dm.attachToProcess("SolarlandClient-Win64-Shipping.exe", false, &imgBase);
		GWorld = UWorld::Get();
		GNamePool = FNamePool::Get();
	}
}
