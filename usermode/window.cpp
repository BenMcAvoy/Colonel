#include "window.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Ensure D3D11/ DXGI libs are linked when building from this translation unit
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

Window::Window(std::string_view title, int width, int height)
	: hWnd(nullptr), width(width), height(height), renderFunc_(nullptr) {
	WNDCLASSEXA wc = {
		sizeof(WNDCLASSEXA),
		CS_CLASSDC,
		Window::WndProc,
		0,
		0,
		GetModuleHandleA(NULL),
		NULL,
		NULL,
		NULL,
		NULL,
		title.data(),
		NULL
	};

	RegisterClassExA(&wc);
	hWnd = CreateWindowExA(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
		wc.lpszClassName,
		title.data(),
		WS_POPUP,
		100,
		100,
		width,
		height,
		NULL,
		NULL,
		wc.hInstance,
		NULL
	);

	// Make the window transparent (black color key)
	SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
	SetWindowLongPtrA(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	InitD3D();
}

LRESULT WINAPI Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Window* window = reinterpret_cast<Window*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (window && window->pSwapChain != nullptr && wParam != SIZE_MINIMIZED)
		{
			window->CleanupRenderTarget();
			// Resize buffers to new window dimensions
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);
			window->pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
			window->CreateRenderTarget();
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProcA(hWnd, msg, wParam, lParam);
}

Window::~Window() {
	CleanupD3D();
	DestroyWindow(hWnd);
}

void Window::Show() {
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	// Main loop
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT) {
		if (PeekMessageA(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
			continue;
		}
		// Start the ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		// Call the user-defined render function
		if (renderFunc_) {
			renderFunc_(*this);
		}
		// Rendering
		ImGui::Render();
		const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		pD3DDeviceContext->OMSetRenderTargets(1, &pMainRenderTargetView, NULL);
		pD3DDeviceContext->ClearRenderTargetView(pMainRenderTargetView, clear_color);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		pSwapChain->Present(0, 0); // Present with vsync
	}
}

void Window::CreateRenderTarget() {
	ID3D11Texture2D* pBackBuffer;
	pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	pD3DDevice->CreateRenderTargetView(pBackBuffer, NULL, &pMainRenderTargetView);
	pBackBuffer->Release();
}

void Window::CleanupRenderTarget() {
	if (pMainRenderTargetView) {
		pMainRenderTargetView->Release();
		pMainRenderTargetView = nullptr;
	}
}

void Window::InitD3D() {
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	UINT createDeviceFlags = 0;
	D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		createDeviceFlags,
		NULL,
		0,
		D3D11_SDK_VERSION,
		&sd,
		&pSwapChain,
		&pD3DDevice,
		NULL,
		&pD3DDeviceContext
	);
	CreateRenderTarget();
	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	// Setup ImGui style
	ImGui::StyleColorsDark();
	// Setup ImGui Win32 + DX11 bindings
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(pD3DDevice, pD3DDeviceContext);
}

void Window::CleanupD3D() {
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	CleanupRenderTarget();
	if (pSwapChain) {
		pSwapChain->Release();
		pSwapChain = nullptr;
	}
	if (pD3DDeviceContext) {
		pD3DDeviceContext->Release();
		pD3DDeviceContext = nullptr;
	}
	if (pD3DDevice) {
		pD3DDevice->Release();
		pD3DDevice = nullptr;
	}
}
