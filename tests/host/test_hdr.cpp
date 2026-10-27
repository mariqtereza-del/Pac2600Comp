// The HDR decisions and arithmetic (Stage I10.1, D-016). No window, no ROM. Expected values are worked out by hand from the rules in hdr.hpp.
#include <algorithm>
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

  // the colour of a highlight: as bright as the slider says, and still the same colour (more vivid, not whiter)
  const Rgb pastelGreen{0.178f, 0.791f, 0.175f};   // a ghost of the game
  const Rgb yellow{0.610f, 0.429f, 0.068f};        // Pac-Man
  for(float scale : {1.0f, 1.75f, 2.5f, 4.0f}) {
    for(const Rgb& c : {pastelGreen, yellow}) {
      const Rgb h = highlightColour(c, scale);
      expect(std::fabs(luminance(h) - scale * luminance(c)) < 0.01f * scale, "a highlight is exactly the scale times as bright (scale " + std::to_string(scale) + ")");
      expect(h.r >= 0 && h.g >= 0 && h.b >= 0, "no channel below 0");
      expect(std::max({h.r, h.g, h.b}) <= scale + 1e-4f, "no channel above the scale");
    }
  }
  expect(near(highlightColour(pastelGreen, 1.0f).g, pastelGreen.g) && near(highlightColour(pastelGreen, 1.0f).r, pastelGreen.r), "scale 1 leaves the colour as it is");
  {
    // whiter or not: the share of the weakest channel in the colour must not grow, so a pale colour does not turn white
    auto whiteness = [](const Rgb& c) { return std::min({c.r, c.g, c.b}) / std::max({c.r, c.g, c.b}); };
    expect(whiteness(highlightColour(pastelGreen, 1.75f)) < whiteness(pastelGreen) && whiteness(highlightColour(pastelGreen, 4.0f)) < whiteness(highlightColour(pastelGreen, 1.75f)),
           "the brighter a pale green is made, the less white it is (it was white at 25 percent when only multiplied)");
    expect(whiteness(highlightColour(yellow, 4.0f)) <= whiteness(yellow), "and the yellow too");
    const Rgb plain = {pastelGreen.r * 1.75f, pastelGreen.g * 1.75f, pastelGreen.b * 1.75f};
    expect(whiteness(highlightColour(pastelGreen, 1.75f)) < whiteness(plain), "more vivid than the same colour just multiplied");
  }
  expect(highlightColour(Rgb{0, 0, 0}, 4.0f).g == 0.0f, "black stays black");
  const Rgb primary = highlightColour(Rgb{0.0f, 1.0f, 0.0f}, 4.0f);
  expect(primary.g <= 4.0f + 1e-4f && primary.r == 0.0f, "a pure green is held to the scale");

  std::printf("%d checks, %d failures\n", checks, failures);
  if(failures == 0) std::printf("ALL TESTS PASSED\n");
  return failures == 0 ? 0 : 1;
}
