#include "d3d_pin.hpp"

#include <windows.h>

#include <initializer_list>

namespace host {

bool pinDirect3D()
{
  bool all = true;
  for(const wchar_t* name : {L"dxgi.dll", L"d3d11.dll"}) {
    HMODULE module = LoadLibraryW(name);   // not the one that is "kept": the pin below is
    HMODULE pinned = nullptr;
    all = all && module && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, name, &pinned) && pinned;
  }
  return all;
}

}  // namespace host
