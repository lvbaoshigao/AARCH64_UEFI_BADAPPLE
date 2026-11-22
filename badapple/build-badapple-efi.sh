#!/usr/bin/env bash
# Build BadApple.efi (AArch64 UEFI Bad Apple!! player with an embedded
# "RBA1" stream) in WSL.
#
#   1. Host encoder ba-encode (clang, native) from ba-encode.c + BaCodec.h.
#   2. For each ladder step (best first): ffmpeg decodes the video to W x H
#      8-bit gray at FPS (aspect kept, black padding), ba-encode codes it and
#      stops early if the file passes --budget bytes. The first step that
#      fits is kept; ba-encode has already decoded it again with BaCodec.h and
#      compared every frame.
#      The ladder puts frame rate first (all 30 fps sizes, then 24, 20, 15):
#      the player upscales bilinearly, so a smaller 30 fps picture still looks smooth.
#   3. BadApple.efi: same toolchain as ../anim/build-anim-efi.sh (clang, lld,
#      EDK II GccBase.lds, BaseTools GenFw); the stream is .incbin'd.
#
# Nothing in the checkout or in the badapple-efi clone is written; all output
# goes to --out, which must not exist yet.
#
# Usage: build-badapple-efi.sh --video /mnt/g/cache/badapple.mov --budget BYTES --out /mnt/.../badapple
#                              [--root UEFI] [--ladder "480x360@30 ..."]
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
ROOT=/mnt/g/cache/rothko-upstream-audit/uefi-rothko
VIDEO=
BUDGET=
OUT=
LADDER="480x360@30 448x336@30 416x312@30 384x288@30 352x264@30 320x240@30 288x216@30 256x192@30 400x300@24 480x360@20 480x360@15 320x240@15 256x192@10 160x120@10"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --root) ROOT="$2"; shift 2 ;;
    --video) VIDEO="$2"; shift 2 ;;
    --budget) BUDGET="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --ladder) LADDER="$2"; shift 2 ;;
    *) printf 'Unknown option: %s\n' "$1" >&2; exit 2 ;;
  esac
done
[ -n "$VIDEO" ] && [ -n "$BUDGET" ] && [ -n "$OUT" ] || { printf 'need --video, --budget and --out\n' >&2; exit 2; }
case "$BUDGET" in ''|*[!0-9]*) printf -- '--budget must be a byte count\n' >&2; exit 2 ;; esac
case "$OUT" in /*) ;; *) printf -- '--out must be absolute\n' >&2; exit 2 ;; esac
if [ -e "$OUT" ] || [ -L "$OUT" ]; then printf 'Refusing to overwrite %s\n' "$OUT" >&2; exit 1; fi

BASETOOLS="$ROOT/Mu_Basecore/BaseTools"
LDS="$BASETOOLS/Scripts/GccBase.lds"
GENFW="$BASETOOLS/Source/C/bin/GenFw"
CC=/usr/bin/clang
TABLES="$HERE/../anim/AnimTables.h"
for f in "$CC" "$LDS" "$GENFW" "$VIDEO" "$TABLES" \
         "$HERE/BadApple.c" "$HERE/BaCodec.h" "$HERE/BaBlob.S" "$HERE/ba-encode.c"; do
  [ -e "$f" ] || { printf 'missing: %s\n' "$f" >&2; exit 1; }
done
command -v ld.lld >/dev/null || { printf 'ld.lld not found (sudo apt install lld)\n' >&2; exit 1; }
command -v ffmpeg >/dev/null || { printf 'ffmpeg not found in WSL (sudo apt install ffmpeg)\n' >&2; exit 1; }

mkdir -p "$(dirname "$OUT")"
mkdir "$OUT"
cp "$HERE/BadApple.c" "$HERE/BaCodec.h" "$HERE/BaBlob.S" "$HERE/ba-encode.c" "$TABLES" "$OUT/"
cd "$OUT"

"$CC" --version | head -n 1
"$GENFW" --version 2>&1 | head -n 1 || true
ffmpeg -hide_banner -version | head -n 1
sha256sum "$VIDEO" | tee video.sha256
ffprobe -v error -show_entries stream=codec_type,codec_name,width,height,r_frame_rate,duration \
  -of default=noprint_wrappers=1 "$VIDEO" 2>&1 | tee video-probe.txt || true

# 1. Host encoder.
"$CC" -O2 -Wall -Wno-unused-function -o ba-encode ba-encode.c

# 2. Ladder.
CHOSEN=
for STEP in $LADDER; do
  W="${STEP%%x*}"; REST="${STEP#*x}"; H="${REST%%@*}"; F="${STEP#*@}"
  TAG="${W}x${H}-${F}fps"
  rm -f candidate.bin
  set +e
  ffmpeg -nostdin -hide_banner -v error -i "$VIDEO" -an \
    -vf "fps=${F},scale=${W}:${H}:force_original_aspect_ratio=decrease:flags=area,pad=${W}:${H}:(ow-iw)/2:(oh-ih)/2:color=black,format=gray" \
    -f rawvideo -pix_fmt gray - 2>"ffmpeg-${TAG}.log" \
    | ./ba-encode "$W" "$H" "$F" candidate.bin "$BUDGET" "preview-${TAG}" >"encode-${TAG}.json" 2>"encode-${TAG}.log"
  STATUS=("${PIPESTATUS[@]}")
  set -e
  printf 'LADDER %s ffmpeg=%s encode=%s %s\n' "$TAG" "${STATUS[0]}" "${STATUS[1]}" "$(tail -n 1 "encode-${TAG}.json" 2>/dev/null)"
  if [ "${STATUS[1]}" = 0 ]; then
    if [ "${STATUS[0]}" != 0 ]; then
      printf 'ffmpeg failed for %s:\n' "$TAG" >&2; cat "ffmpeg-${TAG}.log" >&2; exit 1
    fi
    CHOSEN="$TAG"
    break
  fi
  if [ "${STATUS[1]}" != 3 ]; then
    printf 'ba-encode failed for %s (exit %s):\n' "$TAG" "${STATUS[1]}" >&2
    cat "encode-${TAG}.log" "ffmpeg-${TAG}.log" >&2
    exit 1
  fi
done
[ -n "$CHOSEN" ] || { printf 'no ladder step fits in %s bytes\n' "$BUDGET" >&2; exit 1; }
mv candidate.bin badapple.bin
cp "encode-${CHOSEN}.json" encode-result.json
printf 'CHOSEN %s\n' "$CHOSEN"
cat encode-result.json

# 3. EFI. EDK II DEBUG_CLANGDWARF_AARCH64 code-generation flags, without LTO; integer-only code.
CFLAGS=(
  -target aarch64-linux-gnu -c -g -O2
  -ffreestanding -fno-builtin -fshort-wchar -fno-strict-aliasing -fno-common
  -fno-stack-protector -mlittle-endian -fno-short-enums -funsigned-char
  -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fno-unwind-tables
  -fno-pic -fno-pie -ffixed-x18 -mcmodel=small -mgeneral-regs-only
  -Wall -Wno-unused-function -Wno-unused-const-variable
)
"$CC" "${CFLAGS[@]}" -I "$OUT" -o BadApple.o BadApple.c
"$CC" -target aarch64-linux-gnu -c -DBA_DATA_PATH="\"$OUT/badapple.bin\"" -o BaBlob.o BaBlob.S

# EDK II GCC_AARCH64_DLINK_COMMON + CLANGDWARF_AARCH64_DLINK2_FLAGS.
"$CC" -target aarch64-linux-gnu -nostdlib -fuse-ld=lld \
  -Wl,--emit-relocs -Wl,--gc-sections -u efi_main -Wl,-e,efi_main \
  -Wl,-Map,BadApple.map -z common-page-size=0x1000 \
  -Wl,--no-pie,--no-relax -Wl,-z,notext \
  -Wl,--script="$LDS" -Wl,--defsym=PECOFF_HEADER_SIZE=0x228 \
  -o BadApple.dll BadApple.o BaBlob.o
"$GENFW" -e UEFI_APPLICATION -o BadApple.efi BadApple.dll

ls -l badapple.bin BadApple.efi
sha256sum BadApple.c BaCodec.h BaBlob.S ba-encode.c AnimTables.h badapple.bin BadApple.efi | tee SHA256SUMS.txt
printf 'EFI_OK %s\n' "$OUT/BadApple.efi"
