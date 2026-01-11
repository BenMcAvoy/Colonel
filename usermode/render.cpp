
#define IMGUI_DEFINE_MATH_OPERATORS
#include "render.h"

#include <dwmapi.h>
#include <cstdio>
#include <chrono>
#include <thread>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_SYSKEYDOWN:
        if (wParam == VK_F4) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

Render_t::~Render_t() {
    DeinitImGui();
    DeinitWindow();
    DeinitDevice();
}

bool Render_t::InitWindow() {
    renderData.wndclass.cbSize = sizeof(renderData.wndclass);
    renderData.wndclass.style = CS_CLASSDC;
    renderData.wndclass.lpszClassName = L"T4";
    renderData.wndclass.hInstance = GetModuleHandleA(0);
    renderData.wndclass.lpfnWndProc = WndProc;

    RegisterClassExW(&renderData.wndclass);

    renderData.window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        L"T4",
        L"T4",
        WS_POPUP,
        0,
        0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        0,
        0,
        renderData.wndclass.hInstance,
        0
    );

    if (!renderData.window)
        return false;

    SetLayeredWindowAttributes(renderData.window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

    RECT client_area{};
    RECT window_area{};

    GetClientRect(renderData.window, &client_area);
    GetWindowRect(renderData.window, &window_area);

    POINT diff{};
    ClientToScreen(renderData.window, &diff);

    MARGINS margins {
        window_area.left + (diff.x - window_area.left),
        window_area.top + (diff.y - window_area.top),
        window_area.right,
        window_area.bottom,
    };

    DwmExtendFrameIntoClientArea(renderData.window, &margins);

    ShowWindow(renderData.window, SW_SHOW);
    UpdateWindow(renderData.window);

    return true;
}

bool Render_t::InitDevice() {
    DXGI_SWAP_CHAIN_DESC scd{};

    scd.BufferCount = 1;

    scd.BufferDesc.Width = 0;
    scd.BufferDesc.Height = 0;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    scd.OutputWindow = renderData.window;

    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scd.Windowed = 1;

    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    scd.SampleDesc.Count = 2;
    scd.SampleDesc.Quality = 0;

    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL featureLevels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        featureLevels,
        2,
        D3D11_SDK_VERSION,
        &scd,
        &renderData.swapchain,
        &renderData.device,
        &featureLevel,
        &renderData.deviceCtx
    );

    if (result == DXGI_ERROR_UNSUPPORTED) {
        result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            featureLevels,
            2,
            D3D11_SDK_VERSION,
            &scd,
            &renderData.swapchain,
            &renderData.device,
            &featureLevel,
            &renderData.deviceCtx
        );
    }

    if (result != S_OK)
        MessageBoxA(nullptr, "This software can not run on your computer.", "Critical Problem", MB_ICONERROR | MB_OK);

    ID3D11Texture2D* backbuffer{ nullptr };
    renderData.swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));

    if (backbuffer) {
        renderData.device->CreateRenderTargetView(backbuffer, nullptr, &renderData.renderTarget);
        backbuffer->Release();

        return true;
    }

    return false;
}

bool Render_t::InitImGui() {
    using namespace ImGui;
    CreateContext();
    StyleColorsDark();

	float scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
	style.ScaleAllSizes(scale);

    // C:\Windows\Fonts\verdana.ttf
	io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdana.ttf", 14.0f * scale);
    io.Fonts->Build();

    if (!ImGui_ImplWin32_Init(renderData.window))
        return false;
    if (!renderData.device || !renderData.deviceCtx)
        return false;
    if (!ImGui_ImplDX11_Init(renderData.device, renderData.deviceCtx))
        return false;

    return true;
}

void Render_t::DeinitDevice() {
	if (renderData.renderTarget) renderData.renderTarget->Release();
	if (renderData.swapchain) renderData.swapchain->Release();
	if (renderData.deviceCtx) renderData.deviceCtx->Release();
	if (renderData.device) renderData.device->Release();
}

void Render_t::DeinitWindow() {
    DestroyWindow(renderData.window);
    UnregisterClassW(L"T4", renderData.wndclass.hInstance);
}

void Render_t::DeinitImGui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void Render_t::Init() {
    InitWindow();
    InitDevice();
	InitImGui();
}

void Render_t::BeginFrame() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (GetAsyncKeyState(VK_HOME) & 1) {
        running = !running;

        if (running) {
            SetWindowLong(renderData.window, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);
        } else {
            SetWindowLong(renderData.window, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);
        }
    }
}

void Render_t::EndFrame() {
    ImGui::Render();

    float clearColour[4]{ 0, 0, 0, 0 };
    renderData.deviceCtx->OMSetRenderTargets(1, &renderData.renderTarget, nullptr);
    renderData.deviceCtx->ClearRenderTargetView(renderData.renderTarget, clearColour);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    renderData.swapchain->Present(0, 0);
}
