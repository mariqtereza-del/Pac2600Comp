// Keeps the Direct3D libraries loaded for as long as the program runs (Stage I16, T-I16-1). When Direct3D reports that the graphics device was removed (a driver reset, a
// timeout, an unplugged or changed screen), SDL's renderer makes a new device inside the same renderer and, doing so, unloads d3d11.dll and dxgi.dll. Direct3D objects that
// the CRT filters were still holding then pointed into an unloaded library, and the next call into one crashed the program. A library that is pinned cannot be unloaded.
#pragma once

namespace host {

// Loads d3d11.dll and dxgi.dll and pins them (Windows: GetModuleHandleEx with GET_MODULE_HANDLE_EX_FLAG_PIN). Safe to call again. True when both are loaded.
bool pinDirect3D();

}  // namespace host
