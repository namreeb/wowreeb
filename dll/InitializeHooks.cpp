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

namespace Classic
{
class CVar {};

using SetT = bool(__thiscall CVar::*)(const char *, char, char, char, char);
using InitializeT = const char *(__fastcall *)(void *);

const char *InitializeHook(hadesmem::PatchDetourBase *detour, void *pThis, char *authServer)
{
    auto const initialize = detour->GetTrampolineT<InitializeT>();
    auto const ret = (*initialize)(pThis);

    auto const cvar = *reinterpret_cast<CVar **>(0xC2812C);
    auto const set = hadesmem::detail::AliasCast<SetT>(0x63DF50);

    auto const pDone = authServer + strlen(authServer) + 1;

    (cvar->*set)(authServer, 1, 0, 1, 0);

    detour->Remove();

    *pDone = true;

    return ret;
}

void ApplyClientInitHook(char *authServer, float fov)
{
    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const initializeOrig = hadesmem::detail::AliasCast<InitializeT>(0x5AB680);
    auto initializeDetour = new hadesmem::PatchDetour<InitializeT>(proc, initializeOrig,
        [authServer] (hadesmem::PatchDetourBase *detour, void *pThis)
    {
        return InitializeHook(detour, pThis, authServer);
    });

    initializeDetour->Apply();

    if (fov > 0.01f)
    {
        auto const pFov = reinterpret_cast<PVOID>(0x8089B4);
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

    auto const cvar = *reinterpret_cast<CVar **>(0xD43330);
    auto const set = hadesmem::detail::AliasCast<SetT>(0x63F6C0);

    auto const pDone = AuthServer + strlen(AuthServer) + 1;

    (cvar->*set)(AuthServer, 1, 0, 1, 0);

    detour->Remove();

    *pDone = true;

    return ret;
}

void ApplyClientInitHook(char *authServer, float fov)
{
    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const initializeOrig = hadesmem::detail::AliasCast<InitializeT>(0x5B3D00);
    auto initializeDetour = new hadesmem::PatchDetour<InitializeT>(proc, initializeOrig,
        [authServer] (hadesmem::PatchDetourBase *detour, void *pThis)
    {
        return InitializeHook(detour, pThis, authServer);
    });

    initializeDetour->Apply();

    if (fov > 0.01f)
    {
        auto const pFov = reinterpret_cast<PVOID>(0x8B5A04);
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

    auto const cvar = *reinterpret_cast<CVar **>(0xC79D00);
    auto const set = hadesmem::detail::AliasCast<SetT>(0x7668C0);

    auto const pDone = authServer + strlen(authServer) + 1;

    (cvar->*set)(authServer, 1, 0, 1, 0);

    *pDone = true;

    detour->Remove();
}

void ApplyClientInitHook(char *authServer, float fov)
{
    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const initializeOrig = hadesmem::detail::AliasCast<InitializeT>(0x6B0BF0);
    auto initializeDetour = new hadesmem::PatchDetour<InitializeT>(proc, initializeOrig,
        [authServer] (hadesmem::PatchDetourBase *detour, void *arg, const char *locale)
    {
        InitializeHook(detour, arg, locale, authServer);
    });

    initializeDetour->Apply();

    if (fov > 0.01f)
    {
        auto const pFov = reinterpret_cast<PVOID>(0x9E8D88);
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

    auto const cvar = *reinterpret_cast<CVar **>(0xDBE800);
    auto const set = hadesmem::detail::AliasCast<SetT>(0x6553B0);

    auto const pDone = authServer + strlen(authServer) + 1;

    (cvar->*set)(authServer, 1, 0, 1, 0);

    *pDone = true;

    detour->Remove();
}

void ApplyClientInitHook(char *authServer, float fov)
{
    auto const proc = hadesmem::Process(::GetCurrentProcessId());
    auto const initializeOrig = hadesmem::detail::AliasCast<InitializeT>(0x4CEDF0);
    auto initializeDetour = new hadesmem::PatchDetour<InitializeT>(proc, initializeOrig,
        [authServer](hadesmem::PatchDetourBase *detour)
    {
        InitializeHook(detour, authServer);
    });

    initializeDetour->Apply();
}
}