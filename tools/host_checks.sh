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
  "$EXE" --push-keys --timed 12 "$P/keys.txt" "$ROM" --mute --settings "$P/s9.ini" --record "$P/rec1.inputs" --fingerprints "$P/A.csv"; check "$prog push-keys recording" $? 0
  for v in 1 2 4 8; do has "$prog key direction $v seen" "$P/s9.ini.audio.txt" "joystick_ram_values_seen=.* $v "; done
  "$EXE" --replay "$P/rec1.inputs" --fingerprints "$P/B.csv" --timed 8 "$P/rw.txt" "$ROM" --mute --settings "$P/s10.ini"; check "$prog replay in window" $? 0
  "$EXE" --headless "$P/c.png" "$ROM" --replay "$P/rec1.inputs" --fingerprints "$P/C.csv" --settings "$P/s11.ini"; check "$prog replay headless" $? 0
  python -I -c "
import csv,sys
def h(p): return [r['hash'] for r in csv.DictReader(open(p))]
a,b,c=h(sys.argv[1]),h(sys.argv[2]),h(sys.argv[3])
nab=min(len(a),len(b)); nac=min(len(a),len(c))
dab=sum(1 for i in range(nab) if a[i]!=b[i]); dac=sum(1 for i in range(nac) if a[i]!=c[i])
print('replay compare: window %d frames %d differences; headless %d frames %d differences'%(nab,dab,nac,dac))
sys.exit(0 if (dab==0 and dac==0 and nab>=400 and nac>=700) else 1)" "$P/A.csv" "$P/B.csv" "$P/C.csv"; check "$prog replay equals recording" $? 0
done
for t in test_controls test_inputlog test_layout test_rom_finder test_trace test_windowstate; do
  case $t in
    test_rom_finder) mkdir -p "$W/t_$t"; build/host/$t.exe "$W/t_$t" "$ROM" >"$W/$t.log" 2>&1 ;;
    test_inputlog|test_windowstate) mkdir -p "$W/t_$t"; build/host/$t.exe "$W/t_$t" >"$W/$t.log" 2>&1 ;;
    *) build/host/$t.exe >"$W/$t.log" 2>&1 ;;
  esac
  check "$t" $? 0
done
echo "ALL DONE fail=$fail"
exit $fail
