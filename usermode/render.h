#pragma once

#include <memory>
#include <vector>
#include <string>

#include <d3d11.h>

#include <imgui.h>
#include <imgui_freetype.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

struct RenderData {
	HWND window = nullptr;
	WNDCLASSEX wndclass = {};
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* deviceCtx = nullptr;
	ID3D11RenderTargetView* renderTarget = nullptr;
	IDXGISwapChain* swapchain = nullptr;
};

class Render_t {
public:
	Render_t() = default;
	~Render_t();

	bool running = false;

	void Init();

	void BeginFrame();
	void EndFrame();

	RenderData renderData{};

private:
	bool InitDevice();
	bool InitWindow();
	bool InitImGui();

	void DeinitDevice();
	void DeinitWindow();
	void DeinitImGui();
};

static inline Render_t Render;
