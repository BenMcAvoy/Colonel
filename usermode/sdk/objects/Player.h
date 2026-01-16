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

    struct FMeshBoneInfo {
        uint32_t NameIdx;
        int32_t ParentIdx;
        char pad_0x000[0x4];
    };

    struct FReferenceSkeleton {
        /* 0x000 */ TArray<FMeshBoneInfo> RawRefBoneInfo;
        /* 0x010 */ TArray<FTransform> RawRefBonePos;

        /* 0x020 */ TArray<FMeshBoneInfo> FinalRefBoneInfo;
        /* 0x030 */ TArray<FTransform> FinalRefBonePos;

        /* 0x040 */ TMap<FName, int32_t> RawNameToIndexMap;
        /* 0x040 */ TMap<FName, int32_t> FinalNameToIndexMap;
    };

    // Technically inherits from UStreamableRenderAsset
	class USkeletalMesh : public UObject {
    public:
		DEFINE_PROPERTY_GETTER(USkeletalMesh, FReferenceSkeleton, RefSkeleton, _RefSkeleton)

    private:
		/* 0x0028 */ char pad_0x0028[0x190];
		/* 0x01B8 */ FReferenceSkeleton _RefSkeleton;
    };

    enum class EVisibilityBasedAnimTickOption : uint8_t {
        AlwaysTickPoseAndRefreshBones = 0,
        AlwaysTickPose = 1,
        OnlyTickMontagesWhenNotRendered = 2,
        OnlyTickPoseWhenRendered = 3,
        EVisibilityBasedAnimTickOption_MAX = 4
    };

	class UPrimitiveComponent : public USceneComponent {};
	class UMeshComponent : public UPrimitiveComponent {};
    class USkinnedMeshComponent : public UMeshComponent {
    public:
		DEFINE_PROPERTY_GETTER(USkinnedMeshComponent, USkeletalMesh*, SkeletalMesh, _SkeletalMesh)

		__declspec(property(get = _getBoneArray)) TArray<FTransform> BoneArray;

		__declspec(property(get = _getRenderOptimizationFlag, put = _putRenderOptimizationFlag)) bool RenderOptimization;

        TArray<FTransform> _getBoneArray() {
			TArray<FTransform> transforms1 = CachedComponentSpaceTransforms1;
			return (transforms1.Count > 0) ? transforms1 : CachedComponentSpaceTransforms2;
		}

        bool _getRenderOptimizationFlag() const {
			// Flags bit 2 indicates RenderOptimization
			return (Flags & 0x4) != 0;
		}

        void _putRenderOptimizationFlag(bool value) {
			uint8_t flags = Flags;

			if (value) {
				flags |= 0x4; // Set bit 2
			}
            else {
                flags &= ~0x4; // Clear bit 2
            }

			dm.write<uint8_t>(reinterpret_cast<uintptr_t>(this) + offsetof(USkinnedMeshComponent, _Flags), flags);
        }

        DEFINE_PROPERTY_GETTER(USkinnedMeshComponent, TArray<FTransform>, CachedComponentSpaceTransforms1, _CachedComponentSpaceTransforms1);
        DEFINE_PROPERTY_GETTER(USkinnedMeshComponent, TArray<FTransform>, CachedComponentSpaceTransforms2, _CachedComponentSpaceTransforms2);

		DEFINE_PROPERTY_GETTER(USkinnedMeshComponent, uint8_t, Flags, _Flags);

		DEFINE_PROPERTY_GETSET(USkinnedMeshComponent, EVisibilityBasedAnimTickOption, VisibilityBasedAnimTickOption, _VisibilityBasedAnimTickOption);

    private:
		/* 0x0158 */ char pad_0x0158[0x5A8];
		/* 0x0700 */ USkeletalMesh* _SkeletalMesh;
		/* 0x0708 */ char pad_0x0708[0x18];
        /* 0x0720 */ TArray<FTransform> _CachedComponentSpaceTransforms1;
        /* 0x0730 */ TArray<FTransform> _CachedComponentSpaceTransforms2;
		/* 0x0740 */ char pad_0x0740[0x134];
        /* 0x0874 */ EVisibilityBasedAnimTickOption _VisibilityBasedAnimTickOption;
        /* 0x0878 */ uint8_t _Flags;
    };

    class USkeletalMeshComponent : public USkinnedMeshComponent { };

    class ACharacter : public APawn {
    public:
        DEFINE_PROPERTY_GETTER(ACharacter, USkeletalMeshComponent*, Mesh, _Mesh);

    private:
        /* 0x2D0 */ char pad_0x000[0x70];
        /* 0x070 */ USkeletalMeshComponent* _Mesh;
    };
}
