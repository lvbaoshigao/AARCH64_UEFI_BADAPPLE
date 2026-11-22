/** @file
  BaCodec: the "RBA1" 1-bit video stream played by BadApple.efi.

  Shared by the player (freestanding AArch64 UEFI) and the host encoder
  ba-encode.c, which decodes its own output with this file to verify it.
  No includes and no library calls, so both sides use exactly this code.

  Stream: a 32-byte header, then one LZMA-style adaptive binary range-coded
  stream for all frames. Per frame:

    bit  "same as the previous frame"      (one probability)
    if not same, per row:
      bit  "row same as in the previous frame" (probability by the previous
                                                row's flag)
      if not same, per pixel, left to right:
        bit  pixel (1 = white), probability by a 12-bit context of already
             decoded neighbours in this frame and the co-located 3x3 cross
             in the previous frame (see BaContext).

  Planes are (Width + 4) x (Height + 4) bytes of 0/1 with a zero border of
  BA_PAD on every side, so the context needs no bounds checks. Frame 0 is
  coded against an all-black previous frame.

  Header (little endian):
     0  "RBA1"
     4  u16 version (1)      6  u16 header bytes (32)
     8  u16 width           10  u16 height
    12  u16 fps             14  u16 flags (0)
    16  u32 frames          20  u32 stream bytes
    24  u32 CRC-32 of the stream
    28  u32 CRC-32 of header bytes 0..27

  Reference: the frame format idea follows rinsuki/badapple-efi (MIT); the
  coder here is new.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef BA_CODEC_H_
#define BA_CODEC_H_

typedef unsigned char       ba_u8;
typedef unsigned short      ba_u16;
typedef unsigned int        ba_u32;
typedef unsigned long long  ba_u64;
typedef long long           ba_s64;

#define BA_HEADER_BYTES  32
#define BA_VERSION       1
#define BA_MAX_DIM       1024
#define BA_MAX_FPS       60
#define BA_MAX_FRAMES    200000
#define BA_PAD           2
#define BA_CTX_COUNT     4096
#define BA_PROB_BITS     11
#define BA_PROB_ONE      (1u << BA_PROB_BITS)
#define BA_PROB_INIT     (BA_PROB_ONE / 2)
#define BA_MOVE_BITS     5
#define BA_TOP           (1u << 24)

#define BA_OK              0
#define BA_ERR_SHORT      (-1)
#define BA_ERR_MAGIC      (-2)
#define BA_ERR_VERSION    (-3)
#define BA_ERR_GEOMETRY   (-4)
#define BA_ERR_SIZE       (-5)
#define BA_ERR_HEADER_CRC (-6)
#define BA_ERR_STREAM_CRC (-7)

typedef struct {
  ba_u32    Width;
  ba_u32    Height;
  ba_u32    Fps;
  ba_u32    Frames;
  ba_u32    StreamBytes;
  ba_u32    StreamCrc;
} BA_INFO;

typedef struct {
  ba_u16    Pix[BA_CTX_COUNT];
  ba_u16    Row[2];
  ba_u16    Same;
} BA_MODEL;

typedef struct {
  const ba_u8    *Next;
  const ba_u8    *End;
  ba_u32         Range;
  ba_u32         Code;
} BA_RC_DEC;

typedef struct {
  BA_INFO      Info;
  BA_MODEL     Model;
  BA_RC_DEC    Rc;
  ba_u8        *Plane[2];   // caller-provided, BaPlaneBytes() each
  ba_u32       Latest;      // plane holding the most recent frame
  ba_u32       Decoded;     // frames decoded since BaDecoderReset
  ba_s64       Stride;
} BA_DECODER;

static ba_u32
BaGet16 (
  const ba_u8  *P
  )
{
  return (ba_u32)P[0] | ((ba_u32)P[1] << 8);
}

static ba_u32
BaGet32 (
  const ba_u8  *P
  )
{
  return BaGet16 (P) | (BaGet16 (P + 2) << 16);
}

static void
BaPut16 (
  ba_u8   *P,
  ba_u32  V
  )
{
  P[0] = (ba_u8)V;
  P[1] = (ba_u8)(V >> 8);
}

static void
BaPut32 (
  ba_u8   *P,
  ba_u32  V
  )
{
  BaPut16 (P, V & 0xFFFFu);
  BaPut16 (P + 2, V >> 16);
}

static ba_u32
BaCrc32 (
  const ba_u8  *Data,
  ba_u64       Size
  )
{
  ba_u32  Crc = 0xFFFFFFFFu;
  ba_u64  I;
  int     K;

  for (I = 0; I < Size; I++) {
    Crc ^= Data[I];
    for (K = 0; K < 8; K++) {
      Crc = (Crc >> 1) ^ (0xEDB88320u & (0u - (Crc & 1u)));
    }
  }

  return ~Crc;
}

//
// Writes the 32-byte header for Info (StreamCrc must already be set).
//
static void
BaWriteHeader (
  ba_u8          *H,
  const BA_INFO  *Info
  )
{
  int  I;

  for (I = 0; I < BA_HEADER_BYTES; I++) {
    H[I] = 0;
  }

  H[0] = 'R';
  H[1] = 'B';
  H[2] = 'A';
  H[3] = '1';
  BaPut16 (H + 4, BA_VERSION);
  BaPut16 (H + 6, BA_HEADER_BYTES);
  BaPut16 (H + 8, Info->Width);
  BaPut16 (H + 10, Info->Height);
  BaPut16 (H + 12, Info->Fps);
  BaPut16 (H + 14, 0);
  BaPut32 (H + 16, Info->Frames);
  BaPut32 (H + 20, Info->StreamBytes);
  BaPut32 (H + 24, Info->StreamCrc);
  BaPut32 (H + 28, BaCrc32 (H, 28));
}

//
// Validates the header and the stream bounds; the stream CRC is checked
// separately by BaCheckStream (it reads the whole stream).
//
static int
BaParseHeader (
  const ba_u8  *Data,
  ba_u64       Size,
  BA_INFO      *Info
  )
{
  if (Size < BA_HEADER_BYTES) {
    return BA_ERR_SHORT;
  }

  if ((Data[0] != 'R') || (Data[1] != 'B') || (Data[2] != 'A') || (Data[3] != '1')) {
    return BA_ERR_MAGIC;
  }

  if (BaCrc32 (Data, 28) != BaGet32 (Data + 28)) {
    return BA_ERR_HEADER_CRC;
  }

  if ((BaGet16 (Data + 4) != BA_VERSION) || (BaGet16 (Data + 6) != BA_HEADER_BYTES)) {
    return BA_ERR_VERSION;
  }

  Info->Width       = BaGet16 (Data + 8);
  Info->Height      = BaGet16 (Data + 10);
  Info->Fps         = BaGet16 (Data + 12);
  Info->Frames      = BaGet32 (Data + 16);
  Info->StreamBytes = BaGet32 (Data + 20);
  Info->StreamCrc   = BaGet32 (Data + 24);
  if ((Info->Width == 0) || (Info->Width > BA_MAX_DIM) ||
      (Info->Height == 0) || (Info->Height > BA_MAX_DIM) ||
      (Info->Fps == 0) || (Info->Fps > BA_MAX_FPS) ||
      (Info->Frames == 0) || (Info->Frames > BA_MAX_FRAMES))
  {
    return BA_ERR_GEOMETRY;
  }

  if ((ba_u64)Info->StreamBytes > Size - BA_HEADER_BYTES) {
    return BA_ERR_SIZE;
  }

  return BA_OK;
}

static int
BaCheckStream (
  const ba_u8    *Data,
  const BA_INFO  *Info
  )
{
  return (BaCrc32 (Data + BA_HEADER_BYTES, Info->StreamBytes) == Info->StreamCrc) ? BA_OK : BA_ERR_STREAM_CRC;
}

static ba_u64
BaPlaneBytes (
  const BA_INFO  *Info
  )
{
  return (ba_u64)(Info->Width + 2 * BA_PAD) * (ba_u64)(Info->Height + 2 * BA_PAD);
}

static void
BaFillBytes (
  ba_u8   *P,
  ba_u64  N,
  ba_u8   V
  )
{
  while (N-- != 0) {
    *P++ = V;
  }
}

static void
BaCopyBytes (
  ba_u8        *D,
  const ba_u8  *S,
  ba_u64       N
  )
{
  while (N-- != 0) {
    *D++ = *S++;
  }
}

static int
BaBytesEqual (
  const ba_u8  *A,
  const ba_u8  *B,
  ba_u64       N
  )
{
  while (N-- != 0) {
    if (*A++ != *B++) {
      return 0;
    }
  }

  return 1;
}

static void
BaModelInit (
  BA_MODEL  *M
  )
{
  ba_u32  I;

  for (I = 0; I < BA_CTX_COUNT; I++) {
    M->Pix[I] = BA_PROB_INIT;
  }

  M->Row[0] = BA_PROB_INIT;
  M->Row[1] = BA_PROB_INIT;
  M->Same   = BA_PROB_INIT;
}

//
// Pointer to pixel (0, Y) of a padded plane.
//
static inline ba_u8 *
BaPlaneRow (
  ba_u8   *Plane,
  ba_s64  Stride,
  ba_u32  Y
  )
{
  return Plane + (ba_s64)(Y + BA_PAD) * Stride + BA_PAD;
}

//
// C points at the pixel in the frame being coded, P at the same pixel in the
// previous frame. Only pixels already coded in this frame are read from C.
//
static inline ba_u32
BaContext (
  const ba_u8  *C,
  const ba_u8  *P,
  ba_s64       S
  )
{
  return (ba_u32)C[-1]
         | ((ba_u32)C[-2] << 1)
         | ((ba_u32)C[-S] << 2)
         | ((ba_u32)C[-S - 1] << 3)
         | ((ba_u32)C[-S + 1] << 4)
         | ((ba_u32)C[-2 * S] << 5)
         | ((ba_u32)P[0] << 6)
         | ((ba_u32)P[-1] << 7)
         | ((ba_u32)P[1] << 8)
         | ((ba_u32)P[-S] << 9)
         | ((ba_u32)P[S] << 10)
         | ((ba_u32)C[-S + 2] << 11);
}

static inline ba_u32
BaRcByte (
  BA_RC_DEC  *Rc
  )
{
  return (Rc->Next < Rc->End) ? *Rc->Next++ : 0;
}

static void
BaRcDecInit (
  BA_RC_DEC    *Rc,
  const ba_u8  *Data,
  ba_u64       Size
  )
{
  int  I;

  Rc->Next  = Data;
  Rc->End   = Data + Size;
  Rc->Range = 0xFFFFFFFFu;
  Rc->Code  = 0;
  for (I = 0; I < 5; I++) {
    Rc->Code = (Rc->Code << 8) | BaRcByte (Rc);
  }
}

static inline ba_u32
BaRcDecBit (
  BA_RC_DEC  *Rc,
  ba_u16     *Prob
  )
{
  ba_u32  P     = *Prob;
  ba_u32  Bound = (Rc->Range >> BA_PROB_BITS) * P;
  ba_u32  Bit;

  if (Rc->Code < Bound) {
    Rc->Range = Bound;
    *Prob     = (ba_u16)(P + ((BA_PROB_ONE - P) >> BA_MOVE_BITS));
    Bit       = 0;
  } else {
    Rc->Range -= Bound;
    Rc->Code  -= Bound;
    *Prob      = (ba_u16)(P - (P >> BA_MOVE_BITS));
    Bit        = 1;
  }

  while (Rc->Range < BA_TOP) {
    Rc->Range <<= 8;
    Rc->Code    = (Rc->Code << 8) | BaRcByte (Rc);
  }

  return Bit;
}

//
// D->Info and D->Plane[] must be set. Stream points at the byte after the
// header.
//
static void
BaDecoderReset (
  BA_DECODER   *D,
  const ba_u8  *Stream
  )
{
  ba_u64  N = BaPlaneBytes (&D->Info);

  BaFillBytes (D->Plane[0], N, 0);
  BaFillBytes (D->Plane[1], N, 0);
  BaModelInit (&D->Model);
  BaRcDecInit (&D->Rc, Stream, D->Info.StreamBytes);
  D->Latest  = 0;
  D->Decoded = 0;
  D->Stride  = (ba_s64)D->Info.Width + 2 * BA_PAD;
}

//
// Decodes the next frame into D->Plane[D->Latest]. Returns 1 if it differs
// from the previous frame, 0 if it is the same.
//
static int
BaDecodeFrame (
  BA_DECODER  *D
  )
{
  ba_u8   *Prev   = D->Plane[D->Latest];
  ba_u8   *Cur    = D->Plane[D->Latest ^ 1];
  ba_s64  S       = D->Stride;
  ba_u32  W       = D->Info.Width;
  ba_u32  H       = D->Info.Height;
  ba_u32  RowSame = 0;
  ba_u32  X;
  ba_u32  Y;

  D->Decoded++;
  if (BaRcDecBit (&D->Rc, &D->Model.Same) != 0) {
    return 0;
  }

  for (Y = 0; Y < H; Y++) {
    ba_u8        *C = BaPlaneRow (Cur, S, Y);
    const ba_u8  *P = BaPlaneRow (Prev, S, Y);

    RowSame = BaRcDecBit (&D->Rc, &D->Model.Row[RowSame]);
    if (RowSame != 0) {
      BaCopyBytes (C, P, W);
      continue;
    }

    for (X = 0; X < W; X++) {
      C[X] = (ba_u8)BaRcDecBit (&D->Rc, &D->Model.Pix[BaContext (C + X, P + X, S)]);
    }
  }

  D->Latest ^= 1;
  return 1;
}

#endif // BA_CODEC_H_
