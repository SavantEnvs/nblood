#!/usr/bin/env bash
#
# mayhem/build.sh — build NBlood's fuzz harnesses (sanitized) and the known-answer oracle probe (normal flags).
#
# Runs inside the commit image (mayhem/Dockerfile) as `mayhem` in /mayhem. Env contract comes from the base
# image (CC, CXX, LIB_FUZZING_ENGINE, SANITIZER_FLAGS, DEBUG_FLAGS, STANDALONE_FUZZ_MAIN, SRC).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

: "${SRC:=/mayhem}"
cd "$SRC"
OUT=/mayhem

# Four UBSan checks fire on benign patterns and are relaxed (everything else stays halting): `alignment`
# (packed structs such as ININODE, B_UNBUF32/16 loads in kplib), `shift-base` (kzcalchash shifts a negative
# value), `shift-exponent` (kplib PNG filter mask `1<<filt` with filt up to 255, unused for filt>4) and
# `float-cast-overflow` (sjson_get_int casts an out-of-range double to int).
UBSAN_RELAX="-fno-sanitize=alignment,shift-base,shift-exponent,float-cast-overflow"

# The engine objects live in obj/<variant>/ (gitignored by upstream's `obj/`). Same knobs for both builds:
# LTO=0 so the objects are real ELF (not bitcode), USE_MIMALLOC=0 so ASan sees the real allocator, no GUI/GTK.
MAKE_KNOBS=(RELEASE=1 LTO=0 USE_MIMALLOC=0 HAVE_GTK2=0 USE_LIBVPX=0 STARTUP_WINDOW=0 NOASM=1 CC="$CC" CXX="$CXX")

# Archive every engine + game object into one library. `main` lives in sdlayer.cpp, so it is renamed
# (the harnesses and the probe supply their own entry point).
make_lib() {
  local objdir="$1" lib="$2"
  rm -f "$lib" "$objdir/sdlayer_nomain.o"
  objcopy --redefine-sym main=nblood_sdl_main "$objdir/build/sdlayer.o" "$objdir/sdlayer_nomain.o"
  find "$objdir" -name '*.o' ! -name sdlayer.o ! -name sdlayer_nomain.o | sort > "$objdir/objs.txt"
  ar rcs "$lib" $(cat "$objdir/objs.txt") "$objdir/sdlayer_nomain.o"
}

# Compile flags that the harnesses / probe share with the engine build (struct layouts depend on the defines).
SDL_CFLAGS="$(sdl2-config --cflags)"
INCS=(-Isource/build/include -Isource/blood/src -Isource/mact/include -Isource/audiolib/include
      -Isource/glad/include -Isource/voidwrap/include -Isource/imgui/include)
DEFS=(-funsigned-char -DNOASM -DRENDERTYPESDL=1 -DUSE_OPENGL -DPOLYMER -DNOONE_EXTENSIONS -DHAVE_VORBIS
      -DHAVE_FLAC -DHAVE_XMP -DSDL_TARGET=2 -D_REENTRANT)
SYSLIBS=(-lSDL2 -lrt -latomic -ldl -pthread -lm -lFLAC -lasound)

# 1) Sanitized + SanCov-instrumented engine (the fuzzed code itself).
make -j"$MAYHEM_JOBS" nblood obj=obj/fuzz "${MAKE_KNOBS[@]}" \
  CFLAGS="$SANITIZER_FLAGS $UBSAN_RELAX $DEBUG_FLAGS -fsanitize=fuzzer-no-link" LDFLAGS="$SANITIZER_FLAGS"
# kplib's ZIP name hash (kzcalchash) overflows int32 by design, and fuzz_pk3 reaches it on every archive.
# Build a second kplib.o without the signed-integer-overflow check (make -W forces just that file to
# rebuild) for fuzz_pk3 only; the library and every other target keep the strict check on the image decoders.
cp obj/fuzz/build/kplib.o obj/fuzz/kplib_strict.o
make -j"$MAYHEM_JOBS" nblood obj=obj/fuzz "${MAKE_KNOBS[@]}" -W source/build/src/kplib.cpp \
  CFLAGS="$SANITIZER_FLAGS $UBSAN_RELAX -fno-sanitize=signed-integer-overflow $DEBUG_FLAGS -fsanitize=fuzzer-no-link" LDFLAGS="$SANITIZER_FLAGS"
cp obj/fuzz/build/kplib.o obj/fuzz/kplib_relaxed.o
cp obj/fuzz/kplib_strict.o obj/fuzz/build/kplib.o
rm -f nblood
make_lib obj/fuzz obj/fuzz/libnblood-fuzz.a

# 2) Harnesses: fuzzer binary + standalone reproducer, each linked with lsan_off.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o obj/fuzz/lsan_off.o
$CC  $SANITIZER_FLAGS $DEBUG_FLAGS -x c -c "$STANDALONE_FUZZ_MAIN" -o obj/fuzz/standalone_main.o

for t in fuzz_image fuzz_pk3 fuzz_scriptfile fuzz_inifile fuzz_json; do
  EXTRA_OBJ=()
  [ "$t" = fuzz_pk3 ] && EXTRA_OBJ=(obj/fuzz/kplib_relaxed.o)
  $CXX -std=c++14 -fno-exceptions -fno-rtti $SANITIZER_FLAGS $UBSAN_RELAX $DEBUG_FLAGS -fno-strict-aliasing \
       "${DEFS[@]}" $SDL_CFLAGS "${INCS[@]}" -c "mayhem/$t/$t.cpp" -o "obj/fuzz/$t.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "obj/fuzz/$t.o" "${EXTRA_OBJ[@]}" obj/fuzz/lsan_off.o \
       obj/fuzz/libnblood-fuzz.a "${SYSLIBS[@]}" -o "$OUT/$t"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS obj/fuzz/standalone_main.o "obj/fuzz/$t.o" "${EXTRA_OBJ[@]}" obj/fuzz/lsan_off.o \
       obj/fuzz/libnblood-fuzz.a "${SYSLIBS[@]}" -o "$OUT/$t-standalone"
done

# 3) Known-answer probe for test.sh: the same parsers, built with upstream's NORMAL flags
#    (no sanitizers, no -gdwarf-3) into a separate object dir.
make -j"$MAYHEM_JOBS" nblood obj=obj/kat "${MAKE_KNOBS[@]}" CFLAGS="$COVERAGE_FLAGS" LDFLAGS="$COVERAGE_FLAGS"
rm -f nblood
make_lib obj/kat obj/kat/libnblood-kat.a
$CXX -std=c++14 -fno-exceptions -fno-rtti -O1 $COVERAGE_FLAGS -fno-strict-aliasing \
     "${DEFS[@]}" $SDL_CFLAGS "${INCS[@]}" mayhem/kat/kat.cpp obj/kat/libnblood-kat.a "${SYSLIBS[@]}" \
     -o "$OUT/kat_probe"
