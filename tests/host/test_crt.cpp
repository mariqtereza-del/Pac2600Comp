// The CRT filter choices (Stage I11, D-017). No window, no ROM.
#include <cstdio>
#include <string>

#include "../../src/host/crt.hpp"

namespace {
int checks = 0, failures = 0;
void expect(bool ok, const std::string& what)
{
  ++checks;
  if(!ok) { ++failures; std::printf("FAIL  %s\n", what.c_str()); }
}
}  // namespace

int main()
{
  using namespace crt;
  expect(std::string(settingName(Filter::Off)) == "off" && std::string(settingName(Filter::Lottes)) == "lottes" && std::string(settingName(Filter::Geom)) == "geom" &&
             std::string(settingName(Filter::GuestNtsc)) == "gdv-ntsc",
         "the setting names");
  expect(std::string(title(Filter::GuestNtsc)) == "GDV NTSC 2600" && std::string(title(Filter::Lottes)) == "CRT-Lottes" && std::string(title(Filter::Geom)) == "CRT-Geom", "the titles");
  expect(implemented(Filter::Off) && implemented(Filter::Lottes), "Off and CRT-Lottes are there");
  expect(fromSetting("lottes") == Filter::Lottes && fromSetting("off") == Filter::Off, "a setting gives its filter");
  expect(fromSetting("") == Filter::Off && fromSetting("nonsense") == Filter::Off && fromSetting("Lottes") == Filter::Off, "anything unknown is Off");
  // a filter that is not ported yet is never chosen, even if the file says so (a settings.ini made by a newer program)
  for(Filter f : {Filter::Geom, Filter::GuestNtsc}) if(!implemented(f)) expect(fromSetting(settingName(f)) == Filter::Off, std::string(settingName(f)) + " is Off while it is not ported");
  // the menu goes round the filters that are there
  Filter f = Filter::Off;
  std::string seen;
  for(int i = 0; i < 6; ++i) { seen += std::string(settingName(f)) + " "; f = next(f); }
  int ported = 0;
  for(int i = 0; i < kFilters; ++i) ported += implemented(static_cast<Filter>(i));
  expect(ported >= 2 && next(Filter::Off) == Filter::Lottes, "the first step from Off is CRT-Lottes: " + seen);
  expect(next(Filter::Lottes, -1) == Filter::Off && next(Filter::Off, -1) != Filter::Off, "and back");
  std::printf("%d checks, %d failures\n", checks, failures);
  if(failures == 0) std::printf("ALL TESTS PASSED\n");
  return failures == 0 ? 0 : 1;
}
