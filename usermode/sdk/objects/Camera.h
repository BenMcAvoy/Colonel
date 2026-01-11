#pragma once

#include "objects/UObject.h"
#include "objects/ActorBase.h"
#include "objects/SceneComponent.h"

#include <imgui.h>

namespace SDK {
    struct FMinimalViewInfo {
        /* 0x0000 */ FVector Location;
        /* 0x000C */ FRotator Rotation;
        /* 0x0018 */ char pad_0x0018[0x68];
        /* 0x0080 */ float FOV;
        /* 0x0084 */ float DesiredFOV;
        /* 0x0088 */ float FirstPersonFOV;
        /* 0x008C */ float OrthoWidth;
        /* 0x0090 */ float OrthoNearClipPlane;
        /* 0x0094 */ float OrthoFarClipPlane;
        /* 0x0098 */ float AspectRatio;

        std::string ToString() {
			return std::format("FMinimalViewInfo(Location: {}, Rotation: {}, FOV: {}, DesiredFOV: {}, FirstPersonFOV: {}, OrthoWidth: {}, OrthoNearClipPlane: {}, OrthoFarClipPlane: {}, AspectRatio: {})",
				Location.ToString(), Rotation.ToString(), FOV, DesiredFOV, FirstPersonFOV, OrthoWidth, OrthoNearClipPlane, OrthoFarClipPlane, AspectRatio);
        }
    };

    struct FCameraCacheEntry {
        /* 0x0000 */ float Timestamp;
    private:
        /* 0x0004 */ char pad_0x0004[0xC];
    public:
        /* 0x0010 */ FMinimalViewInfo POV;

        std::string ToString() {
			return std::format("FCameraCacheEntry(Timestamp: {}, POV: {})", Timestamp, POV.ToString());
        }
    };

    class APlayerCameraManager : public AActor {
    public:
		DEFINE_PROPERTY_GETTER(APlayerCameraManager, FCameraCacheEntry, CameraCachePrivate, _CameraCachePrivate)

            std::optional<ImVec2> WorldToScreen(
                const FVector& WorldPos,
                uint32_t Width,
                uint32_t Height)
        {
			auto POV = CameraCachePrivate.POV;

            // Build view rotation matrix
            FMatrix View(POV.Rotation);
            //POV.Rotation.ToMatrix(View.M);

            // Extract axes exactly like your Rust
            const FVector VAxisX{
                View.M[0][0], View.M[0][1], View.M[0][2]
            };

            const FVector VAxisY{
                View.M[1][0], View.M[1][1], View.M[1][2]
            };

            const FVector VAxisZ{
                View.M[2][0], View.M[2][1], View.M[2][2]
            };

            // Delta from camera
            FVector Delta = WorldPos - POV.Location;

            // Transform into view space (same weird axis order)
            const FVector Transformed{
                Delta.Dot(VAxisY),
                Delta.Dot(VAxisZ),
                Delta.Dot(VAxisX)
            };

            // Near plane check
            if (Transformed.Z < 1.0f)
                return std::nullopt;

            const float ScreenCenterX = Width * 0.5f;
            const float ScreenCenterY = Height * 0.5f;

            // Same FOV gymnastics you used in Rust
            const float D =
                ScreenCenterY /
                std::tan((POV.FOV * 0.5f) * 3.14159265f / 180.0f);

            const float FOV =
                2.0f * std::atan(ScreenCenterX / D) * 180.0f / 3.14159265f;

            const float FOVRad = FOV * 3.14159265f / 360.0f;
            const float TanFOV = std::tan(FOVRad);

            ImVec2 Screen;
            Screen.x =
                ScreenCenterX +
                (Transformed.X * (ScreenCenterX / TanFOV)) / Transformed.Z;

            Screen.y =
                ScreenCenterY -
                (Transformed.Y * (ScreenCenterX / TanFOV)) / Transformed.Z;

            return Screen;
        }

    private:
        char pad_0x02D0[0x2B90];
        FCameraCacheEntry _CameraCachePrivate;
    };
}
