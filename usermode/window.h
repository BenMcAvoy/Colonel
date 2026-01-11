#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d11.h>
#include <dxgi.h>

#include <string_view>
#include <functional>

// Topmost overlay window, transparent, ImGui enabled
// Click through when menu is closed
class Window {
public:
	Window(std::string_view title, int width, int height);
	~Window();

	void SetRenderFunc(std::function<void(Window&)> func) {
		renderFunc_ = func;
	}

	void Show();

private:
	void CreateRenderTarget();
	void CleanupRenderTarget();

	void InitD3D();
	void CleanupD3D();

	HWND hWnd;
	int width;
	int height;

	ID3D11Device* pD3DDevice = nullptr;
	ID3D11DeviceContext* pD3DDeviceContext = nullptr;
	IDXGISwapChain* pSwapChain = nullptr;
	ID3D11RenderTargetView* pMainRenderTargetView = nullptr;

	std::function<void(Window&)> renderFunc_;
	static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

