#include "pch.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

// ImGui
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

using Microsoft::WRL::ComPtr;

extern "C"
{
    // Used to enable the "Agility SDK" components
    __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
     HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    constexpr UINT g_frameCount = 2;

    HWND g_hwnd = nullptr;
    UINT g_width = 1280;
    UINT g_height = 720;

    ComPtr<ID3D12Device> g_device;
    ComPtr<ID3D12CommandQueue> g_commandQueue;
    ComPtr<IDXGISwapChain3> g_swapChain;
    ComPtr<ID3D12DescriptorHeap> g_rtvHeap;
    UINT g_rtvDescriptorSize = 0;

    ComPtr<ID3D12Resource> g_renderTargets[g_frameCount];
    ComPtr<ID3D12CommandAllocator> g_commandAllocators[g_frameCount];
    ComPtr<ID3D12GraphicsCommandList> g_commandList;

    ComPtr<ID3D12Fence> g_fence;
    UINT g_frameIndex = 0;
    UINT64 g_fenceValues[g_frameCount] = {};
    HANDLE g_fenceEvent = nullptr;

    ComPtr<ID3D12RootSignature> g_rootSignature;
    ComPtr<ID3D12PipelineState> g_pipelineState;

    ComPtr<ID3D12DescriptorHeap> g_imguiSrvHeap;

    float g_clearColor[4] = {0.08f, 0.1f, 0.14f, 1.0f};
    bool g_showDemo = false;
    bool g_imguiContextCreated = false;
    bool g_imguiWin32Initialized = false;
    bool g_imguiDx12Initialized = false;

    void CheckHr(HRESULT hr, const char* context) {
        if (FAILED(hr)) {
            char message[512] = {};
            sprintf_s(message, "%s failed with HRESULT 0x%08X", context, static_cast<unsigned int>(hr));
            throw std::runtime_error(message);
        }
    }

    std::wstring GetAssetPath(const wchar_t* relativePath) {
        wchar_t modulePath[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
            throw std::runtime_error("GetModuleFileNameW failed");

        return (std::filesystem::path(modulePath).parent_path() / relativePath).wstring();
    }

    void GetHardwareAdapter(IDXGIFactory4* factory, IDXGIAdapter1** adapter) {
        *adapter = nullptr;

        ComPtr<IDXGIAdapter1> selectedAdapter;
        ComPtr<IDXGIFactory6> factory6;
        if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6)))) {
            for (UINT adapterIndex = 0;
                 SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                     adapterIndex,
                     DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                     IID_PPV_ARGS(selectedAdapter.ReleaseAndGetAddressOf())));
                 ++adapterIndex) {
                DXGI_ADAPTER_DESC1 desc = {};
                CheckHr(selectedAdapter->GetDesc1(&desc), "IDXGIAdapter1::GetDesc1");

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;

                if (SUCCEEDED(D3D12CreateDevice(selectedAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) {
                    *adapter = selectedAdapter.Detach();
                    return;
                }
            }
        }

        for (UINT adapterIndex = 0;
             SUCCEEDED(factory->EnumAdapters1(adapterIndex, selectedAdapter.ReleaseAndGetAddressOf()));
             ++adapterIndex) {
            DXGI_ADAPTER_DESC1 desc = {};
            CheckHr(selectedAdapter->GetDesc1(&desc), "IDXGIAdapter1::GetDesc1");

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            if (SUCCEEDED(D3D12CreateDevice(selectedAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) {
                *adapter = selectedAdapter.Detach();
                return;
            }
        }

        ComPtr<IDXGIAdapter> warpAdapter;
        CheckHr(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)), "IDXGIFactory4::EnumWarpAdapter");
        CheckHr(warpAdapter.As(&selectedAdapter), "IDXGIAdapter::As<IDXGIAdapter1>");
        *adapter = selectedAdapter.Detach();
    }

    void AllocateImguiSrvDescriptor(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuDescHandle) {
        *outCpuDescHandle = g_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart();
        *outGpuDescHandle = g_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart();
    }

    void FreeImguiSrvDescriptor(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {
    }

    std::string LoadShaderSource(const std::wstring& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("Failed to open shader file");

        std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (source.size() >= 3 &&
            static_cast<unsigned char>(source[0]) == 0xEF &&
            static_cast<unsigned char>(source[1]) == 0xBB &&
            static_cast<unsigned char>(source[2]) == 0xBF) {
            source.erase(0, 3);
        }

        return source;
    }

    ComPtr<ID3DBlob> CompileShader(const std::wstring& path, const char* entryPoint, const char* target, UINT compileFlags) {
        const std::string source = LoadShaderSource(path);
        const std::string sourcePath = std::filesystem::path(path).string();

        ComPtr<ID3DBlob> shaderBlob;
        ComPtr<ID3DBlob> errorBlob;
        const HRESULT hr = D3DCompile(
            source.data(),
            source.size(),
            sourcePath.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint,
            target,
            compileFlags,
            0,
            &shaderBlob,
            &errorBlob);

        if (FAILED(hr)) {
            std::string message = "D3DCompile failed";
            if (errorBlob && errorBlob->GetBufferPointer()) {
                message += ": ";
                message.append(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
            }
            throw std::runtime_error(message);
        }

        return shaderBlob;
    }
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            default:
            break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}
void WaitForGpu() {
    if (!g_commandQueue || !g_fence || !g_fenceEvent)
        return;

    const UINT64 fenceToWaitFor = g_fenceValues[g_frameIndex];
    CheckHr(g_commandQueue->Signal(g_fence.Get(), fenceToWaitFor), "ID3D12CommandQueue::Signal");
    CheckHr(g_fence->SetEventOnCompletion(fenceToWaitFor, g_fenceEvent), "ID3D12Fence::SetEventOnCompletion");
    WaitForSingleObject(g_fenceEvent, INFINITE);
    g_fenceValues[g_frameIndex] = fenceToWaitFor + 1;
}

void MoveToNextFrame() {
    const UINT64 currentFenceValue = g_fenceValues[g_frameIndex];
    CheckHr(g_commandQueue->Signal(g_fence.Get(), currentFenceValue), "ID3D12CommandQueue::Signal");

    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    if (g_fence->GetCompletedValue() < g_fenceValues[g_frameIndex]) {
        CheckHr(g_fence->SetEventOnCompletion(g_fenceValues[g_frameIndex], g_fenceEvent), "ID3D12Fence::SetEventOnCompletion");
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }

    g_fenceValues[g_frameIndex] = currentFenceValue + 1;
}

void InitWindow(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ImGui";

    if (!RegisterClassExW(&wc))
        throw std::runtime_error("RegisterClassExW failed");

    RECT rc = {0, 0, static_cast<LONG>(g_width), static_cast<LONG>(g_height)};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, 0);

    g_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"DX12 + Imgui",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
        );

    if (!g_hwnd)
        throw std::runtime_error("CreateWindow failed");

    ShowWindow(g_hwnd, nCmdShow);
}

void CreatePipeline()
{
    const std::wstring vertexShaderPath = GetAssetPath(L"Shaders\\TriangleVS.hlsl");
    const std::wstring pixelShaderPath = GetAssetPath(L"Shaders\\TrianglePS.hlsl");

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vertexShaderBlob = CompileShader(vertexShaderPath, "main", "vs_5_0", compileFlags);
    ComPtr<ID3DBlob> pixelShaderBlob = CompileShader(pixelShaderPath, "main", "ps_5_0", compileFlags);

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> rootSignatureErrorBlob;

    CheckHr(D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &rootSignatureBlob,
        &rootSignatureErrorBlob), "D3D12SerializeRootSignature");

    CheckHr(g_device->CreateRootSignature(
        0,
        rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&g_rootSignature)), "ID3D12Device::CreateRootSignature");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {nullptr, 0};
    psoDesc.pRootSignature = g_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = false;
    psoDesc.DepthStencilState.StencilEnable = false;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    CheckHr(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pipelineState)), "ID3D12Device::CreateGraphicsPipelineState");
}

void InitImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    g_imguiContextCreated = true;
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(g_hwnd))
        throw std::runtime_error("ImGui_ImplWin32_Init failed");
    g_imguiWin32Initialized = true;

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = g_device.Get();
    initInfo.CommandQueue = g_commandQueue.Get();
    initInfo.NumFramesInFlight = g_frameCount;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = g_imguiSrvHeap.Get();
    initInfo.SrvDescriptorAllocFn = AllocateImguiSrvDescriptor;
    initInfo.SrvDescriptorFreeFn = FreeImguiSrvDescriptor;

    if (!ImGui_ImplDX12_Init(&initInfo))
        throw std::runtime_error("ImGui_ImplDX12_Init failed");
    g_imguiDx12Initialized = true;
}

void ShutdownImGui() {
    if (g_imguiDx12Initialized) {
        ImGui_ImplDX12_Shutdown();
        g_imguiDx12Initialized = false;
    }

    if (g_imguiWin32Initialized) {
        ImGui_ImplWin32_Shutdown();
        g_imguiWin32Initialized = false;
    }

    if (g_imguiContextCreated && ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
        g_imguiContextCreated = false;
    }
}

void InitD3D() {
    UINT factoryFlags = 0;
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    CheckHr(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");

    ComPtr<IDXGIAdapter1> adapter;
    GetHardwareAdapter(factory.Get(), adapter.GetAddressOf());
    CheckHr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)), "D3D12CreateDevice");

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    CheckHr(g_device->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&g_commandQueue)), "ID3D12Device::CreateCommandQueue");

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = g_frameCount;
    swapChainDesc.Width = g_width;
    swapChainDesc.Height = g_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    CheckHr(factory->CreateSwapChainForHwnd(
        g_commandQueue.Get(),
        g_hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ), "IDXGIFactory4::CreateSwapChainForHwnd");

    CheckHr(factory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER), "IDXGIFactory4::MakeWindowAssociation");
    CheckHr(swapChain.As(&g_swapChain), "IDXGISwapChain1::As<IDXGISwapChain3>");
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    //RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = g_frameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    CheckHr(g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap)), "ID3D12Device::CreateDescriptorHeap(RTV)");
    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    //Create frame resources
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < g_frameCount; ++i)
    {
        CheckHr(g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i])), "IDXGISwapChain3::GetBuffer");
        g_device->CreateRenderTargetView(g_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += g_rtvDescriptorSize;

        CheckHr(g_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&g_commandAllocators[i])), "ID3D12Device::CreateCommandAllocator");
    }

    //ImGui SRV heap(shader-visible)
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    CheckHr(g_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_imguiSrvHeap)), "ID3D12Device::CreateDescriptorHeap(SRV)");

    CreatePipeline();

    CheckHr(g_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_commandAllocators[g_frameIndex].Get(),
        g_pipelineState.Get(),
        IID_PPV_ARGS(&g_commandList)), "ID3D12Device::CreateCommandList");

    CheckHr(g_commandList->Close(), "ID3D12GraphicsCommandList::Close");

    CheckHr(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)), "ID3D12Device::CreateFence");
    g_fenceValues[g_frameIndex] = 1;
    g_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_fenceEvent)
        CheckHr(HRESULT_FROM_WIN32(GetLastError()), "CreateEventW");

    InitImGui();
}

void PopulateCommandList() {
    CheckHr(g_commandAllocators[g_frameIndex]->Reset(), "ID3D12CommandAllocator::Reset");
    CheckHr(g_commandList->Reset(g_commandAllocators[g_frameIndex].Get(), g_pipelineState.Get()), "ID3D12GraphicsCommandList::Reset");

    D3D12_VIEWPORT viewport = {0.0f, 0.0f, (float)g_width, (float)g_height, 0.0f, 1.0f};
    D3D12_RECT scissorRect = {0, 0, (LONG)g_width, (LONG)g_height};

    g_commandList->SetGraphicsRootSignature(g_rootSignature.Get());
    g_commandList->RSSetViewports(1, &viewport);
    g_commandList->RSSetScissorRects(1, &scissorRect);

    //Transition to render target
    D3D12_RESOURCE_BARRIER toRT = {};
    toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRT.Transition.pResource = g_renderTargets[g_frameIndex].Get();
    toRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_commandList->ResourceBarrier(1, &toRT);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(g_frameIndex) * g_rtvDescriptorSize;

    g_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    g_commandList->ClearRenderTargetView(rtvHandle, g_clearColor, 0, nullptr);

    g_commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_commandList->DrawInstanced(3, 1, 0, 0);

    //ImGui
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Test");
    ImGui::Text("Hello from Test");
    ImGui::ColorEdit4("Clear Color", g_clearColor);
    ImGui::Checkbox("Show Demo Window", &g_showDemo);
    ImGui::Text("Back Buffer Index: %u", g_frameIndex);
    ImGui::End();

    if (g_showDemo)
        ImGui::ShowDemoWindow(&g_showDemo);

    ImGui::Render();

    ID3D12DescriptorHeap* heaps[] = {g_imguiSrvHeap.Get()};
    g_commandList->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_commandList.Get());

    D3D12_RESOURCE_BARRIER toPresent = toRT;
    toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_commandList->ResourceBarrier(1, &toPresent);

    CheckHr(g_commandList->Close(), "ID3D12GraphicsCommandList::Close");
}

void Render() {
    PopulateCommandList();

    ID3D12CommandList* list[] = {g_commandList.Get()};
    g_commandQueue->ExecuteCommandLists(1, list);

    CheckHr(g_swapChain->Present(1, 0), "IDXGISwapChain3::Present");
    MoveToNextFrame();
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int nCmdShow) {
#ifdef __MINGW32__
    if (FAILED(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED)))
        return 1;
#else
    if (FAILED(RoInitialize(RO_INIT_MULTITHREADED)))
        return 1;
#endif

    int result = 0;

    try {
        InitWindow(hInstance, nCmdShow);
        InitD3D();

        MSG msg = {};
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else {
                Render();
            }
        }

        WaitForGpu();
        result = (int)msg.wParam;
    }
    catch (const std::exception& ex) {
        const std::string errorMessage =
            std::string("Failed to initialize or run the D3D12 + ImGui sample.\n") + ex.what();
        MessageBoxA(g_hwnd, errorMessage.c_str(), "Error", MB_OK | MB_ICONERROR);
        result = 1;
    }

    try {WaitForGpu();} catch (...) {}

    ShutdownImGui();

    if (g_fenceEvent) {
        CloseHandle(g_fenceEvent);
        g_fenceEvent = nullptr;
    }

#ifdef __MINGW32__
    CoUninitialize();
#else
    RoUninitialize();
#endif

    return result;

}
