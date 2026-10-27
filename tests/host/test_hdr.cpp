// The HDR decisions and arithmetic (Stage I10.1, D-016). No window, no ROM. Expected values are worked out by hand from the rules in hdr.hpp.
#include <cmath>
#include <cstdio>
#include <string>

#include "../../src/host/hdr.hpp"

namespace {
int checks = 0, failures = 0;
void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}
bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }
}  // namespace

int main()
{
  using namespace hdr;
  ScreenState off;                          // Windows not in HDR mode: 1.0 white, 1.0 headroom
  ScreenState on{true, 1.15f, 4.54f};       // the owner's screen as measured: HDR on, white 1.15, headroom 4.54
  ScreenState dim{true, 1.0f, 1.5f};        // a modest HDR screen
  ScreenState odd{true, 1.0f, 1.0f};        // says HDR but has no room above white

  // when the program uses the HDR pipeline: wanted, and Windows in HDR mode with room above white
  expect(useHdr(true, on), "HDR wanted and Windows in HDR mode: used");
  expect(!useHdr(false, on), "HDR turned off in the menu: not used, whatever Windows says");
  expect(!useHdr(true, off), "Windows not in HDR mode: not used");
  expect(!useHdr(true, odd), "HDR mode without any room above white: not used");
  expect(useHdr(true, dim), "a modest HDR screen: used");

  // the most the highlights may reach: the screen's headroom, but no more than 4 times white, and never below white
  expect(near(ceilingOf(on), 4.0f), "headroom 4.54 gives a ceiling of 4 (the glare limit)");
  expect(near(ceilingOf(dim), 1.5f), "headroom 1.5 gives a ceiling of 1.5");
  expect(near(ceilingOf(off), 1.0f), "no HDR: the ceiling is white");
  expect(near(ceilingOf(ScreenState{true, 1.0f, 0.5f}), 1.0f), "a nonsense headroom below 1 is white");

  // the slider: 0 is plain white, 1 is the ceiling, in between in a straight line; never above the ceiling
  expect(near(highlightScale(0.0f, on, true), 1.0f), "slider at 0: no highlight");
  expect(near(highlightScale(1.0f, on, true), 4.0f), "slider at the top: the ceiling");
  expect(near(highlightScale(0.5f, on, true), 2.5f), "slider half way: half way from 1 to 4");
  expect(near(highlightScale(1.0f, dim, true), 1.5f), "on a modest screen the top is its headroom");
  expect(near(highlightScale(2.0f, on, true), 4.0f) && near(highlightScale(-1.0f, on, true), 1.0f), "a slider value outside 0..1 is held to the ends");
  expect(near(highlightScale(1.0f, on, false), 1.0f), "HDR not in use: always white, whatever the slider");
  for(int i = 0; i <= 20; ++i) {
    const float s = static_cast<float>(i) / 20.0f;
    expect(highlightScale(s, on, true) <= on.headroom && highlightScale(s, on, true) >= 1.0f, "the highlight never goes above the screen's headroom or below white");
  }

  std::printf("%d checks, %d failures\n", checks, failures);
  if(failures == 0) std::printf("ALL TESTS PASSED\n");
  return failures == 0 ? 0 : 1;
}
