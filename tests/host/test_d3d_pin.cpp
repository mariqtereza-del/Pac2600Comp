// Test for src\host\d3d_pin (Stage I16): once the Direct3D libraries are pinned, letting go of them as often as SDL does does not unload them.
// Usage: test_d3d_pin
#include <windows.h>

#include <cstdio>
#include <initializer_list>

#include "../../src/host/d3d_pin.hpp"

int main()
{
  int failures = 0;
  auto expect = [&](bool ok, const char* what) { if(!ok) { ++failures; std::printf("FAIL  %s\n", what); } };
  expect(GetModuleHandleW(L"d3d11.dll") == nullptr, "d3d11.dll is not loaded before anything asks for it (the test program does not use it)");
  expect(host::pinDirect3D(), "the libraries load and are pinned");
  expect(host::pinDirect3D(), "pinning again is safe");
  for(const wchar_t* name : {L"d3d11.dll", L"dxgi.dll"}) {
    HMODULE module = GetModuleHandleW(name);
    expect(module != nullptr, "loaded after pinning");
    for(int i = 0; i < 10; ++i) FreeLibrary(module);   // what SDL's renderer does when it recovers from a lost device, and more
    expect(GetModuleHandleW(name) != nullptr, "still loaded after ten FreeLibrary calls");
  }
  std::printf("%s (%d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", failures);
  return failures == 0 ? 0 : 1;
}
