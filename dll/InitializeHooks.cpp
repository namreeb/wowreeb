/*
    MIT License

    Copyright (c) 2017 namreeb http://github.com/namreeb legal@namreeb.org

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

#include <hadesmem/patcher.hpp>

#include <cstdint>

namespace
{
enum class Version
{
    Classic = 0,
    TBC,
    WotLK,
    Cata,
    Total
};

enum class Offset
{
    CVar__Set = 0,
    RealmListCVar,
    Initialize,
    FoV,
    Total
};

PVOID GetAddress(Version version, Offset offset)
{
    static constexpr std::uint32_t offsets[static_cast<int>(Version::Total)][static_cast<int>(Offset::Total)] =
    {
        // Classic
        {
            0x23DF50,
            0x82812C,
            0x1AB680,
            0x4089B4
        },
        // TBC
        {
            0x23F6C0,
            0x943330,
            0x1B3D00,
            0x4B5A04
        },
        // WotLK
        {
            0x3668C0,
            0x879D00,
            0x2B0BF0,
            0x5E8D88
        },
        // Cata
        {
            0x2553B0,
            0x9BE800,
            0x0CEDF0,
            0x00, // not supported.  let me know if anyone actually wants this
        }
    };

    auto const baseAddress = reinterpret_cast<std::uint8_t *>(::GetModuleHandle(nullptr));

    return baseAddress + offsets[static_cast<int>(version)][static_cast<int>(offset)];
}
}

namespace Classic
{
class CVar {};

using SetT = bool(__thiscall CVar::*)(const char *, char, char, char, char);
using InitializeT = const char *(__fastcall *)(void *);

const char *InitializeHook(hadesmem::PatchDetourBase *detour, void *pThis, char *authServer)
{
    auto const initialize = detour->GetTrampolineT<InitializeT>();
    auto const ret = (*initialize)(pThis);

    auto const cvar = *reinterpret_cast<CVar **>(GetAddress(Version::Classic, Offset::RealmListCVar));
    auto const set = hadesmem::detail::AliasCast<SetT>(GetAddress(Version::Classic, Offset::CVar__Set));

    auto const pDone = authServer + strlen(authServer) + 1;

    (cvar->*set)(authServer, 1, 0, 1, 0);

    detour->Remove();

    *pDone = true;

    return ret;
}

void ApplyClientInitHook(char *authServer, float fov)
{
    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const initializeOrig = hadesmem::detail::AliasCast<InitializeT>(GetAddress(Version::Classic, Offset::Initialize));
    auto initializeDetour = new hadesmem::PatchDetour<InitializeT>(proc, initializeOrig,
        [authServer] (hadesmem::PatchDetourBase *detour, void *pThis)
    {
        return InitializeHook(detour, pThis, authServer);
    });

    initializeDetour->Apply();

    if (fov > 0.01f)
    {
        auto const pFov = GetAddress(Version::Classic, Offset::FoV);
        std::vector<std::uint8_t> patchData(sizeof(fov));
        memcpy(&patchData[0], &fov, sizeof(fov));
        auto patch = new hadesmem::PatchRaw(proc, pFov, patchData);
        patch->Apply();
    }
}
}

namespace TBC
{
class CVar {};

using SetT = bool(__thiscall CVar::*)(const char *, char, char, char, char);
using InitializeT = const char * (*)(void *);

const char *InitializeHook(hadesmem::PatchDetourBase *detour, void *arg, char *AuthServer)
{
    auto const initialize = detour->GetTrampolineT<InitializeT>();
    auto const ret = (*initialize)(arg);

    auto const cvar = *reinterpret_cast<CVar **>(GetAddress(Version::TBC, Offset::RealmListCVar));
    auto const set = hadesmem::detail::AliasCast<SetT>(GetAddress(Version::TBC, Offset::CVar__Set));

    auto const pDone = AuthServer + strlen(AuthServer) + 1;

    (cvar->*set)(AuthServer, 1, 0, 1, 0);

    detour->Remove();

    *pDone = true;

    return ret;
}

void ApplyClientInitHook(char *authServer, float fov)
{
    MessageBoxA(nullptr, "Initialize", "DEBUG", 0);

    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const initializeOrig = hadesmem::detail::AliasCast<InitializeT>(GetAddress(Version::TBC, Offset::Initialize));
    auto initializeDetour = new hadesmem::PatchDetour<InitializeT>(proc, initializeOrig,
        [authServer] (hadesmem::PatchDetourBase *detour, void *pThis)
    {
        return InitializeHook(detour, pThis, authServer);
    });

    initializeDetour->Apply();

    if (fov > 0.01f)
    {
        auto const pFov = GetAddress(Version::TBC, Offset::FoV);
        std::vector<std::uint8_t> patchData(sizeof(fov));
        memcpy(&patchData[0], &fov, sizeof(fov));
        auto patch = new hadesmem::PatchRaw(proc, pFov, patchData);
        patch->Apply();
    }
}
}

namespace WOTLK
{
class CVar {};

using SetT = bool(__thiscall CVar::*)(const char *, char, char, char, char);
using InitializeT = void (__cdecl *)(void *, const char *);

void InitializeHook(hadesmem::PatchDetourBase *detour, void *arg, const char *locale, char *authServer)
{
    auto const initialize = detour->GetTrampolineT<InitializeT>();
    (*initialize)(arg, locale);

    auto const cvar = *reinterpret_cast<CVar **>(GetAddress(Version::WotLK, Offset::RealmListCVar));
    auto const set = hadesmem::detail::AliasCast<SetT>(GetAddress(Version::WotLK, Offset::CVar__Set));

    auto const pDone = authServer + strlen(authServer) + 1;

    (cvar->*set)(authServer, 1, 0, 1, 0);

    *pDone = true;

    detour->Remove();
}

void ApplyClientInitHook(char *authServer, float fov)
{
    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const initializeOrig = hadesmem::detail::AliasCast<InitializeT>(GetAddress(Version::WotLK, Offset::Initialize));
    auto initializeDetour = new hadesmem::PatchDetour<InitializeT>(proc, initializeOrig,
        [authServer] (hadesmem::PatchDetourBase *detour, void *arg, const char *locale)
    {
        InitializeHook(detour, arg, locale, authServer);
    });

    initializeDetour->Apply();

    if (fov > 0.01f)
    {
        auto const pFov = GetAddress(Version::WotLK, Offset::FoV);
        std::vector<std::uint8_t> patchData(sizeof(fov));
        memcpy(&patchData[0], &fov, sizeof(fov));
        auto patch = new hadesmem::PatchRaw(proc, pFov, patchData);
        patch->Apply();
    }
}
}

namespace Cata
{
class CVar {};

using SetT = bool(__thiscall CVar::*)(const char *, char, char, char, char);
using InitializeT = void(__cdecl *)();

void InitializeHook(hadesmem::PatchDetourBase *detour, char *authServer)
{
    auto const initialize = detour->GetTrampolineT<InitializeT>();
    (*initialize)();

    auto const cvar = *reinterpret_cast<CVar **>(GetAddress(Version::Cata, Offset::RealmListCVar));
    auto const set = hadesmem::detail::AliasCast<SetT>(GetAddress(Version::Cata, Offset::CVar__Set));

    auto const pDone = authServer + strlen(authServer) + 1;

    (cvar->*set)(authServer, 1, 0, 1, 0);

    *pDone = true;

    detour->Remove();
}

void ApplyClientInitHook(char *authServer, float fov)
{
    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const initializeOrig = hadesmem::detail::AliasCast<InitializeT>(GetAddress(Version::Cata, Offset::Initialize));
    auto initializeDetour = new hadesmem::PatchDetour<InitializeT>(proc, initializeOrig,
        [authServer](hadesmem::PatchDetourBase *detour)
    {
        InitializeHook(detour, authServer);
    });

    initializeDetour->Apply();
}
}