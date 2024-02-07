/**
* Copyright (C) 2024 mercury501
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*/

#include "External\injector\include\injector\injector.hpp"
#include "External\injector\include\injector\utility.hpp"
#include "External\Hooking.Patterns\Hooking.Patterns.h"
#include "OptionsMenuTweaks.h"

bool DrawOptionsHookEnabled = false;
// Master volume
D3DMATRIX WorldMatrix =
{
  1.f, 0.f, 0.f, 0.f,
  0.f, 1.f, 0.f, 0.f,
  0.f, 0.f, 1.f, 0.f,
  0.f, 0.f, 0.f, 1.f
};

BYTE* ChangedOptionsCheckReturn = nullptr;
BYTE* DiscardOptionsBackingOutReturn = nullptr;
BYTE* DiscardOptionsNoBackingOutReturn = nullptr;
BYTE* ChangeMasterVolumeReturn = nullptr;

BYTE* MoveRightArrowHitboxReturn = nullptr;
DWORD* RightArrowDefaultPointer = nullptr;
DWORD RightArrowDefault = 0;

static int SavedMasterVolumeLevel = 0;
static int CurrentMasterVolumeLevel = 0;

injector::hook_back<void(__cdecl*)(DWORD*)> orgDrawOptions;
injector::hook_back<void(__cdecl*)(int32_t, int32_t)> orgDrawArrowRight;
injector::hook_back<void(__cdecl*)(int32_t)> orgConfirmOptionsFun;
injector::hook_back<int32_t(__cdecl*)(int32_t, float, DWORD)> orgPlaySound;

LPDIRECT3DDEVICE8 DirectXInterface = nullptr;

MasterVolume MasterVolumeRef;
MasterVolumeSlider MasterVolumeSliderRef;
bool DiscardOptions = false;
int ChangeMasterVolume = 0;

// Control options
ButtonIcons ButtonIconsRef;

const float ControlOptionRedGreen = 0.497;
const float ControlOptionSelectedBlue = 0.1211f;
const float ControlOptionUnselectedBlue = 0.497f;
const float ControlOptionsLockedRedGreen = 0.7490f;
const float ControlOptionsLockedBlue = 0.7471f;

/*
    ps.1.4

    texld r0, t0
    sub r0, r0, c0
*/

DWORD subtractionPixelShaderAsm[] = {
    0xffff0104, 0x0009fffe, 0x58443344, 0x68532038,
    0x72656461, 0x73734120, 0x6c626d65, 0x56207265,
    0x69737265, 0x30206e6f, 0x0031392e, 0x0009fffe,
    0x454c4946, 0x555c3a43, 0x73726573, 0x6d6f745c,
    0x445c616d, 0x746b7365, 0x745c706f, 0x35747365,
    0x0073702e, 0x0002fffe, 0x454e494c, 0x00000003,
    0x00000042, 0x800f0000, 0xb0e40000, 0x0002fffe,
    0x454e494c, 0x00000005, 0x00000003, 0x800f0000,
    0x80e40000, 0xa0e40000, 0x0000ffff
};

int32_t PlaySound_Hook(int32_t SoundId, float volume, DWORD param3)
{
    if (GetSelectedOption() == 0x07)
    {
        return 0;
    }

    return orgPlaySound.fun(SoundId, volume, param3);
}

#pragma warning(disable : 4100)
void __cdecl DrawArrowRight_Hook(int32_t param1, int32_t param2)
{
    orgDrawArrowRight.fun(0xC5, param2);
}

void __cdecl DrawOptions_Hook(DWORD* pointer)
{
    orgDrawOptions.fun(pointer);

    if (EnableMasterVolume)
    {
        MasterVolumeSliderRef.DrawSlider(DirectXInterface, CurrentMasterVolumeLevel, CurrentMasterVolumeLevel != SavedMasterVolumeLevel);
    }

    if (true) //TODO setting
    {
    ButtonIconsRef.DrawIcons(DirectXInterface);
    }
}

void __cdecl ConfirmOptions_Hook(int32_t param)
{
    MasterVolumeRef.HandleConfirmOptions(true);

    orgConfirmOptionsFun.fun(param);
}

#pragma warning(disable : 4740)
__declspec(naked) void __stdcall SetRightArrowHitbox()
{
    RightArrowDefault = *RightArrowDefaultPointer;

    if (GetSelectedOption() == 0x07)
    {
        __asm
        {   // In the Master Volume option, move the arrow to the right
            mov eax, 0xA8
        }
    }
    else
    {
        __asm
        {   // In another option, use the default from the game
            mov eax, RightArrowDefault
        }
    }

    __asm
    {
        jmp MoveRightArrowHitboxReturn
    }
}

__declspec(naked) void __stdcall ChangeSpeakerConfigCheck()
{
    __asm
    {
        mov dl, byte ptr[CurrentMasterVolumeLevel]
        cmp dl, byte ptr[SavedMasterVolumeLevel]

        jmp ChangedOptionsCheckReturn;
    }
}

__declspec(naked) void __stdcall DiscardOptionsBackingOut()
{
    DiscardOptions = true;

    __asm
    {
        jmp DiscardOptionsBackingOutReturn;
    }
}

__declspec(naked) void __stdcall DiscardOptionsNoBackingOut()
{
    DiscardOptions = true;

    __asm
    {
        jmp DiscardOptionsNoBackingOutReturn;
    }
}

__declspec(naked) void __stdcall IncrementMasterVolume()
{
    ChangeMasterVolume = 1;

    __asm
    {
        jmp ChangeMasterVolumeReturn;
    }
}

__declspec(naked) void __stdcall DecrementMasterVolume()
{
    ChangeMasterVolume = -1;

    __asm
    {
        jmp ChangeMasterVolumeReturn;
    }
}

void EnableDrawOptionsHook()
{
    if (DrawOptionsHookEnabled)
    {
        return;
    }

    // Hook options drawing to draw at the same time
    orgDrawOptions.fun = injector::MakeCALL(GetDrawOptionsFunPointer(), DrawOptions_Hook, true).get();

    DrawOptionsHookEnabled = true;
}

void PatchMasterVolumeSlider()
{
    // Initialize pointers
    ChangedOptionsCheckReturn = GetCheckForChangedOptionsPointer() + 0x0C;

    BYTE* DiscardOptionsBOAddr = GetDiscardOptionBOPointer() + (GameVersion == SH2V_DC ? 0x01 : 0x00);
    BYTE* DiscardOptionsAddr = GetDiscardOptionPointer();

    DiscardOptionsBackingOutReturn = GetDiscardOptionBOPointer();
    DiscardOptionsNoBackingOutReturn = GetDiscardOptionPointer();

    ChangeMasterVolumeReturn = GetDecrementMasterVolumePointer() + (GameVersion == SH2V_DC ? 0x1C : 0x16);

    MoveRightArrowHitboxReturn = GetOptionsRightArrowHitboxPointer() + 0x05;
    RightArrowDefaultPointer = *(DWORD**)(GetOptionsRightArrowHitboxPointer() + 0x01);    

    BYTE* DecrementVolumeAddr = GetDecrementMasterVolumePointer() + (GameVersion == SH2V_DC ? -0x02 : 0x0);
    BYTE* IncrementVolumeAddr = GetIncrementMasterVolumePointer() + (GameVersion == SH2V_DC ? -0x07 : 0x0);

    BYTE* RenderRightArrowAddr = GetRenderOptionsRightArrowFunPointer() + (GameVersion == SH2V_DC ? 0x02 : 0x0);

    EnableDrawOptionsHook();

    // Hook right arrow drawing to move it to the right 
    orgDrawArrowRight.fun = injector::MakeCALL(RenderRightArrowAddr, DrawArrowRight_Hook, true).get();

    // Skip drawing the old option text 
    UpdateMemoryAddress((void*)GetSpkOptionTextOnePointer(), "\x90\x90\x90\x90\x90", 5);
    UpdateMemoryAddress((void*)GetSpkOptionTextTwoPointer(), "\x90\x90\x90\x90\x90", 5);

    // Inject our values in the game's check for changed settings
    WriteJMPtoMemory(GetCheckForChangedOptionsPointer(), ChangeSpeakerConfigCheck, 0x0C);

    // Set the DiscardOptions flag when restoring saved settings
    WriteJMPtoMemory(DiscardOptionsBOAddr - 0x06, DiscardOptionsBackingOut, 0x06);
    WriteJMPtoMemory(DiscardOptionsAddr - 0x06, DiscardOptionsNoBackingOut, 0x06);

    // Detour execution to change the hitbox position
    WriteJMPtoMemory(GetOptionsRightArrowHitboxPointer(), SetRightArrowHitbox, 0x05);

    // Set the ChangeMasterVolumeValue to update the value
    WriteJMPtoMemory(IncrementVolumeAddr, IncrementMasterVolume, 0x07);
    WriteJMPtoMemory(DecrementVolumeAddr, DecrementMasterVolume, 0x07);

    // hook the function that is called when confirming changed options
    orgConfirmOptionsFun.fun = injector::MakeCALL(GetConfirmOptionsOnePointer(), ConfirmOptions_Hook, true).get();
    injector::MakeCALL(GetConfirmOptionsTwoPointer(), ConfirmOptions_Hook, true).get();

    // Hook the function that plays sounds at the end of the options switch
    orgPlaySound.fun = injector::MakeCALL(GetPlaySoundFunPointer(), PlaySound_Hook, true).get();
}

void PatchControlOptionsMenu()
{
    //TODO
    BYTE* FunctionDrawControllerValues = (BYTE*)0x00467985; //TODO address

    EnableDrawOptionsHook();

    UpdateMemoryAddress(FunctionDrawControllerValues, "\x90\x90\x90\x90\x90", 0x05);
}

void MasterVolume::ChangeMasterVolumeValue(int delta)
{
    if (!IsInMainOptionsMenu() || delta == 0)
        return;

    if ((delta < 0 && CurrentMasterVolumeLevel > 0) || (delta > 0 && CurrentMasterVolumeLevel < 0x0F))
    {
        CurrentMasterVolumeLevel += delta;
        ConfigData.VolumeLevel = CurrentMasterVolumeLevel;
        SetNewVolume();

        // Play the ding sound when changing volume
        orgPlaySound.fun(0x2719, 1.0, 0);
    }

    ChangeMasterVolume = 0;
}

void MasterVolume::HandleConfirmOptions(bool ConfirmChange)
{
    if (!ConfirmChange)
    {
        ConfigData.VolumeLevel = SavedMasterVolumeLevel;
        CurrentMasterVolumeLevel = SavedMasterVolumeLevel;
    }

    SaveConfigData();
    SetNewVolume();
}

void MasterVolume::HandleMasterVolume(LPDIRECT3DDEVICE8 ProxyInterface)
{
    if (!EnableMasterVolume)
        return;

    DirectXInterface = ProxyInterface;

    if (DiscardOptions)
    {
        this->HandleConfirmOptions(false);
        DiscardOptions = false;
    }

    // If we just entered the main options menu
    if (IsInOptionsMenu())
    {
        this->ChangeMasterVolumeValue(ChangeMasterVolume);

        if (!this->EnteredOptionsMenu)
        {
            SavedMasterVolumeLevel = ConfigData.VolumeLevel;
            CurrentMasterVolumeLevel = SavedMasterVolumeLevel;

            this->EnteredOptionsMenu = true;
        }
        else
        {
            ConfigData.VolumeLevel = CurrentMasterVolumeLevel;
        }    
    }
    else
    {
        this->EnteredOptionsMenu = false;
    }
}

void MasterVolumeSlider::InitVertices()
{
    this->LastBufferHeight = BufferHeight;
    this->LastBufferWidth = BufferWidth;

    int32_t VerticalInternal = GetInternalVerticalRes();
    int32_t HorizontalInternal = GetInternalHorizontalRes();

    float spacing = (25.781 * (float)HorizontalInternal) / 1200.f;
    float xScaling = (float)HorizontalInternal / 1200.f;
    float yScaling = (float)VerticalInternal / 900.f;

    float UlteriorOffset = (BufferWidth - (float)HorizontalInternal) / 2;
    
    float xOffset = (645.703 * (float)HorizontalInternal) / 1200.f + UlteriorOffset;
    float yOffset = (593.4375 * (float)VerticalInternal) / 900.f - ((50.625f * (float)VerticalInternal) / 900.f);

    for (int i = 0; i < 0xF; i++)
    {
        this->CopyVertexBuffer(this->BezelVertices, this->FinalBezels[i].TopVertices, BEZEL_VERT_NUM);
        this->CopyVertexBuffer(this->BezelVertices, this->FinalBezels[i].BotVertices, BEZEL_VERT_NUM);

        this->CopyVertexBuffer(this->RectangleVertices, this->FinalRects[i].vertices, RECT_VERT_NUM);

        // Flip the top bezel
        this->RotateVertexBuffer(this->FinalBezels[i].TopVertices, BEZEL_VERT_NUM, D3DX_PI);
        
        // Scaling
        this->ScaleVertexBuffer(this->FinalBezels[i].TopVertices, BEZEL_VERT_NUM, xScaling, yScaling);
        this->ScaleVertexBuffer(this->FinalBezels[i].BotVertices, BEZEL_VERT_NUM, xScaling, yScaling);
        this->ScaleVertexBuffer(this->FinalRects[i].vertices, RECT_VERT_NUM, xScaling, yScaling);

        // Translating
        this->TranslateVertexBuffer(this->FinalRects[i].vertices, RECT_VERT_NUM, xOffset + ((float)i * spacing), yOffset);

        float DeltaX = this->FinalRects[i].vertices[0].coords.x - this->FinalBezels[i].BotVertices[3].coords.x;
        float DeltaY = this->FinalRects[i].vertices[0].coords.y - this->FinalBezels[i].BotVertices[3].coords.y;

        this->TranslateVertexBuffer(this->FinalBezels[i].TopVertices, BEZEL_VERT_NUM, DeltaX, DeltaY);
        this->TranslateVertexBuffer(this->FinalBezels[i].BotVertices, BEZEL_VERT_NUM, DeltaX, DeltaY);
    }
}

void MasterVolumeSlider::DrawSlider(LPDIRECT3DDEVICE8 ProxyInterface, int value, bool ValueChanged)
{
    if (!IsInMainOptionsMenu())
        return;

    if (LastBufferHeight != BufferHeight || LastBufferWidth != BufferWidth)
        this->InitVertices();

    const int selected = GetSelectedOption() == 0x07 ? 0 : 1;

    // Set up the graphics' color
    if (ValueChanged)
    {
        for (int i = 0; i < 0xF; i++)
        {
            this->SetVertexBufferColor(this->FinalBezels[i].TopVertices, BEZEL_VERT_NUM, this->LightGoldBezel[selected]);
            this->SetVertexBufferColor(this->FinalBezels[i].BotVertices, BEZEL_VERT_NUM, this->DarkGoldBezel[selected]);
        }

        // Set inner rectangle color, based on the current value
        for (int i = 0; i < 0xF; i++)
        {
            if (value <= i)
                this->SetVertexBufferColor(this->FinalRects[i].vertices, RECT_VERT_NUM, this->InactiveGoldSquare[selected]);
            else
                this->SetVertexBufferColor(this->FinalRects[i].vertices, RECT_VERT_NUM, this->ActiveGoldSquare[selected]);
        }
    }
    else
    {
        for (int i = 0; i < 0xF; i++)
        {
            this->SetVertexBufferColor(this->FinalBezels[i].TopVertices, BEZEL_VERT_NUM, this->LightGrayBezel[selected]);
            this->SetVertexBufferColor(this->FinalBezels[i].BotVertices, BEZEL_VERT_NUM, this->DarkGrayBezel[selected]);
        }

        // Set inner rectangle color, based on the current value
        for (int i = 0; i < 0xF; i++)
        {
            if (value <= i)
                this->SetVertexBufferColor(this->FinalRects[i].vertices, RECT_VERT_NUM, this->InactiveGraySquare[selected]);
            else
                this->SetVertexBufferColor(this->FinalRects[i].vertices, RECT_VERT_NUM, this->ActiveGraySquare[selected]);
        }
    }

    ProxyInterface->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

    ProxyInterface->SetRenderState(D3DRS_ALPHAREF, 1);
    ProxyInterface->SetRenderState(D3DRS_LIGHTING, 0);
    ProxyInterface->SetRenderState(D3DRS_SPECULARENABLE, 0);
    ProxyInterface->SetRenderState(D3DRS_ZENABLE, 1);
    ProxyInterface->SetRenderState(D3DRS_ZWRITEENABLE, 1);
    ProxyInterface->SetRenderState(D3DRS_ALPHATESTENABLE, 0);
    ProxyInterface->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);

    ProxyInterface->SetRenderState(D3DRS_FOGENABLE, FALSE);

    ProxyInterface->SetTextureStageState(0, D3DTSS_COLOROP, 1);
    ProxyInterface->SetTextureStageState(0, D3DTSS_ALPHAOP, 1);

    ProxyInterface->SetTextureStageState(1, D3DTSS_COLOROP, 1);
    ProxyInterface->SetTextureStageState(1, D3DTSS_ALPHAOP, 1);

    ProxyInterface->SetTexture(0, 0);
    
    ProxyInterface->SetTransform(D3DTS_WORLDMATRIX(0x56), &WorldMatrix);

    // Draw every active bezel
    for (int i = 0; i < value; i++)
    {
        ProxyInterface->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 4, this->FinalBezels[i].TopVertices, 20);
        ProxyInterface->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 4, this->FinalBezels[i].BotVertices, 20);
    }
    
    // Draw every inner rectangle
    for (int i = 0; i < 0xF; i++)
        ProxyInterface->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, this->FinalRects[i].vertices, 20);

    ProxyInterface->SetRenderState(D3DRS_ALPHAREF, 2);
    ProxyInterface->SetRenderState(D3DRS_FOGENABLE, 1);
}

void MasterVolumeSlider::TranslateVertexBuffer(ColorVertex* vertices, int count, float x, float y)
{
    D3DXMATRIX TranslateMatrix;
    D3DXMatrixTranslation(&TranslateMatrix, x, y, 0.f);

    this->ApplyVertexBufferTransformation(vertices, count, TranslateMatrix);
}

void MasterVolumeSlider::RotateVertexBuffer(ColorVertex* vertices, int count, float angle)
{
    D3DXMATRIX RotationMatrix;
    D3DXMatrixRotationZ(&RotationMatrix, angle);

    this->ApplyVertexBufferTransformation(vertices, count, RotationMatrix);
}

void MasterVolumeSlider::ScaleVertexBuffer(ColorVertex* vertices, int count, float x, float y)
{
    D3DXMATRIX ScalingMatrix;
    D3DXMatrixScaling(&ScalingMatrix, x, y, 1.f);

    this->ApplyVertexBufferTransformation(vertices, count, ScalingMatrix);
}

void MasterVolumeSlider::ApplyVertexBufferTransformation(ColorVertex* vertices, int count, D3DXMATRIX matrix)
{
    D3DXVECTOR3 temp;

    for (int i = 0; i < count; i++)
    {
        D3DXVec3TransformCoord(&temp, &vertices[i].coords, &matrix);

        vertices[i].coords = temp;
    }
}

void MasterVolumeSlider::SetVertexBufferColor(ColorVertex* vertices, int count, DWORD color)
{
    for (int i = 0; i < count; i++)
    {
        vertices[i].color = color;
    }
}

void MasterVolumeSlider::CopyVertexBuffer(ColorVertex* source, ColorVertex* destination, int count)
{
    for (int i = 0; i < count; i++)
    {
        destination[i] = { D3DXVECTOR3(source[i].coords.x, source[i].coords.y, source[i].coords.z), source[i].rhw, source[i].color };
    }
}

/*
Page : subpage menu
1-2 : 0     main options screen
2   : 1     game options screen
3   : 0     brightness adjust screen
7   : 0     advanced options screen
4   : 0     control options screen
*/

bool IsInMainOptionsMenu()
{
    return GetOptionsPage() == 0x02 && GetOptionsSubPage() == 0x00;
}

bool IsInOptionsMenu()
{
    return GetEventIndex() == EVENT_OPTIONS_FMV &&
        (GetOptionsPage() == 0x02 || GetOptionsPage() == 0x07 || GetOptionsPage() == 0x04) &&
        (GetOptionsSubPage() == 0x00 || GetOptionsSubPage() == 0x01);
}

bool IsInControlOptionsMenu()
{
    return IsInOptionsMenu() && GetOptionsPage() == 0x04;
}

void ButtonIcons::DrawIcons(LPDIRECT3DDEVICE8 ProxyInterface)
{
    if (!IsInControlOptionsMenu())
    {
        return;
    }

    if (LastBufferHeight != BufferHeight || LastBufferWidth != BufferWidth)
    {
        this->Init(ProxyInterface);
    }

    if (ButtonIconsTexture == NULL)
    {
        this->Init(ProxyInterface);
    }

    if (ButtonIconsTexture == NULL)
    {
        //TODO setting to false
        Logging::Log() << __FUNCTION__ << " ERROR: Couldn't load button icons texture.";
        return;
    }

    ProxyInterface->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);

    ProxyInterface->SetRenderState(D3DRS_ALPHATESTENABLE, 0);
    ProxyInterface->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);

    ProxyInterface->SetRenderState(D3DRS_FOGENABLE, FALSE);

    ProxyInterface->SetTextureStageState(0, D3DTSS_COLOROP, 1);
    ProxyInterface->SetTextureStageState(0, D3DTSS_ALPHAOP, 1);

    ProxyInterface->SetTextureStageState(1, D3DTSS_COLOROP, 1);
    ProxyInterface->SetTextureStageState(1, D3DTSS_ALPHAOP, 1);

    ProxyInterface->SetTransform(D3DTS_WORLDMATRIX(0x56), &WorldMatrix);

    ProxyInterface->SetTexture(0, ButtonIconsTexture);

    D3DXVECTOR4 UnselectedSubtractionFactor(ControlOptionRedGreen, ControlOptionRedGreen, ControlOptionUnselectedBlue, 0.f);
    D3DXVECTOR4 SelectedSubtractionFactor(ControlOptionRedGreen, ControlOptionRedGreen, ControlOptionSelectedBlue, 0.f);
    D3DXVECTOR4 LockedSubtractionFactor(ControlOptionsLockedRedGreen, ControlOptionsLockedRedGreen, ControlOptionsLockedBlue, 0.f);

    ProxyInterface->SetPixelShader(SubtractionPixelShader);

    ProxyInterface->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    ProxyInterface->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    ProxyInterface->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

    ProxyInterface->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

    for (int i = 0; i < this->quadsNum; i++)
    {
        switch (this->quads[i].state)
        {
        case OptionState::LOCKED:
            ProxyInterface->SetPixelShaderConstant(0, &LockedSubtractionFactor.x, 1);
            break;
        case OptionState::SELECTED:
            ProxyInterface->SetPixelShaderConstant(0, &SelectedSubtractionFactor.x, 1);
            break;
        default:
        case OptionState::STANDARD:
            ProxyInterface->SetPixelShaderConstant(0, &UnselectedSubtractionFactor.x, 1);
            break;
        }

        ProxyInterface->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, this->quads[i].vertices, sizeof(TexturedVertex));
    }

    ProxyInterface->SetPixelShader(0);
    ProxyInterface->SetRenderState(D3DRS_ALPHAREF, 2);
    ProxyInterface->SetRenderState(D3DRS_FOGENABLE, 1);
}

void ButtonIcons::HandleControllerIcons(LPDIRECT3DDEVICE8 ProxyInterface)
{
    if (!IsInControlOptionsMenu())
    {
        return;
    }

    DirectXInterface = ProxyInterface;

    this->UpdateBinds();
}

void ButtonIcons::TranslateVertexBuffer(TexturedVertex* vertices, int count, float x, float y)
{
    D3DXMATRIX TranslateMatrix;
    D3DXMatrixTranslation(&TranslateMatrix, x, y, 0.f);

    this->ApplyVertexBufferTransformation(vertices, count, TranslateMatrix);
}

void ButtonIcons::ScaleVertexBuffer(TexturedVertex* vertices, int count, float x, float y)
{
    D3DXMATRIX ScalingMatrix;
    D3DXMatrixScaling(&ScalingMatrix, x, y, 1.f);

    this->ApplyVertexBufferTransformation(vertices, count, ScalingMatrix);
}

void ButtonIcons::ApplyVertexBufferTransformation(TexturedVertex* vertices, int count, D3DXMATRIX matrix)
{
    D3DXVECTOR3 temp;

    for (int i = 0; i < count; i++)
    {
        D3DXVec3TransformCoord(&temp, &vertices[i].coords, &matrix);

        vertices[i].coords = temp;
    }
}

void ButtonIcons::Init(LPDIRECT3DDEVICE8 ProxyInterface)
{
    if (!DirectXInterface)
    {
        return;
    }
    
    if (ButtonIconsTexture == NULL)
    {
        HRESULT hr = ProxyInterface->CreatePixelShader(subtractionPixelShaderAsm, &this->SubtractionPixelShader);

        if (FAILED(hr))
        {
            Logging::Log() << __FUNCTION__ << " ERROR: Couldn't create pixel shader: " << Logging::hex(hr);
            return;
        }

        hr = D3DXCreateTextureFromFile(DirectXInterface, L"icon_set.png", &ButtonIconsTexture);

        if (FAILED(hr))
        {
            //TODO setting = false
            Logging::Log() << __FUNCTION__ << " ERROR: Couldn't create texture: " << Logging::hex(hr);
            return;
        }
    }

    this->LastBufferHeight = BufferHeight;
    this->LastBufferWidth = BufferWidth;

    int32_t VerticalInternal = GetInternalVerticalRes();
    int32_t HorizontalInternal = GetInternalHorizontalRes();

    const float xScaling = (float)HorizontalInternal / 1200.f;
    const float yScaling = (float)VerticalInternal / 900.f;

    const float HorizontalOffset = (1091.f * (float)HorizontalInternal) / 1200.f;
    const float VerticalOffset = (141.f * (float)VerticalInternal) / 900.f;

    const float x = (76.f * (float)HorizontalInternal) / 1200.f;
    const float y = (57.f * (float)VerticalInternal) / 900.f;

    for (int i = 0; i < BUTTON_QUADS_NUM; i++)
    {
        this->quads[i].vertices[0].coords = { 0.f, 0.f, 0.5f };
        this->quads[i].vertices[1].coords = { 0.f,   y, 0.5f };
        this->quads[i].vertices[2].coords = {   x,   y, 0.5f };
        this->quads[i].vertices[3].coords = {   x, 0.f, 0.5f };

        this->TranslateVertexBuffer(this->quads[i].vertices, 4, HorizontalOffset, VerticalOffset + (i * y));
        this->ScaleVertexBuffer(this->quads[i].vertices, 4, xScaling, yScaling);
    }
}

void ButtonIcons::UpdateBinds()
{
    if (!this->ControllerBindsAddr)
    {
        this->ControllerBindsAddr = GetKeyBindsPointer() + 0xD0;
    }
    
    this->binds[0] = ControllerButton::L_LEFT;
    this->binds[1] = ControllerButton::L_RIGHT;
    this->binds[2] = ControllerButton::L_UP;
    this->binds[3] = ControllerButton::L_DOWN;

    for (int i = 0; (i + 4) < this->BindsNum; i++)
    {
        // the first 4 keybinds are static, the movement stick
        this->binds[i + 4] = (ControllerButton) this->ControllerBindsAddr[i * 0x08];
    }

    ButtonIconsRef.UpdateUVs();
}