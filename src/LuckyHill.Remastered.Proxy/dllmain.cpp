#define WIN32_LEAN_AND_MEAN

#include <string>
#include <windows.h>

HMODULE DxgiHandle = nullptr;
extern "C" uintptr_t mProcs[20] = {0};

void SetupFunctions()
{
    mProcs[0] = (uintptr_t)GetProcAddress(DxgiHandle, "ApplyCompatResolutionQuirking");
    mProcs[1] = (uintptr_t)GetProcAddress(DxgiHandle, "CompatString");
    mProcs[2] = (uintptr_t)GetProcAddress(DxgiHandle, "CompatValue");
    mProcs[3] = (uintptr_t)GetProcAddress(DxgiHandle, "CreateDXGIFactory");
    mProcs[4] = (uintptr_t)GetProcAddress(DxgiHandle, "CreateDXGIFactory1");
    mProcs[5] = (uintptr_t)GetProcAddress(DxgiHandle, "CreateDXGIFactory2");
    mProcs[6] = (uintptr_t)GetProcAddress(DxgiHandle, "DXGID3D10CreateDevice");
    mProcs[7] = (uintptr_t)GetProcAddress(DxgiHandle, "DXGID3D10CreateLayeredDevice");
    mProcs[8] = (uintptr_t)GetProcAddress(DxgiHandle, "DXGID3D10GetLayeredDeviceSize");
    mProcs[9] = (uintptr_t)GetProcAddress(DxgiHandle, "DXGID3D10RegisterLayers");
    mProcs[10] = (uintptr_t)GetProcAddress(DxgiHandle, "DXGIDeclareAdapterRemovalSupport");
    mProcs[11] = (uintptr_t)GetProcAddress(DxgiHandle, "DXGIDisableVBlankVirtualization");
    mProcs[12] = (uintptr_t)GetProcAddress(DxgiHandle, "DXGIDumpJournal");
    mProcs[13] = (uintptr_t)GetProcAddress(DxgiHandle, "DXGIGetDebugInterface1");
    mProcs[14] = (uintptr_t)GetProcAddress(DxgiHandle, "DXGIReportAdapterConfiguration");
    mProcs[15] = (uintptr_t)GetProcAddress(DxgiHandle, "PIXBeginCapture");
    mProcs[16] = (uintptr_t)GetProcAddress(DxgiHandle, "PIXEndCapture");
    mProcs[17] = (uintptr_t)GetProcAddress(DxgiHandle, "PIXGetCaptureState");
    mProcs[18] = (uintptr_t)GetProcAddress(DxgiHandle, "SetAppCompatStringPointer");
    mProcs[19] = (uintptr_t)GetProcAddress(DxgiHandle, "UpdateHMDEmulationStatus");
}

void LoadOriginalDll()
{
    wchar_t path[MAX_PATH];
    GetSystemDirectory(path, MAX_PATH);

    std::wstring dllPath = std::wstring(path) + L"\\dxgi.dll";

    DxgiHandle = LoadLibrary(dllPath.c_str());
    if (DxgiHandle == NULL)
    {
        ExitProcess(0);
    }
}

BOOL WINAPI DllMain(HMODULE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        LoadOriginalDll();
        SetupFunctions();

        LoadLibrary(L"mods\\LuckyHill.Remastered.dll");
        break;
    }
    case DLL_PROCESS_DETACH:
    {
        FreeLibrary(DxgiHandle);
        break;
    }
    }

    return TRUE;
}
