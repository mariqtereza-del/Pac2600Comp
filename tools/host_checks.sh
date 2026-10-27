#!/bin/bash
# The host program's own checks (Stage G4.7, kept in tools by H3): run against build/host, both programs. Used by the full test command (test_full.ps1 in this folder).
# bash tools/host_checks.sh <scratch folder>   - windows open briefly and the sound device is used (muted). Scratch files only; the ROM is only passed by path.
cd "$(dirname "$0")/.." || exit 9
ROM="$PWD/rom/pacman.a26"
W="$1"            # scratch folder (new, empty)
mkdir -p "$W"
fail=0
check() { # name, exit code, wanted
  if [ "$2" = "$3" ]; then echo "PASS  $1 (exit $2)"; else echo "FAIL  $1 (exit $2, wanted $3)"; fail=1; fi
}
has() { # name, file, text
  if grep -q "$3" "$2" 2>/dev/null; then echo "PASS  $1 ($3)"; else echo "FAIL  $1 (no '$3' in $2)"; fail=1; fi
}
for prog in pacman2600 pacman2600_ref; do
  EXE="build/host/$prog.exe"; P="$W/$prog"; mkdir -p "$P"
  echo "=== $prog"
  "$EXE" --self-check "$P/self.txt" --settings "$P/s0.ini"; check "$prog self-check" $? 0; has "$prog self-check result" "$P/self.txt" "result=OK"
  "$EXE" --find-rom "$P/find.txt" "$ROM" --settings "$P/s1.ini" --roms-dir "$P/none"; check "$prog find-rom" $? 0
  "$EXE" --headless "$P/h.png" "$ROM" --frames 600 --fingerprints "$P/fp.csv" --settings "$P/s2.ini"; check "$prog headless 600 frames" $? 0
  "$EXE" --layout-check "$P/lay.png" "$ROM" --window 1280x960 --reset-at 60 --settings "$P/s3.ini"; check "$prog layout-check" $? 0; has "$prog layout result" "$P/lay.png.txt" "result=OK"
  "$EXE" --layout-check "$P/lay2.png" "$ROM" --window 960x720 --mode fill --settings "$P/s3b.ini"; check "$prog layout-check fill" $? 0
  "$EXE" --help-check "$P/help.png" "$ROM" --settings "$P/s4.ini"; check "$prog help-check" $? 0
  "$EXE" --fullscreen-check "$P/fs.txt" "$ROM" --reset-at 60 --settings "$P/s5.ini"; check "$prog fullscreen-check" $? 0; has "$prog fullscreen result" "$P/fs.txt" "result=OK"
  "$EXE" --audio-check 20 "$P/audio.txt" "$ROM" --mute --reset-at 120 --settings "$P/s6.ini"; check "$prog audio-check 20 s" $? 0; has "$prog audio underruns" "$P/audio.txt" "underruns=0"
  "$EXE" --pad-check "$P/pad.txt" "$ROM" --settings "$P/s7.ini"; check "$prog pad-check" $? 0; has "$prog pad result" "$P/pad.txt" "result=OK"
  "$EXE" --timed 10 "$P/timed.txt" "$ROM" --mute --reset-at 60 --settings "$P/s8.ini"; check "$prog timed 10 s" $? 0
  "$EXE" --state-report "$P/state.txt" --settings "$P/s5.ini"; check "$prog state-report" $? 0
  # Stage I5: the Escape menu. No window: the menu and the game played together (34 scenarios); real window: the menu worked through the event queue.
  "$EXE" --menu-check "$P/menu.txt" "$ROM" --settings "$P/s12.ini"; check "$prog menu-check" $? 0; has "$prog menu-check result" "$P/menu.txt" "scenarios_passed=34"
  "$EXE" --menu-shot "$P/menu.png" "$ROM" --window 1280x960 --settings "$P/s13.ini"; check "$prog menu-shot" $? 0; has "$prog menu-shot result" "$P/menu.png.txt" "result=OK"
  "$EXE" --push-menu --timed 14 "$P/menuwin.txt" "$ROM" --mute --settings "$P/s14.ini"; check "$prog push-menu" $? 0
  # 0, or 1 when the game thread was already in the middle of a frame at the moment Escape was pressed: that frame is counted (T-I6-1, T-I9-4); never more
  has "$prog menu paused the game" "$P/s14.ini.audio.txt" "menu_frames_run_while_open=[01]$"
  has "$prog menu opened four times" "$P/s14.ini.audio.txt" "menu_opens=4"
  has "$prog menu chose game 3" "$P/s14.ini.audio.txt" "final_game_number_byte=10"
  has "$prog menu set the level" "$P/s14.ini.audio.txt" "final_level=4"
  has "$prog menu set left difficulty A" "$P/s14.ini.audio.txt" "final_game_flags=20"
  has "$prog menu Quit closed the program" "$P/s14.ini.audio.txt" "seconds=8"
  has "$prog graphics page remembered the background" "$P/s14.ini" "background=mirror"
  # Stage I9.5: the frame rate and the counter are rows of Game options (75 in the program with smooth motion, 30 in the reference program, which has only 30 and 60)
  want=75; [ "$prog" = pacman2600_ref ] && want=30
  has "$prog menu remembered the frame rate" "$P/s14.ini" "frame_rate=$want"
  has "$prog menu remembered the frame counter" "$P/s14.ini" "frame_counter=on"
  # Stage I10: the HDR rows of Game options (HDR turned off, the highlights taken from 50 to 60)
  has "$prog menu remembered HDR off" "$P/s14.ini" "hdr=off"
  has "$prog menu remembered the highlights" "$P/s14.ini" "hdr_highlights=60"
  # Stage I7: every background in the real window at three window shapes (tall, wide, square); the picture is never touched and the space is filled
  for size in 1280x1024 1920x1080 1000x1000; do
    "$EXE" --backdrop-check "$P/bd_$size" "$ROM" --window $size --settings "$P/s15.ini"; check "$prog backdrop-check $size" $? 0; has "$prog backdrop result $size" "$P/bd_$size.txt" "result=OK"
  done
  # Stage I11.2: the CRT-Lottes filter in the real window (standard and, where Windows is in HDR mode, HDR): not black, not the plain picture
  if [ "$prog" = pacman2600 ]; then
    for filter in lottes geom; do
      "$EXE" --crt-shot "$P/crt_$filter" "$ROM" --crt $filter --window 1280x960 --settings "$P/s23_$filter.ini"; check "$prog crt-shot $filter" $? 0; has "$prog crt-shot $filter result" "$P/crt_$filter.txt" "^result=OK"
    done
  fi
  # Stage I11.1: our own shaders on SDL's Direct3D 11 renderer (standard and, where Windows is in HDR mode, HDR): identical to the plain picture where nothing changes
  if [ "$prog" = pacman2600 ]; then
    "$EXE" --filter-check "$P/flt" "$ROM" --window 1280x960 --settings "$P/s22.ini"; check "$prog filter-check" $? 0; has "$prog filter-check result" "$P/flt.txt" "^result=OK"
  fi
  # Stage I10.1: HDR. What the screen says is written down; the picture through the HDR renderer equals the standard one in every flat area (or the check says it
  # was SKIPPED because Windows is not in HDR mode on this computer); the game is the same with HDR on and off. Only the program with the rebuilt game draws in HDR.
  "$EXE" --hdr-report "$P/hdr_report.txt"; check "$prog hdr-report" $? 0; has "$prog hdr-report result" "$P/hdr_report.txt" "result=OK"
  if [ "$prog" = pacman2600 ]; then
    "$EXE" --hdr-check "$P/hdrcmp" "$ROM" --window 1280x960 --settings "$P/s19.ini"; check "$prog hdr-check" $? 0
    if grep -q "^result=OK" "$P/hdrcmp.txt"; then echo "PASS  $prog HDR picture equals the standard one in flat areas"; elif grep -q "SKIPPED" "$P/hdrcmp.txt"; then echo "PASS  $prog HDR picture check skipped (Windows is not in HDR mode here)"; else echo "FAIL  $prog HDR picture check"; fail=1; fi
  fi
  # Stage I9.4: the frame rate of the picture. Only the program with the rebuilt game has smooth motion; the reference program keeps to 60.
  if [ "$prog" = pacman2600 ]; then
    for size in 1280x960 1920x1080 1000x1000; do   # a wide window, a tall one and a square one: the moving objects stay inside the picture
      "$EXE" --smooth-shot "$P/smooth_$size" "$ROM" --window $size --settings "$P/s16.ini"; check "$prog smooth-shot $size" $? 0; has "$prog smooth-shot result $size" "$P/smooth_$size.txt" "result=OK"
    done
    for fps in 30 60 144 unlocked; do
      "$EXE" --fps $fps --timed 6 "$P/fps_$fps.txt" "$ROM" --mute --reset-at 60 --settings "$P/s17_$fps.ini"; check "$prog frame rate $fps" $? 0
      has "$prog frame rate $fps was used" "$P/s17_$fps.ini.audio.txt" "frame_rate=$fps"
    done
    python -I -c "
import sys
def get(p,k):
    for line in open(p):
        if line.startswith(k+'='): return float(line.split('=')[1])
r={}
for fps in ('30','60','144','unlocked'):
    p=sys.argv[1]+'/s17_'+fps+'.ini.audio.txt'
    r[fps]=(get(p,'pictures_per_second'),get(p,'presents_per_second'))
print('pictures a second:',{k:round(v[0],1) for k,v in r.items()},'presents a second:',{k:round(v[1],1) for k,v in r.items()})
ok = 27<=r['30'][0]<=33 and 56<=r['60'][0]<=62 and r['144'][0]>=0.85*min(144,r['144'][1]) and r['unlocked'][0]>=0.85*r['unlocked'][1]
sys.exit(0 if ok else 1)" "$P"; check "$prog frame rates give the pictures a second asked for" $? 0
  fi
  "$EXE" --push-keys --timed 12 "$P/keys.txt" "$ROM" --mute --settings "$P/s9.ini" --record "$P/rec1.inputs" --fingerprints "$P/A.csv"; check "$prog push-keys recording" $? 0
  for v in 1 2 4 8; do has "$prog key direction $v seen" "$P/s9.ini.audio.txt" "joystick_ram_values_seen=.* $v "; done
  "$EXE" --replay "$P/rec1.inputs" --fingerprints "$P/B.csv" --timed 8 "$P/rw.txt" "$ROM" --mute --settings "$P/s10.ini"; check "$prog replay in window" $? 0
  "$EXE" --headless "$P/c.png" "$ROM" --replay "$P/rec1.inputs" --fingerprints "$P/C.csv" --settings "$P/s11.ini"; check "$prog replay headless" $? 0
  # Stage I9.6: the game does not depend on the frame rate. The same recording played in the window at 60 and at 144 (smooth motion on) must give the same
  # trace, frame for frame: input, switches, picture fingerprint, sound fingerprint and all 128 bytes of memory (only the program with smooth motion).
  if [ "$prog" = pacman2600 ]; then
    for fps in 60 144 unlocked; do
      "$EXE" --replay "$P/rec1.inputs" --fps $fps --trace "$P/trace_$fps.txt" --timed 8 "$P/rt_$fps.txt" "$ROM" --mute --settings "$P/s18_$fps.ini"; check "$prog replay at frame rate $fps" $? 0
    done
    # Stage I10.3: the highlights at three slider positions: only the moving objects change, each by exactly the scale, nothing above what the screen can show
    "$EXE" --hdr-highlights "$P/hdrhl" "$ROM" --window 1280x960 --settings "$P/s21.ini"; check "$prog hdr-highlights" $? 0
    if grep -q "^result=OK" "$P/hdrhl.txt"; then echo "PASS  $prog HDR highlights are exactly the scale and only on the moving objects"; elif grep -q "SKIPPED" "$P/hdrhl.txt"; then echo "PASS  $prog HDR highlights check skipped (Windows is not in HDR mode here)"; else echo "FAIL  $prog HDR highlights"; fail=1; fi
    for mode in on off; do   # Stage I10.1: HDR on and off give the same game
      "$EXE" --hdr $mode --replay "$P/rec1.inputs" --trace "$P/trace_hdr_$mode.txt" --timed 8 "$P/rh_$mode.txt" "$ROM" --mute --settings "$P/s20_$mode.ini"; check "$prog replay with HDR $mode" $? 0
    done
    python -I -c "
import sys
def lines(p):
    return [l for l in open(p) if l and not l.startswith('#') and not l.startswith('PacMan2600') and not l.startswith('frame') and not l.startswith('rom') ]
a,b=lines(sys.argv[1]),lines(sys.argv[2])
n=min(len(a),len(b))
d=sum(1 for i in range(n) if a[i]!=b[i])
print('trace with HDR on / off: %d frames compared, %d differences'%(n,d))
sys.exit(0 if (n>=400 and d==0) else 1)" "$P/trace_hdr_on.txt" "$P/trace_hdr_off.txt"; check "$prog game identical with HDR on and off" $? 0
    python -I -c "
import sys
def lines(p):
    return [l for l in open(p) if l and not l.startswith('#') and not l.startswith('PacMan2600') and not l.startswith('frame') and not l.startswith('rom') ]
a,b,c=lines(sys.argv[1]),lines(sys.argv[2]),lines(sys.argv[3])
n=min(len(a),len(b),len(c))
dab=sum(1 for i in range(n) if a[i]!=b[i]); dac=sum(1 for i in range(n) if a[i]!=c[i])
print('trace at 60 / 144 / unlocked: %d frames compared, %d differences at 144, %d at unlocked'%(n,dab,dac))
sys.exit(0 if (n>=400 and dab==0 and dac==0) else 1)" "$P/trace_60.txt" "$P/trace_144.txt" "$P/trace_unlocked.txt"; check "$prog game identical at frame rates 60, 144 and unlocked" $? 0
  fi
  python -I -c "
import csv,sys
def h(p): return [r['hash'] for r in csv.DictReader(open(p))]
a,b,c=h(sys.argv[1]),h(sys.argv[2]),h(sys.argv[3])
nab=min(len(a),len(b)); nac=min(len(a),len(c))
dab=sum(1 for i in range(nab) if a[i]!=b[i]); dac=sum(1 for i in range(nac) if a[i]!=c[i])
print('replay compare: window %d frames %d differences; headless %d frames %d differences'%(nab,dab,nac,dac))
sys.exit(0 if (dab==0 and dac==0 and nab>=400 and nac>=700) else 1)" "$P/A.csv" "$P/B.csv" "$P/C.csv"; check "$prog replay equals recording" $? 0
done
for t in test_controls test_inputlog test_layout test_menu test_rom_finder test_trace test_windowstate; do
  case $t in
    test_rom_finder) mkdir -p "$W/t_$t"; build/host/$t.exe "$W/t_$t" "$ROM" >"$W/$t.log" 2>&1 ;;
    test_inputlog|test_windowstate) mkdir -p "$W/t_$t"; build/host/$t.exe "$W/t_$t" >"$W/$t.log" 2>&1 ;;
    *) build/host/$t.exe >"$W/$t.log" 2>&1 ;;
  esac
  check "$t" $? 0
done
echo "ALL DONE fail=$fail"
exit $fail
