#include "sdk/sdk.h"

#include "window.h"

#include <imgui.h>
#include <chrono>
#include <iostream>
#include <format>
#include <print>
#include <algorithm>

#include "render.h"

std::pair<uint32_t, uint32_t> SkeletonPairs[] =
{
    // Spine
    {2, 3},            // pelvis -> spine_01
    {3, 4},            // spine_01 -> spine_02
    {4, 5},            // spine_02 -> spine_03
    {5, 6},            // spine_03 -> neck_01

    // Left arm
    {5, 8},            // spine_03 -> clavicle_l
    {8, 9},            // clavicle_l -> upperarm_l
    {9, 10},           // upperarm_l -> lowerarm_l
    {10, 11},          // lowerarm_l -> hand_l

    // Right arm
    {5, 28},           // spine_03 -> clavicle_r
    {28, 29},          // clavicle_r -> upperarm_r
    {29, 30},          // upperarm_r -> lowerarm_r
    {30, 31},          // lowerarm_r -> hand_r

    // Left leg
    {2, 51},           // pelvis -> thigh_l
    {51, 52},          // thigh_l -> calf_l
    {52, 53},          // calf_l -> foot_l
    {53, 54},          // foot_l -> ball_l

    // Right leg
    {2, 55},           // pelvis -> thigh_r
    {55, 56},          // thigh_r -> calf_r
    {56, 57},          // calf_r -> foot_r
    {57, 58},          // foot_r -> ball_r
};

void DumpBones() {
	auto localPawn =
		SDK::GWorld
		->GameState
		->PlayerArray[3]
		->Pawn;

	auto localChar = static_cast<SDK::ACharacter*>(localPawn);

	auto boneCount = localChar->Mesh->BoneArray.Count;
	std::println("CachedComponentSpaceTransforms bone count: {}", boneCount);

	auto refSkel = localChar->Mesh->SkeletalMesh->RefSkeleton;
	std::println("Bone count: {}", refSkel.RawRefBoneInfo.Count);

	size_t boneIndex = 0;
	for (auto boneInfo : refSkel.RawRefBoneInfo) {
		try {
			SDK::FName boneName(boneInfo.NameIdx);
			std::println("Bone {} is called {}", boneIndex, boneName.ToString());
		}
		catch (DriverException e) {
			std::println("DriverException when reading bone: {}", e.what());
			boneIndex++;
			continue;
		}

		boneIndex++;
	}
}

int main(void) {
	SDK::Init();

	auto localPawn =
		SDK::GWorld
		->OwningGameInstance
		->LocalPlayers[0]
		->PlayerController
		->AcknowledgedPawn;

	auto localChar = static_cast<SDK::ACharacter*>(localPawn);

	// Dump some bones
	DumpBones();

	Render.Init();

	bool renderESP = true;

	while (true) {
		Render.BeginFrame();

		{ /* Watermark (TODO: Make nicer) */
			ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.75f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
			ImGui::Begin("##Watermark", nullptr,
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav);
			ImGui::Text("Coffeeware v1");
			ImGui::End();
			// Draw a second one next to it showing FPS
			auto cursorX = ImGui::GetCursorPosX();
			ImGui::SetNextWindowPos(ImVec2(120, 10), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.75f);
			ImGui::Begin("##FPS", nullptr,
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav);
			ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
			ImGui::End();
			ImGui::PopStyleVar();
		}

		auto players = SDK::GWorld
			->GameState
			->PlayerArray;
		auto localPlayer = SDK::GWorld
			->OwningGameInstance
			->LocalPlayers[0];

		if (renderESP) {
			for (auto player : players) {
				if (!player->Pawn) continue;
				if (player->Pawn == localPlayer->PlayerController->AcknowledgedPawn) continue;

				auto loc = player->Pawn->RootComponent->RelativeLocation;
				if (auto screenPos = localPlayer->PlayerController->PlayerCameraManager->WorldToScreen(loc, 1920, 1080)) {
					auto distance = (localPlayer->PlayerController->AcknowledgedPawn->RootComponent->RelativeLocation - loc).Size();
					float alphaF = 0.75f - (distance / 40000.0f);
					alphaF = std::clamp(alphaF, 0.0f, 1.0f);
					int alpha = static_cast<int>(alphaF * 255.0f);

					/*ImGui::GetBackgroundDrawList()->AddLine(
						ImVec2(960, 1080),
						*screenPos,
						IM_COL32(255, 255, 255, alpha),
						2.0f
					);
					ImGui::GetBackgroundDrawList()->AddLine(
						ImVec2(960, 1080),
						*screenPos,
						IM_COL32(255, 255, 255, alpha),
						1.0f
					);*/

					SDK::ACharacter* character = static_cast<SDK::ACharacter*>(player->Pawn);
					SDK::FTransform c2w = character->Mesh->ComponentToWorld;

					// Disable bone update optimizations to get correct bone transforms
					//character->Mesh->RenderOptimization = false;
					//character->Mesh->VisibilityBasedAnimTickOption = SDK::EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
					//std::println("VisibilityBasedAnimTickOption: {}", static_cast<int>(character->Mesh->VisibilityBasedAnimTickOption));

					auto c2wMatrix = c2w.ToMatrix();
					auto bonesArr = character->Mesh->BoneArray;

					/*auto boneCount = bonesArr.Count;
					for (int i = 0; i < boneCount; i++) {
						try {
							SDK::FTransform trans = bonesArr[i];
							auto boneMat = trans.ToMatrix();
							auto boneWorldMat = boneMat * c2wMatrix;

							auto fvec = SDK::FVector{
								boneWorldMat.M[3][0],
								boneWorldMat.M[3][1],
								boneWorldMat.M[3][2],
							};

							if (auto boneScreenPos = localPlayer->PlayerController->PlayerCameraManager->WorldToScreen(fvec, 1920, 1080)) {
								ImGui::GetBackgroundDrawList()->AddCircleFilled(
									*boneScreenPos,
									3.0f,
									IM_COL32(255, 60, 60, alpha)
								);
							}
						}
						catch (DriverException e) {
							std::println("DriverException when reading bone: {}", e.what());
							continue;
						}
					}*/

					for (auto& pair : SkeletonPairs) {
						try {
							auto jointA = bonesArr[pair.first];
							auto jointB = bonesArr[pair.second];

							auto jointAMat = jointA.ToMatrix();
							auto jointBMat = jointB.ToMatrix();

							auto jointAWorldMat = jointAMat * c2wMatrix;
							auto jointBWorldMat = jointBMat * c2wMatrix;

							auto jointAFVec = SDK::FVector{
								jointAWorldMat.M[3][0],
								jointAWorldMat.M[3][1],
								jointAWorldMat.M[3][2],
							};

							auto jointBFVec = SDK::FVector{
								jointBWorldMat.M[3][0],
								jointBWorldMat.M[3][1],
								jointBWorldMat.M[3][2],
							};

							auto jointAScreenPos = localPlayer->PlayerController->PlayerCameraManager->WorldToScreen(jointAFVec, 1920, 1080);
							auto jointBScreenPos = localPlayer->PlayerController->PlayerCameraManager->WorldToScreen(jointBFVec, 1920, 1080);

							if (jointAScreenPos && jointBScreenPos) {
								ImGui::GetBackgroundDrawList()->AddLine(
									*jointAScreenPos,
									*jointBScreenPos,
									IM_COL32(60, 255, 60, alpha),
									2.0f
								);
							}
						}
						catch (DriverException e) {
							std::println("DriverException when reading bone: {}", e.what());
							continue;
						}
						catch (std::exception e) {
							std::println("Exception when reading bone: {}", e.what());
							continue;
						}
					}
				}
			}
		}

		if (Render.running) {
			ImGui::Begin("Player list");

			// Show table of players with Name | Ping | Location

			ImGui::Text("Player count: %d", players.Count);

			if (ImGui::BeginTable("PlayersTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Score");
				ImGui::TableSetupColumn("Ping");
				ImGui::TableSetupColumn("Location");
				ImGui::TableSetupColumn("Bone count");
				ImGui::TableHeadersRow();
				for (auto playerState : players) {
					auto pawn = playerState->Pawn;
					if (!pawn) continue;
					auto rootComp = pawn->RootComponent;
					auto loc = rootComp->RelativeLocation;

					SDK::ASolarPlayerState* solarPlayerState =
						static_cast<SDK::ASolarPlayerState*>(playerState);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (pawn == localPlayer->PlayerController->AcknowledgedPawn) {
						ImGui::Text("%s (You)", solarPlayerState->NickName.ToString().c_str());
					}
					else {
						ImGui::Text("%s", solarPlayerState->NickName.ToString().c_str());
					}
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%d", playerState->Score);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%d", static_cast<int>(playerState->Ping));
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("X: %.1f Y: %.1f Z: %.1f", loc.X, loc.Y, loc.Z);
					ImGui::TableSetColumnIndex(4);

					SDK::USkeletalMeshComponent* mesh = static_cast<SDK::ACharacter*>(pawn)->Mesh;
					if (mesh) {
						ImGui::Text("Bones: %d", mesh->BoneArray.Count);
					}
					else {
						ImGui::Text("NULL");
					}
				}
				ImGui::EndTable();
			}

			ImGui::End();
		}

		Render.EndFrame();
	}

#if 0	
	while (true) {
		try {
			SDK::Init();

			auto players = SDK::GWorld
				->GameState
				->PlayerArray;

			auto localPlayer = SDK::GWorld
				->OwningGameInstance
				->LocalPlayers[0];

			std::println("Player count: {}", players.Count);

			for (auto playerState : players) {
				auto pawn = playerState->Pawn;
				if (!pawn) continue;

				auto rootComp = pawn->RootComponent;
				auto loc = rootComp->RelativeLocation;

				if (pawn == localPlayer->PlayerController->AcknowledgedPawn) {
					std::print("Local player -> ");
				}

				SDK::ASolarPlayerState* solarPlayerState =
					static_cast<SDK::ASolarPlayerState*>(playerState);

				std::println("Player: {} | Score: {} | Id: {} | Ping: {} | Location: {}",
					solarPlayerState->NickName.ToString(),
					playerState->Score,
					playerState->PlayerId,
					static_cast<int>(playerState->Ping),
					loc.ToString()
				);
			}
		}
		catch (const DriverException& e) {
			std::cerr << "Driver error: " << e.what() << std::endl;
			std::cerr << "Status: " << DriverException::getStatusMessage(e.status()) << std::endl;
			return -1;
		}
		catch (const std::exception& e) {
			std::cerr << "Error: " << e.what() << std::endl;
			return -1;
		}

	}
	return 0;
#endif
}