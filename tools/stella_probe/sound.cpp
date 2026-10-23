// stella_sound: runs a ROM in the Stella 7.0 core and records every change of the six TIA sound
// registers (at CPU-instruction precision), the audio stream as a WAV file, and pictures of chosen
// frames. Optional scripted joystick play. Stage C5, DECISIONS.md T-C5-1.
// Frame steps are the same as StellaLIBRETRO::runFrame() (see timing.cpp, T-C4-1); only public,
// side-effect-free values are read between instructions. It only reads the ROM and writes into out_dir.
//
// Usage: stella_sound <rom> <out_dir> <frames> <reset_frame> <seed> [dump_frames_file|-] [reset_every]
//   seed              -1 = no joystick input; otherwise seed of the scripted joystick play
//   dump_frames_file  text file with frame numbers whose picture is saved as frame_NNNNNN_160xH.raw
//   reset_every       also hold Game Reset at reset_frame + k * reset_every (0 = only once), to start new games

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "StellaLIBRETRO.hxx"
#include "OSystem.hxx"
#include "Console.hxx"
#include "System.hxx"
#include "M6502.hxx"
#include "M6532.hxx"
#include "TIA.hxx"
#include "FrameBuffer.hxx"
#include "Event.hxx"

namespace {

constexpr int kWidth = 160;  // TIAConstants::H_PIXEL
constexpr int kResetFrames = 10;
constexpr uInt8 kAudFirst = 0x15;  // AUDC0, AUDC1, AUDF0, AUDF1, AUDV0, AUDV1 = $15-$1A
const char* const kAudName[6] = {"AUDC0", "AUDC1", "AUDF0", "AUDF1", "AUDV0", "AUDV1"};

struct Change { uInt32 line, cycle; int reg; uInt8 value; };

class TimedStella : public StellaLIBRETRO {
  public:
    // Same steps as StellaLIBRETRO::runFrame(); updateVideo() unrolled to single instructions.
    void runFrameTimed(std::vector<Change>& ch, uInt64 frameStart, uInt8 aud[6])
    {
      Console& console = osystem().console();
      M6532& riot = console.system().m6532();
      for(int lcv = 0; lcv <= 127; lcv++)
        riot.poke(lcv | 0x80, getRAM()[lcv]);

      updateInput();

      TIA& tia = console.tia();
      M6502& cpu = console.system().m6502();
      ch.clear();
      while(true) {
        const uInt32 line = tia.scanlines();
        while(line == tia.scanlines()) {
          const bool ok = cpu.execute(1);
          for(int r = 0; r < 6; ++r) {
            const uInt8 v = tia.registerValue(kAudFirst + r);
            if(v != aud[r]) {
              ch.push_back({tia.scanlines(), static_cast<uInt32>(tia.cycles() - frameStart), r, v});
              aud[r] = v;
            }
          }
          if(!ok) break;
        }
        if(tia.scanlines() == 0) break;
      }
      if(tia.newFramePending()) {
        tia.renderToFrameBuffer();
        osystem().frameBuffer().updateInEmulationMode(0);
      }

      updateAudio();

      std::memcpy(getRAM(), riot.getRAM(), 128);
    }
};

TimedStella stella;

uint64_t fnv1a(const uint8_t* data, size_t size)
{
  uint64_t h = 1469598103934665603ULL;
  for(size_t i = 0; i < size; ++i) { h ^= data[i]; h *= 1099511628211ULL; }
  return h;
}

void put32(std::ofstream& o, uInt32 v) { o.write(reinterpret_cast<const char*>(&v), 4); }
void put16(std::ofstream& o, uInt16 v) { o.write(reinterpret_cast<const char*>(&v), 2); }

}  // namespace

// Stella's FSNodeLIBRETRO reads the ROM through this function; same definition as in libretro.cxx.
uInt32 libretro_read_rom(void* data)
{
  std::memcpy(data, stella.getROM(), stella.getROMSize());
  return stella.getROMSize();
}

int main(int argc, char* argv[])
{
  if(argc < 6) {
    std::fprintf(stderr, "usage: stella_sound <rom> <out_dir> <frames> <reset_frame> <seed> [dump_frames_file]\n");
    return 2;
  }
  const std::string romPath = argv[1], outDir = argv[2];
  const int frames = std::stoi(argv[3]), resetFrame = std::stoi(argv[4]);
  const long long seed = std::stoll(argv[5]);
  std::set<int> dump;
  if(argc > 6 && std::string(argv[6]) != "-") { std::ifstream df(argv[6]); int n; while(df >> n) dump.insert(n); }
  const int resetEvery = argc > 7 ? std::stoi(argv[7]) : 0;

  std::ifstream romFile(romPath, std::ios::binary);
  const std::vector<char> rom((std::istreambuf_iterator<char>(romFile)), std::istreambuf_iterator<char>());
  if(rom.empty()) { std::fprintf(stderr, "cannot read ROM %s\n", romPath.c_str()); return 3; }

  stella.setROM(romPath.c_str(), rom.data(), rom.size());
  if(!stella.create(false)) { std::fprintf(stderr, "Stella could not create the console\n"); return 4; }

  Console& console = stella.osystem().console();
  TIA& tia = console.tia();
  const uInt32 rate = static_cast<uInt32>(stella.getAudioRate());

  std::ofstream info(outDir + "/info.txt");
  info << "display_format=" << console.about().DisplayFormat << "\naudio_rate=" << stella.getAudioRate()
       << "\naudio_channels=2\naudio_bits=16\nframes=" << frames << "\nreset_frame=" << resetFrame
       << "\nseed=" << seed << "\nreset_every=" << resetEvery << "\n";
  info.close();

  // WAV header; the two sizes are filled in at the end.
  std::ofstream wav(outDir + "/audio.wav", std::ios::binary);
  wav.write("RIFF", 4); put32(wav, 0); wav.write("WAVEfmt ", 8); put32(wav, 16);
  put16(wav, 1); put16(wav, 2); put32(wav, rate); put32(wav, rate * 4); put16(wav, 4); put16(wav, 16);
  wav.write("data", 4); put32(wav, 0);
  uInt32 dataBytes = 0;

  std::ofstream changes(outDir + "/aud_changes.csv");
  changes << "frame,line,cycle_in_frame,register,value\n";
  std::ofstream csv(outDir + "/frames.csv");
  csv << "frame,input,audc0,audc1,audf0,audf1,audv0,audv1,changes,audio_samples,audio_offset,hash,status_hash,colors\n";

  uInt8 aud[6];
  for(int r = 0; r < 6; ++r) aud[r] = tia.registerValue(kAudFirst + r);

  // Scripted joystick: after Game Reset, hold one direction for 30-119 frames, then pick another.
  // A fixed linear congruential generator makes every run with the same seed identical.
  uint64_t lcg = static_cast<uint64_t>(seed);
  auto next = [&]() { lcg = lcg * 6364136223846793005ULL + 1442695040888963407ULL; return static_cast<uInt32>(lcg >> 33); };
  const Event::Type dirs[4] = {Event::LeftJoystickUp, Event::LeftJoystickDown, Event::LeftJoystickLeft, Event::LeftJoystickRight};
  const char* const dirName = "UDLR";
  int dir = -1, holdLeft = 0;

  std::vector<Change> ch;
  uInt64 frameStart = tia.cycles();
  uInt64 sampleOffset = 0;
  for(int f = 0; f < frames; ++f) {
    const int sinceReset = resetFrame >= 0 && f >= resetFrame
                           ? (resetEvery > 0 ? (f - resetFrame) % resetEvery : f - resetFrame) : -1;
    const bool reset = sinceReset >= 0 && sinceReset < kResetFrames;
    stella.setInputEvent(Event::ConsoleReset, reset ? 1 : 0);
    if(reset) dir = -1;
    if(seed >= 0 && sinceReset >= kResetFrames) {
      if(holdLeft <= 0) { dir = static_cast<int>(next() % 4); holdLeft = 30 + static_cast<int>(next() % 90); }
      --holdLeft;
    }
    for(int d = 0; d < 4; ++d) stella.setInputEvent(dirs[d], d == dir ? 1 : 0);

    stella.runFrameTimed(ch, frameStart, aud);

    for(const Change& c : ch)
      changes << f << ',' << c.line << ',' << c.cycle << ',' << kAudName[c.reg] << ',' << static_cast<int>(c.value) << '\n';

    const uInt32 samples = stella.getAudioSize();
    wav.write(reinterpret_cast<const char*>(stella.getAudioBuffer()), static_cast<std::streamsize>(samples) * 4);
    dataBytes += samples * 4;

    const size_t fbSize = static_cast<size_t>(tia.height()) * kWidth;
    char hash[17];
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(fnv1a(tia.frameBuffer(), fbSize)));
    csv << f << ',' << (dir < 0 ? '-' : dirName[dir]);
    for(int r = 0; r < 6; ++r) csv << ',' << static_cast<int>(aud[r]);
    // Status bar (score, lives): rows 180-199 of the picture (C3: drawn rows end at 199).
    char statusHash[17] = "-";
    if(tia.height() >= 200)
      std::snprintf(statusHash, sizeof statusHash, "%016llx", static_cast<unsigned long long>(
                    fnv1a(tia.frameBuffer() + 180 * kWidth, 20 * kWidth)));
    bool used[256] = {};
    for(size_t i = 0; i < fbSize; ++i) used[tia.frameBuffer()[i]] = true;
    std::string colors;
    for(int c = 0; c < 256; ++c)
      if(used[c]) { char b[4]; std::snprintf(b, sizeof b, "%02x", c); colors += (colors.empty() ? "" : ":"); colors += b; }
    csv << ',' << ch.size() << ',' << samples << ',' << sampleOffset << ',' << hash << ',' << statusHash << ',' << colors << '\n';
    sampleOffset += samples;

    if(dump.count(f)) {
      char name[64];
      std::snprintf(name, sizeof name, "/frame_%06d_%dx%u.raw", f, kWidth, tia.height());
      std::ofstream raw(outDir + name, std::ios::binary);
      raw.write(reinterpret_cast<const char*>(tia.frameBuffer()), static_cast<std::streamsize>(fbSize));
    }
    frameStart = tia.cycles();
  }

  wav.seekp(4); put32(wav, 36 + dataBytes);
  wav.seekp(40); put32(wav, dataBytes);
  wav.close();
  csv.close();
  changes.close();
  std::printf("done: %d frames, %llu audio samples per channel\n", frames, static_cast<unsigned long long>(sampleOffset));
  stella.destroy();
  return 0;
}
