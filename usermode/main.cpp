#include "sdk/sdk.h"

#include "window.h"

#include <imgui.h>
#include <chrono>
#include <iostream>
#include <format>
#include <print>
#include <algorithm>

#include "render.h"

int main(void) {
	SDK::Init();

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
					float alphaF = 1.0f - (distance / 40000.0f);
					alphaF = std::clamp(alphaF, 0.0f, 1.0f);
					int alpha = static_cast<int>(alphaF * 255.0f);

					// Draw a line to it
					ImGui::GetBackgroundDrawList()->AddLine(
						ImVec2(960, 540),
						*screenPos,
						IM_COL32(255, 255, 255, alpha),
						2.0f
					);
					ImGui::GetBackgroundDrawList()->AddLine(
						ImVec2(960, 540),
						*screenPos,
						IM_COL32(255, 255, 255, alpha),
						1.0f
					);

					SDK::ASolarPlayerState* solarPlayerState =
						static_cast<SDK::ASolarPlayerState*>(player);

					auto textSize = ImGui::CalcTextSize(solarPlayerState->NickName.ToString().c_str());
					ImGui::GetBackgroundDrawList()->AddRectFilled(
						ImVec2(screenPos->x - textSize.x / 2 - 2, screenPos->y - textSize.y / 2 - 2),
						ImVec2(screenPos->x + textSize.x / 2 + 2, screenPos->y + textSize.y / 2 + 2),
						IM_COL32(0, 0, 0, alpha),
						5.0f
					);
					ImGui::GetBackgroundDrawList()->AddText(
						ImVec2(screenPos->x - textSize.x / 2, screenPos->y - textSize.y / 2),
						IM_COL32(255, 255, 255, alpha),
						solarPlayerState->NickName.ToString().c_str()
					);
				}
			}
		}

		if (Render.running) {
			ImGui::Begin("Player list");

			// Show table of players with Name | Ping | Location

			ImGui::Text("Player count: %d", players.Count);

			if (ImGui::BeginTable("PlayersTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Score");
				ImGui::TableSetupColumn("Ping");
				ImGui::TableSetupColumn("Location");
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