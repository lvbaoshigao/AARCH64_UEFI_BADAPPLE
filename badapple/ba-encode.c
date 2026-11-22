/*
 * ba-encode: host encoder for the "RBA1" stream played by BadApple.efi.
 *
 *   ffmpeg ... -f rawvideo -pix_fmt gray - | ba-encode W H FPS OUT.bin MAXBYTES [PREVIEW_PREFIX]
 *
 * Reads 8-bit gray frames of W x H from stdin, thresholds at 128 (>= 128 is
 * white), codes them with the model in BaCodec.h and writes header + stream
 * to OUT.bin. MAXBYTES (0 = no limit) is the size budget for the whole file:
 * when the output passes it, encoding stops with exit code 3 so the caller
 * can try a smaller ladder step.
 *
 * After encoding, the file is read back and decoded with BaCodec.h (the same
 * code the player runs) and every frame is compared with the input; any
 * difference is exit code 4. With PREVIEW_PREFIX, decoded frames at 1/4,
 * 1/2 and 3/4 are written as PREFIX-NNNNN.pgm.
 *
 * The last stdout line is a JSON summary.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BaCodec.h"

typedef struct {
  ba_u64    Low;
  ba_u32    Range;
  ba_u8     Cache;
  ba_u64    CacheSize;
  ba_u8     *Buf;
  size_t    Len;
  size_t    Cap;
} ENC;

static void
Die (
  const char  *Msg
  )
{
  fprintf (stderr, "ba-encode: %s\n", Msg);
  exit (1);
}

static void *
Grow (
  void    *P,
  size_t  Bytes
  )
{
  void  *Q = realloc (P, Bytes);

  if (Q == NULL) {
    Die ("out of memory");
  }

  return Q;
}

static void
Put (
  ENC    *E,
  ba_u8  B
  )
{
  if (E->Len == E->Cap) {
    E->Cap = E->Cap ? E->Cap * 2 : (size_t)1 << 20;
    E->Buf = Grow (E->Buf, E->Cap);
  }

  E->Buf[E->Len++] = B;
}

/* LZMA SDK RangeEnc_ShiftLow. */
static void
ShiftLow (
  ENC  *E
  )
{
  if (((ba_u32)E->Low < 0xFF000000u) || ((E->Low >> 32) != 0)) {
    ba_u8  Temp = E->Cache;

    do {
      Put (E, (ba_u8)(Temp + (ba_u8)(E->Low >> 32)));
      Temp = 0xFF;
    } while (--E->CacheSize != 0);

    E->Cache = (ba_u8)((ba_u32)E->Low >> 24);
  }

  E->CacheSize++;
  E->Low = (ba_u64)((ba_u32)E->Low & 0x00FFFFFFu) << 8;
}

static void
EncBit (
  ENC     *E,
  ba_u16  *Prob,
  ba_u32  Bit
  )
{
  ba_u32  P     = *Prob;
  ba_u32  Bound = (E->Range >> BA_PROB_BITS) * P;

  if (Bit == 0) {
    E->Range = Bound;
    *Prob    = (ba_u16)(P + ((BA_PROB_ONE - P) >> BA_MOVE_BITS));
  } else {
    E->Low   += Bound;
    E->Range -= Bound;
    *Prob     = (ba_u16)(P - (P >> BA_MOVE_BITS));
  }

  while (E->Range < BA_TOP) {
    E->Range <<= 8;
    ShiftLow (E);
  }
}

static unsigned long
ParseUnsigned (
  const char     *S,
  unsigned long  Max,
  const char     *What
  )
{
  char           *End;
  unsigned long  V = strtoul (S, &End, 10);

  if ((*S == '\0') || (*End != '\0') || (V > Max)) {
    fprintf (stderr, "ba-encode: bad %s '%s'\n", What, S);
    exit (2);
  }

  return V;
}

static void
WritePreview (
  const char   *Prefix,
  ba_u32       Index,
  const ba_u8  *Plane,
  ba_s64       Stride,
  ba_u32       W,
  ba_u32       H
  )
{
  char    Name[4096];
  FILE    *F;
  ba_u32  X;
  ba_u32  Y;

  snprintf (Name, sizeof (Name), "%s-%05u.pgm", Prefix, Index);
  F = fopen (Name, "wb");
  if (F == NULL) {
    Die ("cannot write preview");
  }

  fprintf (F, "P5\n%u %u\n255\n", W, H);
  for (Y = 0; Y < H; Y++) {
    const ba_u8  *Row = BaPlaneRow ((ba_u8 *)Plane, Stride, Y);

    for (X = 0; X < W; X++) {
      fputc (Row[X] ? 255 : 0, F);
    }
  }

  fclose (F);
}

int
main (
  int   argc,
  char  **argv
  )
{
  ba_u32         W, H, Fps;
  unsigned long  MaxBytes;
  const char     *OutPath;
  const char     *Preview;
  size_t         FrameBytes, PackedBytes, Got;
  ba_u8          *Gray, *Bits, *Prev, *Cur, *Tmp, *Packed = NULL;
  size_t         PackedCap = 0;
  ba_u32         Frames = 0, SameFrames = 0;
  ba_u64         SameRows = 0, PlaneBytes;
  ba_s64         S;
  BA_MODEL       *Model;
  BA_INFO        Info;
  ENC            E;
  ba_u8          Header[BA_HEADER_BYTES];
  FILE           *Out;
  ba_u8          *File;
  size_t         FileBytes;
  BA_DECODER     *D;
  ba_u32         Mismatch = 0, T, X, Y;

  if ((argc != 6) && (argc != 7)) {
    fprintf (stderr, "usage: ba-encode W H FPS OUT.bin MAXBYTES [PREVIEW_PREFIX]\n");
    return 2;
  }

  W        = (ba_u32)ParseUnsigned (argv[1], BA_MAX_DIM, "width");
  H        = (ba_u32)ParseUnsigned (argv[2], BA_MAX_DIM, "height");
  Fps      = (ba_u32)ParseUnsigned (argv[3], BA_MAX_FPS, "fps");
  OutPath  = argv[4];
  MaxBytes = ParseUnsigned (argv[5], 0xFFFFFFFFul, "max bytes");
  Preview  = (argc == 7) ? argv[6] : NULL;
  if ((W == 0) || (H == 0) || (Fps == 0)) {
    Die ("width, height and fps must be non-zero");
  }

  memset (&Info, 0, sizeof (Info));
  Info.Width  = W;
  Info.Height = H;
  Info.Fps    = Fps;

  FrameBytes  = (size_t)W * H;
  PackedBytes = (FrameBytes + 7) / 8;
  PlaneBytes  = BaPlaneBytes (&Info);
  S           = (ba_s64)W + 2 * BA_PAD;
  Gray        = Grow (NULL, FrameBytes);
  Bits        = Grow (NULL, FrameBytes);
  Prev        = calloc (1, PlaneBytes);
  Cur         = calloc (1, PlaneBytes);
  Model       = calloc (1, sizeof (*Model));
  if ((Prev == NULL) || (Cur == NULL) || (Model == NULL)) {
    Die ("out of memory");
  }

  BaModelInit (Model);
  memset (&E, 0, sizeof (E));
  E.Range     = 0xFFFFFFFFu;
  E.CacheSize = 1;

  for ( ; ;) {
    int     Same = 1;
    ba_u32  RowSame;

    Got = fread (Gray, 1, FrameBytes, stdin);
    if (Got == 0) {
      break;
    }

    if (Got != FrameBytes) {
      Die ("input ends inside a frame (wrong W x H?)");
    }

    if (Frames >= BA_MAX_FRAMES) {
      Die ("too many frames");
    }

    for (size_t I = 0; I < FrameBytes; I++) {
      Bits[I] = Gray[I] >= 128;
    }

    if ((size_t)(Frames + 1) * PackedBytes > PackedCap) {
      PackedCap = PackedCap ? PackedCap * 2 : PackedBytes * 1024;
      Packed    = Grow (Packed, PackedCap);
    }

    memset (Packed + (size_t)Frames * PackedBytes, 0, PackedBytes);
    for (size_t I = 0; I < FrameBytes; I++) {
      Packed[(size_t)Frames * PackedBytes + I / 8] |= (ba_u8)(Bits[I] << (I % 8));
    }

    for (Y = 0; Y < H && Same; Y++) {
      Same = memcmp (Bits + (size_t)Y * W, BaPlaneRow (Prev, S, Y), W) == 0;
    }

    EncBit (&E, &Model->Same, (ba_u32)Same);
    if (Same) {
      SameFrames++;
    } else {
      RowSame = 0;
      for (Y = 0; Y < H; Y++) {
        ba_u8        *C   = BaPlaneRow (Cur, S, Y);
        const ba_u8  *P   = BaPlaneRow (Prev, S, Y);
        const ba_u8  *Src = Bits + (size_t)Y * W;
        ba_u32       Rs   = memcmp (Src, P, W) == 0;

        EncBit (&E, &Model->Row[RowSame], Rs);
        RowSame = Rs;
        if (Rs) {
          memcpy (C, P, W);
          SameRows++;
          continue;
        }

        for (X = 0; X < W; X++) {
          ba_u32  B = Src[X];

          EncBit (&E, &Model->Pix[BaContext (C + X, P + X, S)], B);
          C[X] = (ba_u8)B;
        }
      }

      Tmp  = Prev;
      Prev = Cur;
      Cur  = Tmp;
    }

    Frames++;
    if ((MaxBytes != 0) && (E.Len + BA_HEADER_BYTES > MaxBytes)) {
      fprintf (stderr, "ba-encode: %zu bytes after %u frames, over the %lu byte budget\n", E.Len, Frames, MaxBytes);
      printf ("{\"fits\": false, \"width\": %u, \"height\": %u, \"fps\": %u, \"frames_coded\": %u, \"bytes\": %zu, \"budget\": %lu}\n",
              W, H, Fps, Frames, E.Len + BA_HEADER_BYTES, MaxBytes);
      return 3;
    }
  }

  if (Frames == 0) {
    Die ("no frames on stdin");
  }

  for (int I = 0; I < 5; I++) {
    ShiftLow (&E);
  }

  if ((MaxBytes != 0) && (E.Len + BA_HEADER_BYTES > MaxBytes)) {
    printf ("{\"fits\": false, \"width\": %u, \"height\": %u, \"fps\": %u, \"frames_coded\": %u, \"bytes\": %zu, \"budget\": %lu}\n",
            W, H, Fps, Frames, E.Len + BA_HEADER_BYTES, MaxBytes);
    return 3;
  }

  if (E.Len > 0xFFFFFFFFu) {
    Die ("stream too large");
  }

  Info.Frames      = Frames;
  Info.StreamBytes = (ba_u32)E.Len;
  Info.StreamCrc   = BaCrc32 (E.Buf, E.Len);
  BaWriteHeader (Header, &Info);
  Out = fopen (OutPath, "wb");
  if ((Out == NULL) ||
      (fwrite (Header, 1, sizeof (Header), Out) != sizeof (Header)) ||
      (fwrite (E.Buf, 1, E.Len, Out) != E.Len) ||
      (fclose (Out) != 0))
  {
    Die ("cannot write output");
  }

  /* Read the file back and decode it with the player's code. */
  Out = fopen (OutPath, "rb");
  if (Out == NULL) {
    Die ("cannot reopen output");
  }

  FileBytes = sizeof (Header) + E.Len;
  File      = Grow (NULL, FileBytes + 1);
  if (fread (File, 1, FileBytes + 1, Out) != FileBytes) {
    Die ("output size changed on readback");
  }

  fclose (Out);

  D = calloc (1, sizeof (*D));
  if (D == NULL) {
    Die ("out of memory");
  }

  if (BaParseHeader (File, FileBytes, &D->Info) != BA_OK) {
    Die ("readback: header rejected");
  }

  if (BaCheckStream (File, &D->Info) != BA_OK) {
    Die ("readback: stream CRC mismatch");
  }

  D->Plane[0] = calloc (1, PlaneBytes);
  D->Plane[1] = calloc (1, PlaneBytes);
  if ((D->Plane[0] == NULL) || (D->Plane[1] == NULL)) {
    Die ("out of memory");
  }

  BaDecoderReset (D, File + BA_HEADER_BYTES);
  for (T = 0; T < Frames; T++) {
    const ba_u8  *Ref = Packed + (size_t)T * PackedBytes;
    int          Bad  = 0;

    BaDecodeFrame (D);
    for (Y = 0; Y < H && !Bad; Y++) {
      const ba_u8  *Row = BaPlaneRow (D->Plane[D->Latest], D->Stride, Y);

      for (X = 0; X < W; X++) {
        size_t  I = (size_t)Y * W + X;

        if (Row[X] != ((Ref[I / 8] >> (I % 8)) & 1)) {
          Bad = 1;
          break;
        }
      }
    }

    Mismatch += (ba_u32)Bad;
    if ((Preview != NULL) && ((T == Frames / 4) || (T == Frames / 2) || (T == (Frames / 4) * 3))) {
      WritePreview (Preview, T, D->Plane[D->Latest], D->Stride, W, H);
    }
  }

  if (D->Rc.Next > D->Rc.End) {
    Die ("decoder read past the stream");
  }

  printf ("{\"fits\": true, \"width\": %u, \"height\": %u, \"fps\": %u, \"frames\": %u, \"seconds\": %.2f, "
          "\"bytes\": %zu, \"budget\": %lu, \"bytes_per_frame\": %.1f, \"same_frames\": %u, \"same_rows\": %llu, "
          "\"stream_crc32\": \"%08x\", \"verified_frames\": %u, \"mismatched_frames\": %u}\n",
          W, H, Fps, Frames, (double)Frames / Fps, FileBytes, MaxBytes, (double)FileBytes / Frames,
          SameFrames, (unsigned long long)SameRows, Info.StreamCrc, Frames - Mismatch, Mismatch);
  return Mismatch ? 4 : 0;
}
