// Tests for src\host\settings and src\host\windowstate (Stage D7).
// Usage: test_windowstate <scratch folder>   (only small ini files are written there; nothing is deleted)
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../../src/host/rom_finder.hpp"
#include "../../src/host/settings.hpp"
#include "../../src/host/windowstate.hpp"

namespace fs = std::filesystem;
using windowstate::Area;
using windowstate::State;

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}

static void writeFile(const std::string& path, const std::string& text)
{
  std::ofstream out(path, std::ios::trunc);
  out << text;
}

int main(int argc, char* argv[])
{
  if(argc < 2) { std::printf("usage: test_windowstate <scratch folder>\n"); return 2; }
  const fs::path dir = argv[1];
  std::error_code ec;
  fs::create_directories(dir, ec);
  auto S = [&](const char* name) { return (dir / name).string(); };

  // settings: a missing file/key is empty; set keeps the other lines; set replaces only its own line
  const std::string ini = S("d7_settings.ini");
  expect(settings::get(S("d7_never_written.ini"), "a").empty(), "missing file gives an empty value");
  writeFile(ini, "rom_path=C:\\games\\pacman.a26\r\nother=1\r\n");
  expect(settings::get(ini, "rom_path") == "C:\\games\\pacman.a26", "reads a value from a file with Windows line ends");
  expect(settings::get(ini, "missing").empty(), "missing key gives an empty value");
  expect(settings::set(ini, "window_x", "40"), "set adds a new key");
  expect(settings::set(ini, "other", "2"), "set replaces an existing key");
  expect(settings::get(ini, "rom_path") == "C:\\games\\pacman.a26" && settings::get(ini, "window_x") == "40" && settings::get(ini, "other") == "2",
         "all three keys are present afterwards");
  expect(settings::set(ini, "window_x", "41") && settings::get(ini, "window_x") == "41", "a key is not duplicated");

  // saving the ROM path must keep the window lines, and the other way round
  windowstate::save(ini, {true, {10, 20, 800, 600}, false});
  expect(rom::savePath(ini, (dir / "x.a26").string()), "the ROM path can be saved next to window state");
  expect(windowstate::load(ini).known && windowstate::load(ini).area.w == 800, "window state survived saving the ROM path");
  windowstate::save(ini, {true, {11, 21, 801, 601}, true});
  expect(!rom::loadSavedPath(ini).empty(), "ROM path survived saving the window state");

  // state: round trip, defaults, bad data
  const std::string st = S("d7_state.ini");
  writeFile(st, "");
  expect(!windowstate::load(st).known, "an empty file remembers nothing");
  const State a{true, {-1920, 100, 1280, 960}, true};  // a monitor to the left has negative coordinates
  expect(windowstate::save(st, a), "state is saved");
  const State b = windowstate::load(st);
  expect(b.known && b.area.x == -1920 && b.area.y == 100 && b.area.w == 1280 && b.area.h == 960 && b.fullscreen, "state round trip, negative position included");
  writeFile(st, "window_x=10\nwindow_y=10\nwindow_w=abc\nwindow_h=500\n");
  expect(!windowstate::load(st).known, "a non-number remembers nothing");
  writeFile(st, "window_x=10\nwindow_y=10\nwindow_w=10\nwindow_h=10\n");
  expect(!windowstate::load(st).known, "a tiny size remembers nothing");
  writeFile(st, "window_x=10\nwindow_y=10\nwindow_w=800\n");
  expect(!windowstate::load(st).known, "a missing key remembers nothing");
  writeFile(st, "window_x=10\nwindow_y=10\nwindow_w=800\nwindow_h=600\n");
  expect(windowstate::load(st).known && !windowstate::load(st).fullscreen, "no fullscreen line means windowed");

  // fitToMonitors
  const std::vector<Area> one{{0, 0, 2560, 1440}};
  const std::vector<Area> two{{0, 0, 2560, 1440}, {2560, 0, 1920, 1080}};
  const std::vector<Area> left{{0, 0, 2560, 1440}, {-1920, 0, 1920, 1080}};
  auto same = [](const State& x, const State& y) { return x.area.x == y.area.x && x.area.y == y.area.y && x.area.w == y.area.w && x.area.h == y.area.h; };

  const State fine{true, {300, 200, 1280, 960}, false};
  expect(same(windowstate::fitToMonitors(fine, one), fine), "a window on the monitor is left alone");
  expect(!windowstate::fitToMonitors(State{}, one).known, "nothing remembered stays nothing remembered");
  expect(same(windowstate::fitToMonitors(fine, {}), fine), "no monitor list: unchanged");

  const State onSecond{true, {2800, 100, 1000, 700}, false};
  expect(same(windowstate::fitToMonitors(onSecond, two), onSecond), "a window on the second monitor is left alone");
  const State gone = windowstate::fitToMonitors(onSecond, one);
  expect(gone.area.x == (2560 - 1000) / 2 && gone.area.y == (1440 - 700) / 2 && gone.area.w == 1000 && gone.area.h == 700,
         "the second monitor is unplugged: the window is centred on the first");
  const State onLeft{true, {-1500, 100, 1000, 700}, false};
  expect(same(windowstate::fitToMonitors(onLeft, left), onLeft), "a window on a monitor with negative coordinates is left alone");
  expect(windowstate::fitToMonitors(onLeft, one).area.x == (2560 - 1000) / 2, "that monitor gone: centred on the first");

  const State huge = windowstate::fitToMonitors(State{true, {0, 40, 9000, 7000}, false}, one);
  expect(huge.area.w == 2560 && huge.area.h == 1440, "a too-large window is cut down to the biggest monitor");
  const State tiny = windowstate::fitToMonitors(State{true, {100, 100, 50, 50}, false}, one);
  expect(tiny.area.w == windowstate::kMinW && tiny.area.h == windowstate::kMinH, "a too-small window is raised to the minimum");
  const State above = windowstate::fitToMonitors(State{true, {300, -200, 1000, 700}, false}, one);
  expect(above.area.y == (1440 - 700) / 2, "a title bar above the screen is brought back");
  const State below = windowstate::fitToMonitors(State{true, {300, 1430, 1000, 700}, false}, one);
  expect(below.area.y == (1440 - 700) / 2, "a window hanging off the bottom edge is brought back");
  const State farRight = windowstate::fitToMonitors(State{true, {5000, 100, 1000, 700}, true}, two);
  expect(farRight.area.x == (2560 - 1000) / 2 && farRight.fullscreen, "far off to the right is brought back; the fullscreen flag is kept");

  std::printf("\n%s (%d checks, %d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", checks, failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
