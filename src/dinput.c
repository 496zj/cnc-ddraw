#include <windows.h>
#include <dinput.h>
#include "hook.h"
#include "main.h"

typedef HRESULT (WINAPI *DIRECTINPUTCREATEAPROC)(HINSTANCE, DWORD, LPDIRECTINPUTA*, LPUNKNOWN);
typedef HRESULT (WINAPI *DICREATEDEVICEPROC)(IDirectInputA*, REFGUID, LPDIRECTINPUTDEVICEA *, LPUNKNOWN);
typedef HRESULT (WINAPI *DIDSETCOOPERATIVELEVELPROC)(IDirectInputDeviceA *, HWND, DWORD);
typedef HRESULT (WINAPI *DIDGETDEVICEDATAPROC)(IDirectInputDeviceA*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);

static DIRECTINPUTCREATEAPROC DInputCreateA;
static DICREATEDEVICEPROC DICreateDevice;
static DIDSETCOOPERATIVELEVELPROC DIDSetCooperativeLevel;
static DIDGETDEVICEDATAPROC DIDGetDeviceData;

static PROC HookFunc(PROC *orgFunc, PROC newFunc)
{
    PROC org = *orgFunc;
    DWORD oldProtect;

    if (VirtualProtect(orgFunc, sizeof(PROC), PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        *orgFunc = newFunc;
        VirtualProtect(orgFunc, sizeof(PROC), oldProtect, &oldProtect);
        return org;
    }

    return 0;
}

static HRESULT WINAPI fake_DIDSetCooperativeLevel(IDirectInputDeviceA *This, HWND hwnd, DWORD dwFlags)
{
    return DIDSetCooperativeLevel(This, hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
}

static HRESULT WINAPI fake_DIDGetDeviceData(IDirectInputDeviceA *This, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags)
{
    HRESULT result = DIDGetDeviceData(This, cbObjectData, rgdod, pdwInOut, dwFlags);

    if (SUCCEEDED(result) && ddraw && !ddraw->locked)
    {
        *pdwInOut = 0;
    }

    return result;
}

static HRESULT WINAPI fake_DICreateDevice(IDirectInputA *This, REFGUID rguid, LPDIRECTINPUTDEVICEA * lplpDIDevice, LPUNKNOWN pUnkOuter)
{
    HRESULT result = DICreateDevice(This, rguid, lplpDIDevice, pUnkOuter);

    if (SUCCEEDED(result) && !DIDSetCooperativeLevel)
    {
        DIDSetCooperativeLevel = 
            (DIDSETCOOPERATIVELEVELPROC)HookFunc(
                (PROC *)&(*lplpDIDevice)->lpVtbl->SetCooperativeLevel, (PROC)fake_DIDSetCooperativeLevel);

        DIDGetDeviceData =
            (DIDGETDEVICEDATAPROC)HookFunc(
                (PROC*)&(*lplpDIDevice)->lpVtbl->GetDeviceData, (PROC)fake_DIDGetDeviceData);
    }

    return result;
}

static HRESULT WINAPI fake_DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA* lplpDirectInput, LPUNKNOWN punkOuter)
{
    printf("??? DirectInputCreateA\n");

    DInputCreateA = 
        (DIRECTINPUTCREATEAPROC)GetProcAddress(GetModuleHandle("dinput.dll"), "DirectInputCreateA");

    if (!DInputCreateA)
        return DIERR_GENERIC;

    HRESULT result = DInputCreateA(hinst, dwVersion, lplpDirectInput, punkOuter);

    if (SUCCEEDED(result) && !DICreateDevice)
    {
        DICreateDevice =
            (DICREATEDEVICEPROC)HookFunc((PROC *)&(*lplpDirectInput)->lpVtbl->CreateDevice, (PROC)fake_DICreateDevice);
    }

    return result;
}

void DInput_Hook()
{
    Hook_PatchIAT(GetModuleHandle(NULL), "dinput.dll", "DirectInputCreateA", (PROC)fake_DirectInputCreateA);
}

void DInput_UnHook()
{
    Hook_PatchIAT(
        GetModuleHandle(NULL), 
        "dinput.dll", 
        "DirectInputCreateA", 
        (PROC)GetProcAddress(GetModuleHandle("dinput.dll"), "DirectInputCreateA"));
}
