// The frame rate choices and arithmetic (Stage I9.4, T-I9-4). No window, no ROM. Expected values are worked out by hand from the rules in framerate.hpp.
#include <cmath>
#include <cstdio>
#include <string>

#include "../../src/host/framerate.hpp"

namespace {
int checks = 0, failures = 0;
void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
}  // namespace

int main()
{
  using namespace framerate;
  // the six choices, in the order the owner gave them
  const char* settings[kRates] = {"30", "60", "75", "120", "144", "unlocked"};
  const char* titles[kRates] = {"30", "60", "75", "120", "144", "Unlocked"};
  for(int i = 0; i < kRates; ++i) {
    expect(std::string(settingName(static_cast<Rate>(i))) == settings[i], std::string("setting name ") + settings[i]);
    expect(std::string(title(static_cast<Rate>(i))) == titles[i], std::string("title ") + titles[i]);
    expect(fromSetting(settings[i]) == static_cast<Rate>(i), std::string("setting round trip ") + settings[i]);
  }
  expect(fromSetting("") == Rate::Fps60 && fromSetting("90") == Rate::Fps60 && fromSetting("Unlocked") == Rate::Fps60, "anything unknown is 60");

  // the list goes round, both ways
  expect(next(Rate::Fps30) == Rate::Fps60 && next(Rate::Fps144) == Rate::Unlocked && next(Rate::Unlocked) == Rate::Fps30, "next goes round the list");
  expect(next(Rate::Fps30, -1) == Rate::Unlocked && next(Rate::Fps60, -1) == Rate::Fps30, "and back");

  // what each one means
  expect(!smooth(Rate::Fps30) && !smooth(Rate::Fps60), "30 and 60 show the game's own pictures");
  expect(smooth(Rate::Fps75) && smooth(Rate::Fps120) && smooth(Rate::Fps144) && smooth(Rate::Unlocked), "75 and up are smooth");
  expect(unlocked(Rate::Unlocked) && !unlocked(Rate::Fps144), "only Unlocked is unlocked");
  expect(perSecond(Rate::Fps30) == 30 && perSecond(Rate::Fps144) == 144 && perSecond(Rate::Unlocked) == 0, "pictures a second");

  // the moments pictures are made: every 1/120 s at 120 a second, whatever the game step
  expect(pictureIndex(Rate::Fps120, 0.0) == 0 && pictureIndex(Rate::Fps120, 1.0 / 120.0 + 1e-9) == 1 && pictureIndex(Rate::Fps120, 1.0) == 120, "120: a picture every 1/120 s");
  expect(near(pictureTime(Rate::Fps120, 0.0123), 1.0 / 120.0), "120: a picture is made at the moment its grid begins");
  expect(pictureIndex(Rate::Unlocked, 5.0) == -1 && near(pictureTime(Rate::Unlocked, 5.1234), 5.1234), "unlocked: no grid, the moment as it is");
  // how far through a step (1/60 s long, begun at 10 s)
  const double step = 1.0 / 60.0;
  expect(near(progress(10.0, 10.0, step), 0.0), "the start of a step is 0");
  expect(near(progress(10.0 + step / 2, 10.0, step), 0.5), "half way is 0.5");
  expect(near(progress(9.9, 10.0, step), 0.0), "a picture made before the step began is at its start");
  expect(progress(11.0, 10.0, step) < 1.0 && progress(11.0, 10.0, step) > 0.99, "a step that goes on longer stays just below 1");
  // at 144 a second pictures come every 1/144 s: in 1/60 s of the clock there are 2 or 3 of them, each at or after the one before
  int pictures = 0;
  long long last = -2;
  bool inOrder = true;
  double lastProgress = -1;
  for(double t = 10.0; t < 10.0 + step; t += 0.0002) {
    const long long index = pictureIndex(Rate::Fps144, t);
    const double p = progress(pictureTime(Rate::Fps144, t), 10.0, step);
    if(index != last) ++pictures;
    if(p < lastProgress) inOrder = false;
    last = index;
    lastProgress = p;
  }
  expect(pictures >= 2 && pictures <= 3 && inOrder, "at 144 a second a step of 1/60 s has 2 or 3 pictures, in order");

  // 30 shows every second step
  expect(shownStep(Rate::Fps30, 10) == 10 && shownStep(Rate::Fps30, 11) == 10, "30 shows the even steps");
  expect(shownStep(Rate::Fps60, 11) == 11 && shownStep(Rate::Fps144, 11) == 11, "the others show each step");

  std::printf("%d checks, %d failures\n", checks, failures);
  if(failures == 0) std::printf("ALL TESTS PASSED\n");
  return failures == 0 ? 0 : 1;
}
