#include <SDK.hpp>
#include <MinHook.h>

#include <windows.h>

#undef DrawText

constexpr SDK::FLinearColor GallowsColor = { 0.08, 0.25, 0.15, 1 };
constexpr SDK::FLinearColor PinpadColor = { 0.05, 0.18, 0.10, 1 };

typedef void(__thiscall* ProcessEvent)(SDK::UObject*, SDK::UFunction*, void*);
typedef void(__thiscall* PostRender)(SDK::AHUD*);

ProcessEvent s_oProcessEvent = nullptr;
PostRender s_oHudPostRender = nullptr;

std::vector<SDK::TWeakObjectPtr<SDK::APinpad_3Digits_BP_C>> s_Pinpads;
std::vector<SDK::TWeakObjectPtr<SDK::AGallowsPuzzle_BP_C>> s_Gallows;

SDK::UWorld* s_GWorld = nullptr;
bool s_HudReady = false;

void DrawBoxAroundComponent(SDK::AHUD* hud, const SDK::UStaticMeshComponent* component, const SDK::FLinearColor borderColor, const float borderThickness)
{
    if (!component
        || !hud || !hud->Canvas)
    {
        return;
    }

    constexpr int NumCorners = 4;

    SDK::FVector minBounds, maxBounds;
    component->GetLocalBounds(&minBounds, &maxBounds);

    const auto worldTransform = component->K2_GetComponentToWorld();
    const SDK::FVector corners[NumCorners]
    {
        worldTransform.TransformPosition(minBounds), // Min, Min, Min
        worldTransform.TransformPosition(SDK::FVector(maxBounds.X, minBounds.Y, minBounds.Z)), // Max, Min, Min
        worldTransform.TransformPosition(SDK::FVector(maxBounds.X, maxBounds.Y, minBounds.Z)), // Max, Max, Min
        worldTransform.TransformPosition(SDK::FVector(minBounds.X, maxBounds.Y, minBounds.Z)), // Min, Max, Min

        // worldTransform.TransformPosition(SDK::FVector(minBounds.X, minBounds.Y, maxBounds.Z)), // Min, Min, Max
        // worldTransform.TransformPosition(SDK::FVector(maxBounds.X, minBounds.Y, maxBounds.Z)), // Max, Min, Max
        // worldTransform.TransformPosition(maxBounds), // Max, Max, Max
        // worldTransform.TransformPosition(SDK::FVector(minBounds.X, maxBounds.Y, maxBounds.Z)), // Min, Max, Max
    };

    SDK::FVector2D projectedCorners[NumCorners] { };
    auto anyCornerOnScreen = false;

    for (int i = 0; i < NumCorners; ++i)
    {
        const auto projection = hud->Project(corners[i], false);
        projectedCorners[i] = {projection.X, projection.Y};

        const auto xOnScreen = projection.X > 0 && projection.X < hud->Canvas->ClipX;
        const auto yOnScreen = projection.Y > 0 && projection.Y < hud->Canvas->ClipY;
        const auto zOnScreen = projection.Z > 0 && projection.Z < 1;

        anyCornerOnScreen |= xOnScreen && yOnScreen && zOnScreen;
    }

    // Prevent clipping
    if (!anyCornerOnScreen)
    {
        return;
    }

    // Vertical lines
    // hud->DrawLine(projectedCorners[0].X, projectedCorners[0].Y, projectedCorners[4].X, projectedCorners[4].Y, borderColor, borderThickness);
    // hud->DrawLine(projectedCorners[1].X, projectedCorners[1].Y, projectedCorners[5].X, projectedCorners[5].Y, borderColor, borderThickness);
    // hud->DrawLine(projectedCorners[2].X, projectedCorners[2].Y, projectedCorners[6].X, projectedCorners[6].Y, borderColor, borderThickness);
    // hud->DrawLine(projectedCorners[3].X, projectedCorners[3].Y, projectedCorners[7].X, projectedCorners[7].Y, borderColor, borderThickness);

    // Top face
    // hud->DrawLine(projectedCorners[4].X, projectedCorners[4].Y, projectedCorners[5].X, projectedCorners[5].Y, borderColor, borderThickness);
    // hud->DrawLine(projectedCorners[5].X, projectedCorners[5].Y, projectedCorners[6].X, projectedCorners[6].Y, borderColor, borderThickness);
    // hud->DrawLine(projectedCorners[6].X, projectedCorners[6].Y, projectedCorners[7].X, projectedCorners[7].Y, borderColor, borderThickness);
    // hud->DrawLine(projectedCorners[7].X, projectedCorners[7].Y, projectedCorners[4].X, projectedCorners[4].Y, borderColor, borderThickness);

    // Bottom face
    hud->DrawLine(projectedCorners[0].X, projectedCorners[0].Y, projectedCorners[1].X, projectedCorners[1].Y, borderColor, borderThickness);
    hud->DrawLine(projectedCorners[1].X, projectedCorners[1].Y, projectedCorners[2].X, projectedCorners[2].Y, borderColor, borderThickness);
    hud->DrawLine(projectedCorners[2].X, projectedCorners[2].Y, projectedCorners[3].X, projectedCorners[3].Y, borderColor, borderThickness);
    hud->DrawLine(projectedCorners[3].X, projectedCorners[3].Y, projectedCorners[0].X, projectedCorners[0].Y, borderColor, borderThickness);
}

SDK::UStaticMeshComponent* GetPinpadLight(const int num, const SDK::APinpad_3Digits_BP_C* pinpad)
{
    switch (num)
    {
    case 0: return pinpad->PinPadButtonLight_01_A;
    case 1: return pinpad->PinPadButtonLight_02_A;
    case 2: return pinpad->PinPadButtonLight_03_A;
    }

    return nullptr;
}

void HighlightPinpad(const SDK::APinpad_3Digits_BP_C* pinpad, SDK::AHUD* hud)
{
    for (auto i = 0; i < pinpad->RandomizedRequiredCode.Num(); ++i)
    {
        const auto code = pinpad->RandomizedRequiredCode[i];
        const auto light = GetPinpadLight(i, pinpad);

        const auto worldTransform = light->K2_GetComponentToWorld();
        const auto screenLocation = hud->Project(worldTransform.Translation, false);

        const auto fString = SDK::FString(std::to_wstring(code));

        float textWidth, textHeight;
        hud->GetTextSize(fString, &textWidth, &textHeight, nullptr, 5);

        const float textDrawX = screenLocation.X - textWidth / 2; // Center text horizontally
        const float textDrawY = screenLocation.Y - textHeight / 2; // Position text vertically

        hud->DrawText(fString, PinpadColor, textDrawX, textDrawY, nullptr, 5, false);
    }
}

void DrawGallows(SDK::AHUD* hud)
{
    if (s_Gallows.empty())
    {
        return;
    }

    if (!s_GWorld->Levels.IsValidIndex(4)
        || s_GWorld->Levels[4]->GetFullName() != "Level Lv_Prison_05_Logic.Lv_Prison_05_Logic.PersistentLevel")
    {
        s_Gallows.clear();
        return;
    }

    for (const auto& gallowPuzzleWeak : s_Gallows)
    {
        const auto gallowPuzzle = gallowPuzzleWeak.Get();
        if (!gallowPuzzle->IsPuzzleEnabled)
        {
            continue;
        }

        for (const auto gallowNoose : gallowPuzzle->Nooses)
        {
            if (gallowNoose && gallowNoose->CorrectNoose)
            {
                DrawBoxAroundComponent(hud, gallowNoose->Gallows_01_B, GallowsColor, 5);
            }
        }
    }
}

void DrawPinPad(SDK::AHUD* hud)
{
    if (s_Pinpads.empty())
    {
        return;
    }

    if (!s_GWorld->Levels.IsValidIndex(3)
        || s_GWorld->Levels[3]->GetFullName() != "Level Lv_HistoricalSociety_04_Logic.Lv_HistoricalSociety_04_Logic.PersistentLevel")
    {
        s_Pinpads.clear();
        return;
    }

    for (const auto& pinPadWeak : s_Pinpads)
    {
        const auto pinpad = pinPadWeak.Get();
        if (pinpad->SHFocusable->IsShown())
        {
            HighlightPinpad(pinpad, hud);
        }
    }
}

void HudPostRenderHook(SDK::AHUD* hud)
{
    DrawGallows(hud);
    DrawPinPad(hud);

    s_oHudPostRender(hud);
}

void TryHookHud(const SDK::ULocalPlayer* player)
{
    if (s_HudReady)
    {
        return;
    }

    if (!player
        || !player->PlayerController
        || !player->PlayerController->MyHUD)
    {
        return;
    }

    const auto hudVtable = player->PlayerController->MyHUD->VTable;

    DWORD hudProtect = 0;
    VirtualProtect(&hudVtable[SDK::Offsets::HudPostRenderIdx], sizeof(void*), PAGE_EXECUTE_READWRITE, &hudProtect);

    s_oHudPostRender = reinterpret_cast<decltype(s_oHudPostRender)>(hudVtable[SDK::Offsets::HudPostRenderIdx]);
    hudVtable[SDK::Offsets::HudPostRenderIdx] = &HudPostRenderHook;

    VirtualProtect(&hudVtable[SDK::Offsets::HudPostRenderIdx], sizeof(void*), hudProtect, NULL);

    s_HudReady = true;
}

void HkProcessEvent(SDK::UObject* self, SDK::UFunction* function, void* parms)
{
    if (function->GetName() != "ReceiveBeginPlay")
    {
        s_oProcessEvent(self, function, parms);

        return;
    }

    const auto name = function->GetFullName();
    if (name == "Function GallowsPuzzle_BP.GallowsPuzzle_BP_C.ReceiveBeginPlay")
    {
        if (!self->IsDefaultObject())
        {
            const auto gallowsPuzzle = static_cast<SDK::AGallowsPuzzle_BP_C*>(self);
            s_Gallows.push_back(gallowsPuzzle);
        }
    }
    else if (name == "Function Pinpad_3Digits_BP.Pinpad_3Digits_BP_C.ReceiveBeginPlay")
    {
        if (!self->IsDefaultObject())
        {
            const auto pinpad = static_cast<SDK::APinpad_3Digits_BP_C*>(self);
            s_Pinpads.push_back(pinpad);
        }
    }
    else if (name == "Function SHCharacterPlay_BP.SHCharacterPlay_BP_C.ReceiveBeginPlay")
    {
        s_GWorld = SDK::UWorld::GetWorld();

        if (s_GWorld->OwningGameInstance
            && s_GWorld->OwningGameInstance->LocalPlayers.IsValidIndex(0))
        {
            TryHookHud(s_GWorld->OwningGameInstance->LocalPlayers[0]);
        }
    }

    s_oProcessEvent(self, function, parms);
}

DWORD MainThread(HMODULE _)
{
    MH_Initialize();

    while (!SDK::UObject::GObjects->Objects)
    {
        Sleep(100);
    }

    Sleep(1000);

    const auto vtable = SDK::UObject::GObjects->GetByIndex(0)->VTable;
    const auto processEvent = vtable[SDK::Offsets::ProcessEventIdx];

    MH_CreateHook(reinterpret_cast<LPVOID>(processEvent), reinterpret_cast<void*>(HkProcessEvent), reinterpret_cast<PVOID*>(&s_oProcessEvent));
    MH_EnableHook(reinterpret_cast<LPVOID>(processEvent));

    // Keep alive indefinitely
    while (true);

    return 0;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
        break;
    }

    return TRUE;
}
