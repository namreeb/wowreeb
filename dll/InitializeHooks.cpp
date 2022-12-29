/*
    MIT License

    Copyright (c) 2018-2023 namreeb http://github.com/namreeb legal@namreeb.org

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

*/

#pragma comment(lib, "asmjit.lib")
#pragma comment(lib, "udis86.lib")

#include "InitializeHooks.hpp"

#include <cstdint>
#include <hadesmem/patcher.hpp>

namespace
{
enum class Version
{
    Classic = 0,
    TBC,
    WotLK,
    Cata32,
    Cata64,
    Total
};

enum class Offset
{
    CVar__Set = 0,
    RealmListCVar,
    Idle,
    CGlueMgr__m_pendingServerAlert,
    Login,
    FoV,
    Total
};

PVOID GetAddress(Version version, Offset offset)
{
    static constexpr std::uint32_t offsets[static_cast<int>(
        Version::Total)][static_cast<int>(Offset::Total)] = {
        // clang-format off
        // Classic
        {
            0x23DF50,
            0x82812C,
            0x6B930,
            0x741E28,
            0x6AFB0,
            0x4089B4
        },
        // TBC
        {
            0x23F6C0,
            0x943330,
            0x70160,
            0x807DB8,
            0x6E560,
            0x4B5A04
        },
        // WotLK
        {
            0x3668C0,
            0x879D00,
            0xDAB40,
            0x76AF88,
            0xD8A30,
            0x5E8D88
        },
        // Cata32
        {
            0x2553B0,
            0x9BE800,
            0x405310,
            0xABBF04,
            0x400240,
            0x00,   // not supported.  let me know if anyone actually wants this
        },
        // Cata64
        {
            0x2F61D0,
            0xCA4328,
            0x51A7C0,
            0xDACCA8,
            0x514100,
            0x00,   // not supported.  let me know if anyone actually wants this
        }
        // clang-format on
    };

    auto const baseAddress =
        reinterpret_cast<std::uint8_t*>(::GetModuleHandle(nullptr));

    return baseAddress +
           offsets[static_cast<int>(version)][static_cast<int>(offset)];
}
} // namespace

namespace Classic
{
class CVar
{
};

using SetT = bool (__thiscall CVar::*)(const char*, char, char, char, char);
using IdleT = int(__cdecl*)();
using LoginT = void(__fastcall*)(char*, char*);

int IdleHook(hadesmem::PatchDetourBase* detour, GameSettings* settings)
{
    auto const idle = detour->GetTrampolineT<IdleT>();
    auto const ret = idle();

    // if we are no longer waiting for the server alert, proceed with
    // configuration
    if (!*reinterpret_cast<std::uint32_t*>(
            GetAddress(Version::Classic, Offset::CGlueMgr__m_pendingServerAlert)))
    {
        auto const cvar = *reinterpret_cast<CVar**>(
            GetAddress(Version::Classic, Offset::RealmListCVar));
        auto const set = hadesmem::detail::AliasCast<SetT>(
            GetAddress(Version::Classic, Offset::CVar__Set));

        (cvar->*set)(settings->AuthServer, 1, 0, 1, 0);

        detour->Remove();

        if (settings->CredentialsSet)
        {
            auto const login = hadesmem::detail::AliasCast<LoginT>(
                GetAddress(Version::Classic, Offset::Login));
            login(settings->Username, settings->Password);
        }

        settings->LoadComplete = true;
    }

    return ret;
}

void ApplyClientInitHook(GameSettings* settings)
{
    // just to make sure the value is initialized
    *reinterpret_cast<std::uint32_t*>(
        GetAddress(Version::Classic, Offset::CGlueMgr__m_pendingServerAlert)) = 1;

    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const idleOrig = hadesmem::detail::AliasCast<IdleT>(
        GetAddress(Version::Classic, Offset::Idle));
    auto idleDetour = new hadesmem::PatchDetour<IdleT>(
        proc, idleOrig,
        [settings](hadesmem::PatchDetourBase* detour)
        { return IdleHook(detour, settings); });

    idleDetour->Apply();

    if (settings->FoVSet)
    {
        auto const pFov = GetAddress(Version::Classic, Offset::FoV);
        std::vector<std::uint8_t> patchData(sizeof(settings->FoV));
        memcpy(&patchData[0], &settings->FoV, sizeof(settings->FoV));
        auto patch = new hadesmem::PatchRaw(proc, pFov, patchData);
        patch->Apply();
    }
}
} // namespace Classic

namespace TBC
{
class CVar
{
};

using SetT = bool (__thiscall CVar::*)(const char*, char, char, char, char);
using IdleT = int(__cdecl*)();
using LoginT = void(__cdecl*)(char*, char*);

int IdleHook(hadesmem::PatchDetourBase* detour, GameSettings* settings)
{
    auto const idle = detour->GetTrampolineT<IdleT>();
    auto const ret = idle();

    // if we are no longer waiting for the server alert, proceed with
    // configuration
    if (!*reinterpret_cast<std::uint32_t*>(
            GetAddress(Version::TBC, Offset::CGlueMgr__m_pendingServerAlert)))
    {
        auto const cvar = *reinterpret_cast<CVar**>(
            GetAddress(Version::TBC, Offset::RealmListCVar));
        auto const set = hadesmem::detail::AliasCast<SetT>(
            GetAddress(Version::TBC, Offset::CVar__Set));

        (cvar->*set)(settings->AuthServer, 1, 0, 1, 0);

        detour->Remove();

        if (settings->CredentialsSet)
        {
            auto const login = hadesmem::detail::AliasCast<LoginT>(
                GetAddress(Version::TBC, Offset::Login));
            login(settings->Username, settings->Password);
        }

        settings->LoadComplete = true;
    }

    return ret;
}

void ApplyClientInitHook(GameSettings* settings)
{
    // just to make sure the value is initialized
    *reinterpret_cast<std::uint32_t*>(
        GetAddress(Version::TBC, Offset::CGlueMgr__m_pendingServerAlert)) = 1;

    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const idleOrig = hadesmem::detail::AliasCast<IdleT>(
        GetAddress(Version::TBC, Offset::Idle));
    auto idleDetour = new hadesmem::PatchDetour<IdleT>(
        proc, idleOrig,
        [settings](hadesmem::PatchDetourBase* detour)
        { return IdleHook(detour, settings); });

    idleDetour->Apply();

    if (settings->FoVSet)
    {
        auto const pFov = GetAddress(Version::TBC, Offset::FoV);
        std::vector<std::uint8_t> patchData(sizeof(settings->FoV));
        memcpy(&patchData[0], &settings->FoV, sizeof(settings->FoV));
        auto patch = new hadesmem::PatchRaw(proc, pFov, patchData);
        patch->Apply();
    }
}
} // namespace TBC

namespace WOTLK
{
class CVar
{
};

using SetT = bool (__thiscall CVar::*)(const char*, char, char, char, char);
using IdleT = int(__cdecl*)();
using LoginT = void(__cdecl*)(char*, char*);

int IdleHook(hadesmem::PatchDetourBase* detour, GameSettings* settings)
{
    auto const idle = detour->GetTrampolineT<IdleT>();
    auto const ret = idle();

    // if we are no longer waiting for the server alert, proceed with
    // configuration
    if (!*reinterpret_cast<std::uint32_t*>(
            GetAddress(Version::WotLK, Offset::CGlueMgr__m_pendingServerAlert)))
    {
        auto const cvar = *reinterpret_cast<CVar**>(
            GetAddress(Version::WotLK, Offset::RealmListCVar));
        auto const set = hadesmem::detail::AliasCast<SetT>(
            GetAddress(Version::WotLK, Offset::CVar__Set));

        (cvar->*set)(settings->AuthServer, 1, 0, 1, 0);

        detour->Remove();

        if (settings->CredentialsSet)
        {
            auto const login = hadesmem::detail::AliasCast<LoginT>(
                GetAddress(Version::WotLK, Offset::Login));
            login(settings->Username, settings->Password);
        }

        settings->LoadComplete = true;
    }

    return ret;
}

void ApplyClientInitHook(GameSettings* settings)
{
    // just to make sure the value is initialized
    *reinterpret_cast<std::uint32_t*>(
        GetAddress(Version::WotLK, Offset::CGlueMgr__m_pendingServerAlert)) = 1;

    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const idleOrig = hadesmem::detail::AliasCast<IdleT>(
        GetAddress(Version::WotLK, Offset::Idle));
    auto idleDetour = new hadesmem::PatchDetour<IdleT>(
        proc, idleOrig,
        [settings](hadesmem::PatchDetourBase* detour)
        { return IdleHook(detour, settings); });

    idleDetour->Apply();

    if (settings->FoVSet)
    {
        auto const pFov = GetAddress(Version::WotLK, Offset::FoV);
        std::vector<std::uint8_t> patchData(sizeof(settings->FoV));
        memcpy(&patchData[0], &settings->FoV, sizeof(settings->FoV));
        auto patch = new hadesmem::PatchRaw(proc, pFov, patchData);
        patch->Apply();
    }
}
} // namespace WOTLK

namespace Cata32
{
class CVar
{
};

using SetT = bool (__thiscall CVar::*)(const char*, char, char, char, char);
using IdleT = int(__cdecl*)();
using LoginT = void(__cdecl*)(char*, char*);

int IdleHook(hadesmem::PatchDetourBase* detour, GameSettings* settings)
{
    auto const idle = detour->GetTrampolineT<IdleT>();
    auto const ret = idle();

    // if we are no longer waiting for the server alert, proceed with
    // configuration
    if (!*reinterpret_cast<std::uint32_t*>(
            GetAddress(Version::Cata32, Offset::CGlueMgr__m_pendingServerAlert)))
    {
        auto const cvar = *reinterpret_cast<CVar**>(
            GetAddress(Version::Cata32, Offset::RealmListCVar));
        auto const set = hadesmem::detail::AliasCast<SetT>(
            GetAddress(Version::Cata32, Offset::CVar__Set));

        (cvar->*set)(settings->AuthServer, 1, 0, 1, 0);

        detour->Remove();

        if (settings->CredentialsSet)
        {
            auto const login = hadesmem::detail::AliasCast<LoginT>(
                GetAddress(Version::Cata32, Offset::Login));
            login(settings->Username, settings->Password);
        }

        settings->LoadComplete = true;
    }

    return ret;
}

void ApplyClientInitHook(GameSettings* settings)
{
    // just to make sure the value is initialized
    *reinterpret_cast<std::uint32_t*>(
        GetAddress(Version::Cata32, Offset::CGlueMgr__m_pendingServerAlert)) = 1;

    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const idleOrig = hadesmem::detail::AliasCast<IdleT>(
        GetAddress(Version::Cata32, Offset::Idle));
    auto idleDetour = new hadesmem::PatchDetour<IdleT>(
        proc, idleOrig,
        [settings](hadesmem::PatchDetourBase* detour)
        { return IdleHook(detour, settings); });

    idleDetour->Apply();
}
} // namespace Cata32

namespace Cata64
{
class CVar
{
};

using SetT = bool (__fastcall CVar::*)(const char*, char, char, char, char);
using IdleT = int(__stdcall*)();
using LoginT = void(__fastcall*)(char*, char*);

int IdleHook(hadesmem::PatchDetourBase* detour, GameSettings* settings)
{
    auto const idle = detour->GetTrampolineT<IdleT>();
    auto const ret = idle();

    // if we are no longer waiting for the server alert, proceed with
    // configuration
    if (!*reinterpret_cast<std::uint32_t*>(
            GetAddress(Version::Cata64, Offset::CGlueMgr__m_pendingServerAlert)))
    {
        auto const cvar = *reinterpret_cast<CVar**>(
            GetAddress(Version::Cata64, Offset::RealmListCVar));
        auto const set = hadesmem::detail::AliasCast<SetT>(
            GetAddress(Version::Cata64, Offset::CVar__Set));

        (cvar->*set)(settings->AuthServer, 1, 0, 1, 0);

        detour->Remove();

        if (settings->CredentialsSet)
        {
            auto const login = hadesmem::detail::AliasCast<LoginT>(
                GetAddress(Version::Cata64, Offset::Login));
            login(settings->Username, settings->Password);
        }

        settings->LoadComplete = true;
    }

    return ret;
}

void ApplyClientInitHook(GameSettings* settings)
{
    // just to make sure the value is initialized
    *reinterpret_cast<std::uint32_t*>(
        GetAddress(Version::Cata64, Offset::CGlueMgr__m_pendingServerAlert)) = 1;

    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const idleOrig = hadesmem::detail::AliasCast<IdleT>(
        GetAddress(Version::Cata64, Offset::Idle));
    auto idleDetour = new hadesmem::PatchDetour<IdleT>(
        proc, idleOrig,
        [settings](hadesmem::PatchDetourBase* detour)
        { return IdleHook(detour, settings); });

    idleDetour->Apply();
}
} // namespace Cata64