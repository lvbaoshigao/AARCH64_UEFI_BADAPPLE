/** @file
  BadApple: AArch64 UEFI player for the embedded "RBA1" Bad Apple!! stream,
  started from the rothko UniXNU menu (or the firmware Boot Manager).

  The stream (BaCodec.h) is linked into the image by BaBlob.S, so the player
  needs no file system access. It is checked (header CRC, stream CRC) before
  anything is drawn.

  Picture: 1-bit frames scaled up bilinearly to the screen width (aspect
  kept), so edges come out as soft grey instead of blocks; centred, black
  around it; a title above, and below it the play time, the frame rate
  actually drawn, a progress bar and the key hints. Only the Graphics Output
  Protocol is used, as in RothkoAnim.

  Timing comes from the generic timer (CNTVCT/CNTFRQ). Frames are decoded in
  order (each depends on the previous one); when drawing falls behind, the
  frames in between are decoded but not drawn, so playback keeps real time.
  RothkoDisplayDxe pushes the frame buffer to the panel from a periodic timer
  armed with SetTimer (200000) in 100 ns units, i.e. every 20 ms (its comment
  says 200 ms, the value is 20 ms), so the panel is not the limit at 30 fps.
  The "N/M FPS" readout shows frames drawn in the last second against the
  stream rate.

  No sound: the rothko firmware has no audio driver.

  Output (see gMode): only the screen rectangles under source tiles that
  changed are redrawn; by default (OWN) they are written into every scanout
  layer and the player starts the DSI frame itself, so the display driver's
  per-refresh full-frame mirror copy never runs during playback.

  Keys (rothko KeypadDeviceLib): volume up pauses and resumes, volume down
  cycles the output mode (GOP / SYNC / DIRECT / OWN); power
  (CHAR_CARRIAGE_RETURN) or any other key returns to the menu. Keys during
  the first 1.5 s are drained. At the end it shows "THE END" for 2 s and
  returns; load option "loop=1" plays it again instead.

  The MTK RGU watchdog is fed every 500 ms (0x1C00B000 + 0x08 <- 0x1971, as
  RothkoWatchdogDxe does) and the UEFI watchdog is disabled.

  Freestanding: no EDK II libraries. Built by build-badapple-efi.sh.

  Reference: rinsuki/badapple-efi (MIT) for the idea; this is a rewrite.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;
typedef signed short        INT16;
typedef signed int          INT32;
typedef signed long long    INT64;
typedef UINT64              UINTN;
typedef UINT16              CHAR16;
typedef UINTN               EFI_STATUS;
typedef void                *EFI_HANDLE;
typedef void                *EFI_EVENT;

#define NULL                  ((void *)0)
#define EFI_SUCCESS           0ULL
#define EFI_ERROR(s)          (((INT64)(s)) < 0)
#define EFI_UNSUPPORTED       (0x8000000000000000ULL | 3)
#define EFI_OUT_OF_RESOURCES  (0x8000000000000000ULL | 9)
#define EFI_CRC_ERROR         (0x8000000000000000ULL | 27)
#define EFI_ABORTED           (0x8000000000000000ULL | 21)
#define EFI_BUFFER_TOO_SMALL  (0x8000000000000000ULL | 5)

#define EfiLoaderData         2
#define EfiBltVideoFill       0
#define EfiBltBufferToVideo   2

#define SCAN_UP               0x0001
#define SCAN_DOWN             0x0002

typedef struct {
  UINT32    Data1;
  UINT16    Data2;
  UINT16    Data3;
  UINT8     Data4[8];
} EFI_GUID;

typedef struct {
  UINT64    Signature;
  UINT32    Revision;
  UINT32    HeaderSize;
  UINT32    CRC32;
  UINT32    Reserved;
} EFI_TABLE_HEADER;

typedef struct {
  UINT16    ScanCode;
  CHAR16    UnicodeChar;
} EFI_INPUT_KEY;

typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
  EFI_STATUS    (*Reset)(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, UINT8 Extended);
  EFI_STATUS    (*ReadKeyStroke)(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key);
  EFI_EVENT     WaitForKey;
};

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
  EFI_STATUS    (*Reset)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINT8 Extended);
  EFI_STATUS    (*OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *String);
};

#define TPL_APPLICATION  4
#define TPL_CALLBACK     8

typedef struct {
  UINT32    Type;
  UINT32    Pad;
  UINT64    PhysicalStart;
  UINT64    VirtualStart;
  UINT64    NumberOfPages;
  UINT64    Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef struct {
  EFI_TABLE_HEADER    Hdr;
  UINTN               (*RaiseTPL)(UINTN NewTpl);
  void                (*RestoreTPL)(UINTN OldTpl);
  void                *AllocatePages;
  void                *FreePages;
  EFI_STATUS          (*GetMemoryMap)(UINTN *MapSize, EFI_MEMORY_DESCRIPTOR *Map, UINTN *MapKey, UINTN *DescriptorSize, UINT32 *DescriptorVersion);
  EFI_STATUS          (*AllocatePool)(UINT32 PoolType, UINTN Size, void **Buffer);
  EFI_STATUS          (*FreePool)(void *Buffer);
  void                *CreateEvent;
  void                *SetTimer;
  void                *WaitForEvent;
  void                *SignalEvent;
  void                *CloseEvent;
  void                *CheckEvent;
  void                *InstallProtocolInterface;
  void                *ReinstallProtocolInterface;
  void                *UninstallProtocolInterface;
  EFI_STATUS          (*HandleProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol, void **Interface);
  void                *Reserved;
  void                *RegisterProtocolNotify;
  void                *LocateHandle;
  void                *LocateDevicePath;
  void                *InstallConfigurationTable;
  void                *LoadImage;
  void                *StartImage;
  void                *Exit;
  void                *UnloadImage;
  void                *ExitBootServices;
  void                *GetNextMonotonicCount;
  EFI_STATUS          (*Stall)(UINTN Microseconds);
  EFI_STATUS          (*SetWatchdogTimer)(UINTN Timeout, UINT64 Code, UINTN DataSize, CHAR16 *Data);
  void                *ConnectController;
  void                *DisconnectController;
  void                *OpenProtocol;
  void                *CloseProtocol;
  void                *OpenProtocolInformation;
  void                *ProtocolsPerHandle;
  void                *LocateHandleBuffer;
  EFI_STATUS          (*LocateProtocol)(EFI_GUID *Protocol, void *Registration, void **Interface);
} EFI_BOOT_SERVICES;

typedef struct {
  EFI_TABLE_HEADER                   Hdr;
  CHAR16                             *FirmwareVendor;
  UINT32                             FirmwareRevision;
  EFI_HANDLE                         ConsoleInHandle;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL     *ConIn;
  EFI_HANDLE                         ConsoleOutHandle;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL    *ConOut;
  EFI_HANDLE                         StandardErrorHandle;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL    *StdErr;
  void                               *RuntimeServices;
  EFI_BOOT_SERVICES                  *BootServices;
  UINTN                              NumberOfTableEntries;
  void                               *ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct {
  UINT32              Revision;
  EFI_HANDLE          ParentHandle;
  EFI_SYSTEM_TABLE    *SystemTable;
  EFI_HANDLE          DeviceHandle;
  void                *FilePath;
  void                *Reserved;
  UINT32              LoadOptionsSize;
  void                *LoadOptions;
  void                *ImageBase;
  UINT64              ImageSize;
  UINT32              ImageCodeType;
  UINT32              ImageDataType;
  void                *Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

typedef struct {
  UINT32    RedMask;
  UINT32    GreenMask;
  UINT32    BlueMask;
  UINT32    ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct {
  UINT32               Version;
  UINT32               HorizontalResolution;
  UINT32               VerticalResolution;
  UINT32               PixelFormat;
  EFI_PIXEL_BITMASK    PixelInformation;
  UINT32               PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
  UINT32                                  MaxMode;
  UINT32                                  Mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Info;
  UINTN                                   SizeOfInfo;
  UINT64                                  FrameBufferBase;
  UINTN                                   FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;
struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
  void                                 *QueryMode;
  void                                 *SetMode;
  //
  // Buffer is EFI_GRAPHICS_OUTPUT_BLT_PIXEL: bytes B, G, R, reserved, i.e.
  // a little-endian UINT32 0x00RRGGBB.
  //
  EFI_STATUS                           (*Blt)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *This,
    UINT32                        *Buffer,
    UINT32                        Operation,
    UINTN                         SourceX,
    UINTN                         SourceY,
    UINTN                         DestinationX,
    UINTN                         DestinationY,
    UINTN                         Width,
    UINTN                         Height,
    UINTN                         Delta
    );
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE    *Mode;
};

static EFI_GUID  mGopGuid = {
  0x9042a9de, 0x23dc, 0x4a38, { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a }
};
static EFI_GUID  mLoadedImageGuid = {
  0x5B1B31A1, 0x9562, 0x11d2, { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

//
// Compiler support: clang may still emit calls to these for struct copies.
//
void *
memset (
  void           *Dest,
  int            Value,
  __SIZE_TYPE__  Count
  )
{
  volatile UINT8  *P = (volatile UINT8 *)Dest;

  while (Count-- != 0) {
    *P++ = (UINT8)Value;
  }

  return Dest;
}

void *
memcpy (
  void           *Dest,
  const void     *Src,
  __SIZE_TYPE__  Count
  )
{
  volatile UINT8  *D = (volatile UINT8 *)Dest;
  const UINT8     *S = (const UINT8 *)Src;

  while (Count-- != 0) {
    *D++ = *S++;
  }

  return Dest;
}

#include "AnimTables.h"
#include "BaCodec.h"

//
// The stream, from BaBlob.S.
//
extern const UINT8  gBaData[];
extern const UINT8  gBaDataEnd[];

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

#define KEY_GRACE_MS  1500
#define WDT_FEED_MS   500
#define INFO_MS       250
#define END_MS        2000
#define ERROR_MS      6000

#define COLOR_WHITE   0x00FFFFFFU
#define COLOR_BLACK   0x00000000U
#define COLOR_TEXT    0x00C8C8C8U
#define COLOR_DIM     0x00707070U
#define COLOR_BAR     0x00E04848U
#define COLOR_TRACK   0x00303030U

#define ROTHKO_WDT_RESTART      (0x1C00B000ULL + 0x08)
#define ROTHKO_WDT_RESTART_KEY  0x00001971U

static EFI_SYSTEM_TABLE              *gST;
static EFI_BOOT_SERVICES             *gBS;
static EFI_GRAPHICS_OUTPUT_PROTOCOL  *gGop;

static UINT32  gW, gH;              // screen
static UINT32  gVX, gVY, gVW, gVH;  // video rectangle on screen
static UINT32  gTextScale;
static UINT32  gTitleScale;
static UINT32  gTitleY, gTitleH;
static UINT32  gInfoY, gInfoH;      // 0 height = no room for the info panel

static UINT32  *gVideo;             // gVW * gVH
static UINT32  *gInfo;              // gW * Max(gInfoH, gTitleH)
static UINT32  *gXMap;              // gVW: left source column per screen column
static UINT32  *gXW;                // gVW: weight 0..256 of the column to its right
static UINT32  *gYMap;              // gVH: upper source row per screen row
static UINT32  *gYW;                // gVH: weight 0..256 of the row below
static void    *gPool;
static UINT32  gGray[257];          // bilinear value 0..256 -> pixel
static UINT32  gShownFps;           // frames drawn in the last full second
static UINT8   *gShown;             // copy of the plane last drawn (same layout)
static int     gShownValid;         // 0 = next RenderFrame draws everything

//
// Output modes (volume down cycles them, load option "mode=N" picks one):
//   0 GOP     GOP Blt; the display refresh timer runs whenever it fires.
//   1 SYNC    GOP Blt; TPL_CALLBACK is held while a frame is made, so the
//             refresh (RothkoDisplayDxe: a full-frame mirror copy into the
//             second scanout layer, then a DSI push, every 20 ms) runs at
//             most once per presented frame.
//   2 DIRECT  as 1, but the dirty rectangles are stored straight into the
//             GOP frame buffer (alpha set, cleaned to PoC): no Blt read-back.
//   3 OWN     TPL_CALLBACK is held for the whole playback, so the firmware
//             refresh never runs. The dirty rectangles go straight into every
//             scanout layer and the player arms the mutex and starts the DSI
//             frame itself: what Refresh() does, without the 13.5 MB mirror
//             copy. The keypad timer is held back as well, so keys are read
//             from the GPIO (volume up) and the PMIC latches (power, volume
//             down) directly, the way KeypadDeviceLib does.
//
#define MODE_GOP     0U
#define MODE_SYNC    1U
#define MODE_DIRECT  2U
#define MODE_OWN     3U
#define MODE_COUNT   4U

static UINT32  gMode = MODE_OWN;
static UINT32  *gFb;                // NULL = direct modes unavailable
static UINT32  gFbPitch;            // pixels
static UINTN   gCacheLine = 64;
static UINTN   gOldTpl;
static int     gRaised;
static UINT64  gAvgDec;             // per presented frame, microseconds
static UINT64  gAvgDraw;
static UINT64  gAvgOut;
static UINT64  gAvgFw;              // time not spent here: firmware callbacks
static UINT32  gAvgPx;              // percent of the video area redrawn per frame
static UINT64  gRDraw;              // last RenderFrame: upscale time, us
static UINT64  gRPixels;            // last RenderFrame: screen pixels redrawn

#define OWN_MAX_LAYERS  2U
static UINT32  *gLayer[OWN_MAX_LAYERS];       // gLayer[0] == gFb
static UINT32  gLayerPitch[OWN_MAX_LAYERS];   // pixels
static UINT32  gLayerCount;
static UINT32  gMutexId;
static UINT32  gMutexMask;
static UINT32  gOwnWhy = 1;         // 0 = OWN usable, else why not (shown on screen)
static int     gNeedPush;
static UINT64  gLastPushUs;
static int     gUpWas;
static UINT64  gUpSince;
static UINT32  gPmicFails;

static BA_DECODER  gDec;
static const UINT8 *gStream;

static UINT64  gFreq;
static UINT64  gT0;
static UINT64  gFakeMs;
static UINT64  gLastFeed;

static inline UINT64
ReadCounter (
  void
  )
{
  UINT64  Value;

  __asm__ volatile ("isb\n\tmrs %0, cntvct_el0" : "=r" (Value) :: "memory");
  return Value;
}

static inline UINT64
ReadFrequency (
  void
  )
{
  UINT64  Value;

  __asm__ volatile ("mrs %0, cntfrq_el0" : "=r" (Value));
  return Value;
}

static UINT64
NowMs (
  void
  )
{
  if (gFreq == 0) {
    return gFakeMs;
  }

  return ((ReadCounter () - gT0) * 1000ULL) / gFreq;
}

static UINT64
NowUs (
  void
  )
{
  if (gFreq == 0) {
    return gFakeMs * 1000;
  }

  return ((ReadCounter () - gT0) * 1000000ULL) / gFreq;
}

static void
SleepMs (
  UINT64  Ms
  )
{
  gBS->Stall ((UINTN)Ms * 1000);
  if (gFreq == 0) {
    gFakeMs += Ms;
  }
}

static void
FeedWatchdog (
  void
  )
{
  *(volatile UINT32 *)(UINTN)ROTHKO_WDT_RESTART = ROTHKO_WDT_RESTART_KEY;
  __asm__ volatile ("dsb sy" ::: "memory");
}

static void
MaybeFeedWatchdog (
  UINT64  Now
  )
{
  if (Now - gLastFeed >= WDT_FEED_MS) {
    FeedWatchdog ();
    gLastFeed = Now;
  }
}

//
// Sleep in short steps so the watchdog keeps being fed.
//
static void
WaitMs (
  UINT64  Ms
  )
{
  UINT64  End = NowMs () + Ms;
  UINT64  Now;

  while ((Now = NowMs ()) < End) {
    MaybeFeedWatchdog (Now);
    SleepMs ((End - Now) > 50 ? 50 : (End - Now));
  }
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

static UINTN
AsciiLen (
  const char  *S
  )
{
  UINTN  N = 0;

  while (S[N] != '\0') {
    N++;
  }

  return N;
}

static char *
Append (
  char        *Out,
  const char  *S
  )
{
  while (*S != '\0') {
    *Out++ = *S++;
  }

  *Out = '\0';
  return Out;
}

static char *
AppendUint (
  char    *Out,
  UINT32  Value
  )
{
  char   Tmp[12];
  UINTN  N = 0;

  do {
    Tmp[N++] = (char)('0' + (Value % 10));
    Value   /= 10;
  } while ((Value != 0) && (N < sizeof (Tmp)));

  while (N != 0) {
    *Out++ = Tmp[--N];
  }

  *Out = '\0';
  return Out;
}

static char *
AppendTime (
  char    *Out,
  UINT32  Seconds
  )
{
  UINT32  M = Seconds / 60;
  UINT32  S = Seconds % 60;

  if (M < 10) {
    *Out++ = '0';
  }

  Out    = AppendUint (Out, M);
  *Out++ = ':';
  *Out++ = (char)('0' + S / 10);
  *Out++ = (char)('0' + S % 10);
  *Out   = '\0';
  return Out;
}

static void
Fill (
  UINT32  *Buf,
  UINT32  BufW,
  UINT32  X,
  UINT32  Y,
  UINT32  W,
  UINT32  H,
  UINT32  Color
  )
{
  UINT32  Row;
  UINT32  Col;
  UINT32  *Line;

  for (Row = 0; Row < H; Row++) {
    Line = Buf + (UINTN)(Y + Row) * BufW + X;
    for (Col = 0; Col < W; Col++) {
      Line[Col] = Color;
    }
  }
}

static void
Glyph (
  UINT32  *Buf,
  UINT32  BufW,
  UINT32  X,
  UINT32  Y,
  UINT8   Ch,
  UINT32  Scale,
  UINT32  Fg
  )
{
  const UINT8  *Bits;
  UINT32       Row;
  UINT32       Sy;
  UINT32       Col;
  UINT32       Sx;
  UINT32       *Line;

  if ((Ch < 0x20) || (Ch > 0x7E)) {
    Ch = '?';
  }

  Bits = AnimFont[Ch - 0x20];
  for (Row = 0; Row < ANIM_GLYPH_H; Row++) {
    for (Sy = 0; Sy < Scale; Sy++) {
      Line = Buf + (UINTN)(Y + Row * Scale + Sy) * BufW + X;
      for (Col = 0; Col < ANIM_GLYPH_W; Col++) {
        if ((Bits[Row] & (0x80 >> Col)) != 0) {
          for (Sx = 0; Sx < Scale; Sx++) {
            Line[Col * Scale + Sx] = Fg;
          }
        }
      }
    }
  }
}

static void
TextCentered (
  UINT32      *Buf,
  UINT32      BufW,
  UINT32      Y,
  const char  *S,
  UINT32      Scale,
  UINT32      Fg
  )
{
  UINT32  Width = (UINT32)AsciiLen (S) * ANIM_GLYPH_W * Scale;
  UINT32  X     = (Width < BufW) ? (BufW - Width) / 2 : 0;

  while (*S != '\0') {
    if (X + ANIM_GLYPH_W * Scale > BufW) {
      break;
    }

    Glyph (Buf, BufW, X, Y, (UINT8)*S, Scale, Fg);
    X += ANIM_GLYPH_W * Scale;
    S++;
  }
}

static void
BltRect (
  UINT32  *Buf,
  UINT32  BufW,
  UINT32  DstX,
  UINT32  DstY,
  UINT32  W,
  UINT32  H
  )
{
  if ((W == 0) || (H == 0)) {
    return;
  }

  gGop->Blt (gGop, Buf, EfiBltBufferToVideo, 0, 0, DstX, DstY, W, H, (UINTN)BufW * sizeof (UINT32));
}

static void
ClearScreen (
  void
  )
{
  UINT32  Black = COLOR_BLACK;

  gGop->Blt (gGop, &Black, EfiBltVideoFill, 0, 0, 0, 0, gW, gH, 0);
}

//
// Up to three centred lines on a black screen (errors, no layout needed).
//
static void
ShowMessage (
  const char  *L1,
  const char  *L2,
  const char  *L3,
  UINT32      Scale
  )
{
  static UINT32  Line[1280 * ANIM_GLYPH_H * 3];
  UINT32         W     = (gW < 1280) ? gW : 1280;
  UINT32         H     = ANIM_GLYPH_H * Scale;
  UINT32         Y     = gH / 2 - 2 * H;
  const char     *L[3] = { L1, L2, L3 };
  int            I;

  if (Scale > 3) {
    return;
  }

  ClearScreen ();
  for (I = 0; I < 3; I++) {
    if (L[I] == NULL) {
      continue;
    }

    Fill (Line, W, 0, 0, W, H, COLOR_BLACK);
    TextCentered (Line, W, 0, L[I], Scale, (I == 0) ? COLOR_BAR : COLOR_TEXT);
    BltRect (Line, W, (gW - W) / 2, Y + (UINT32)I * (H + H / 2), W, H);
  }
}

// ---------------------------------------------------------------------------
// Video
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

//
// Display registers, as RothkoDisplayDxe uses them (it probes the same OVL
// layers and the same mutex group at boot; nothing here is programmed that
// the driver does not program on every refresh).
//
#define OVL_OVLSYS0_BASE    0x14402000U
#define OVL_OVLSYS1_BASE    0x14602000U
#define OVL_UNIT_STRIDE     0x00001000U
#define OVL_UNIT_COUNT      3U
#define OVL_EN              0x00CU
#define OVL_ROI_SIZE        0x020U
#define OVL_SRC_CON         0x02CU
#define OVL_CON(n)          (0x030U + 0x20U * (n))
#define OVL_SRC_SIZE(n)     (0x038U + 0x20U * (n))
#define OVL_PITCH_MSB(n)    (0x040U + 0x20U * (n))
#define OVL_PITCH(n)        (0x044U + 0x20U * (n))
#define OVL_ADDR(n)         (0x0F40U + 0x20U * (n))
#define OVL_ADDR_MSB(n)     (0x0F4CU + 0x20U * (n))
#define L_CON_CFMT_MASK     (0xFU << 12)
#define L_CON_CFMT_RGBA     (2U << 12)
#define L_CON_BTSW          (1U << 24)
#define MUTEX_EN(n)         (0x20U + 0x20U * (n))
#define MUTEX_SOF(n)        (0x2CU + 0x20U * (n))
#define MUTEX_MOD(n)        (0x30U + 0x20U * (n))
#define MUTEX_MOD2(n)       (0x34U + 0x20U * (n))
#define MUTEX_ID_COUNT      10U
#define MUTEX_SIDE_BANK     0x14201000U
#define MUTEX_DSI0_BIT      (1U << 15)
#define DSI0_BASE           0x1420A000U
#define DSI_START           0x000U
#define FB_CARVEOUT_BYTES   0x026E0000ULL   // LK atag,videolfb size (MemoryMapLib "Display Reserved")
#define PUSH_MIN_US         20000ULL        // the driver's own refresh period

//
// Keys (rothko KeypadDeviceLib): volume up = GPIO196, DIN at pinctrl + 0x260
// bit 4, active low; power / volume down = MT6363 key latches at PMIC 0x91B
// bits 0 / 1 over PMIF SID 4, write 1 to clear.
//
#define GPIO_VOLUP_DIN      (0x10005000U + 0x260U)
#define GPIO_VOLUP_BIT      (1U << 4)
#define PMIF_BASE           0x1CC04000U
#define PMIC_KEY_STATUS     0x91BU
#define PMIC_KEY_IRQS       0x1BU
#define PMIC_KEY_ENABLE     0x90FU
#define UP_HOLD_QUIT_MS     3000ULL         // OWN safety exit: hold volume up

static const UINT32  kMutexBanks[4] = { 0x14001000U, 0x14201000U, 0x14401000U, 0x14601000U };

static inline UINT32
Rd32 (
  UINTN  A
  )
{
  return *(volatile UINT32 *)A;
}

static inline void
Wr32 (
  UINTN   A,
  UINT32  V
  )
{
  *(volatile UINT32 *)A = V;
}

static const char *
ModeName (
  UINT32  Mode
  )
{
  if (Mode == MODE_GOP) {
    return "GOP";
  }

  if (Mode == MODE_SYNC) {
    return "SYNC";
  }

  if (Mode == MODE_DIRECT) {
    return "DIRECT";
  }

  return "OWN";
}

static int
ModeAvailable (
  UINT32  Mode
  )
{
  if (Mode == MODE_DIRECT) {
    return gFb != NULL;
  }

  if (Mode == MODE_OWN) {
    return (gFb != NULL) && (gOwnWhy == 0);
  }

  return Mode < MODE_COUNT;
}

static UINT32
NextMode (
  UINT32  Mode
  )
{
  do {
    Mode = (Mode + 1) % MODE_COUNT;
  } while (!ModeAvailable (Mode));

  return Mode;
}

//
// Hold back TPL_CALLBACK notifications (display refresh, keypad scan, the
// firmware watchdog kick) in the SYNC / DIRECT / OWN modes. Ungate() lets
// the pending ones run.
//
static void
Gate (
  void
  )
{
  if (!gRaised && (gMode != MODE_GOP)) {
    gOldTpl = gBS->RaiseTPL (TPL_CALLBACK);
    gRaised = 1;
  }
}

static void
Ungate (
  void
  )
{
  if (gRaised) {
    gRaised = 0;
    gBS->RestoreTPL (gOldTpl);
  }
}

static void
CleanRange (
  const void  *P,
  UINTN       Bytes
  )
{
  UINTN  A = (UINTN)P & ~(gCacheLine - 1);
  UINTN  E = (UINTN)P + Bytes;

  for ( ; A < E; A += gCacheLine) {
    __asm__ volatile ("dc cvac, %0" :: "r" (A) : "memory");
  }
}

static void
InitDirect (
  void
  )
{
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  *M = gGop->Mode;
  UINT64                             Ctr;

  gFb = NULL;
  if ((M->FrameBufferBase == 0) || (M->Info->PixelFormat != 1) ||   // PixelBlueGreenRedReserved8BitPerColor
      (M->Info->PixelsPerScanLine < gW) ||
      ((UINT64)M->Info->PixelsPerScanLine * gH * sizeof (UINT32) > M->FrameBufferSize))
  {
    return;
  }

  __asm__ volatile ("mrs %0, ctr_el0" : "=r" (Ctr));
  gCacheLine = (UINTN)4 << ((Ctr >> 16) & 0xF);
  if ((gCacheLine < 16) || (gCacheLine > 2048)) {
    gCacheLine = 64;
  }

  gFb      = (UINT32 *)(UINTN)M->FrameBufferBase;
  gFbPitch = M->Info->PixelsPerScanLine;
}

//
// The memory map entry holding the frame buffer: the "Display Reserved"
// carveout that every scanout layer has to sit in.
//
static int
FindRegion (
  UINT64  Addr,
  UINT64  *Lo,
  UINT64  *Hi
  )
{
  UINTN       Size  = 0;
  UINTN       Key   = 0;
  UINTN       DSize = 0;
  UINT32      Ver   = 0;
  void        *Map  = NULL;
  UINTN       Off;
  int         Found = 0;
  EFI_STATUS  St;

  St = gBS->GetMemoryMap (&Size, NULL, &Key, &DSize, &Ver);
  if ((St != EFI_BUFFER_TOO_SMALL) || (Size == 0)) {
    return 0;
  }

  Size += 8192;
  if (EFI_ERROR (gBS->AllocatePool (EfiLoaderData, Size, &Map)) || (Map == NULL)) {
    return 0;
  }

  St = gBS->GetMemoryMap (&Size, (EFI_MEMORY_DESCRIPTOR *)Map, &Key, &DSize, &Ver);
  if (!EFI_ERROR (St) && (DSize >= sizeof (EFI_MEMORY_DESCRIPTOR))) {
    for (Off = 0; Off + DSize <= Size; Off += DSize) {
      EFI_MEMORY_DESCRIPTOR  *D = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + Off);
      UINT64                 E  = D->PhysicalStart + D->NumberOfPages * 4096ULL;

      if ((Addr >= D->PhysicalStart) && (Addr < E)) {
        *Lo   = D->PhysicalStart;
        *Hi   = E;
        Found = 1;
        break;
      }
    }
  }

  gBS->FreePool (Map);
  return Found;
}

//
// Same acceptance rules as the driver's CollectScanout().
//
static void
AddLayer (
  UINT64  Addr,
  UINT32  PitchBytes,
  UINT32  Con,
  UINT32  Size,
  UINT64  Lo,
  UINT64  Hi
  )
{
  UINT32  I;

  if ((Addr == 0) || (Addr < Lo) || (Addr >= Hi)) {
    return;
  }

  if ((Size != (gW | (gH << 16))) || ((Con & L_CON_CFMT_MASK) != L_CON_CFMT_RGBA) ||
      ((Con & L_CON_BTSW) != 0))
  {
    return;
  }

  if ((PitchBytes < gW * sizeof (UINT32)) || ((PitchBytes & 3U) != 0) ||
      ((UINT64)PitchBytes * gH > Hi - Addr))
  {
    return;
  }

  for (I = 0; I < gLayerCount; I++) {
    if ((UINT64)(UINTN)gLayer[I] == Addr) {
      return;
    }
  }

  if (gLayerCount >= OWN_MAX_LAYERS) {
    return;
  }

  gLayer[gLayerCount]      = (UINT32 *)(UINTN)Addr;
  gLayerPitch[gLayerCount] = PitchBytes / sizeof (UINT32);
  gLayerCount++;
}

static int
ProbeMutex (
  void
  )
{
  UINT32  Id;
  UINT32  Bank;
  UINT32  Matches = 0;

  for (Id = 0; Id < MUTEX_ID_COUNT; Id++) {
    UINT32  Mask   = 0;
    UINT32  Mod[4];
    int     Single = 1;
    int     HasDsi = 0;
    int     HasOvl = 0;

    for (Bank = 0; Bank < 4; Bank++) {
      UINTN   B = kMutexBanks[Bank];
      UINT32  Sof;

      Mod[Bank] = Rd32 (B + MUTEX_MOD (Id));
      Sof       = Rd32 (B + MUTEX_SOF (Id));
      if ((Mod[Bank] == 0) && (Rd32 (B + MUTEX_MOD2 (Id)) == 0)) {
        continue;
      }

      Mask |= 1U << Bank;
      if ((Sof & 7U) != 0) {
        Single = 0;
      }
    }

    if ((Mask == 0) || !Single) {
      continue;
    }

    for (Bank = 0; Bank < 4; Bank++) {
      if ((kMutexBanks[Bank] == MUTEX_SIDE_BANK) && ((Mod[Bank] & MUTEX_DSI0_BIT) != 0)) {
        HasDsi = 1;
      }

      if (((kMutexBanks[Bank] == 0x14401000U) || (kMutexBanks[Bank] == 0x14601000U)) &&
          ((Mod[Bank] & 0x0FU) != 0))
      {
        HasOvl = 1;
      }
    }

    if (!HasDsi || !HasOvl) {
      continue;
    }

    gMutexId   = Id;
    gMutexMask = Mask;
    Matches++;
  }

  return Matches == 1;
}

static int
PmicWait (
  UINT32  State
  )
{
  UINT32  I;

  for (I = 0; I < 10000; I++) {
    if (((Rd32 (PMIF_BASE + 0x8A8) >> 1) & 7U) == State) {
      return 1;
    }

    gBS->Stall (1);
  }

  return 0;
}

static int
PmicRead (
  UINT32  Reg,
  UINT8   *Value
  )
{
  if (!PmicWait (0)) {
    return 0;
  }

  Wr32 (PMIF_BASE + 0x880, (3U << 30) | (4U << 24) | Reg);
  if (!PmicWait (6)) {
    return 0;
  }

  *Value = (UINT8)Rd32 (PMIF_BASE + 0x894);
  Wr32 (PMIF_BASE + 0x8A4, 1);
  return 1;
}

static int
PmicWrite (
  UINT32  Reg,
  UINT8   Value
  )
{
  if (!PmicWait (0)) {
    return 0;
  }

  Wr32 (PMIF_BASE + 0x884, Value);
  Wr32 (PMIF_BASE + 0x880, (3U << 30) | (1U << 29) | (4U << 24) | Reg);
  return PmicWait (0);
}

//
// OWN needs: the direct frame buffer, every scanout layer (the frame buffer
// among them), exactly one DSI0 mutex group, and a working PMIC key read.
// gOwnWhy: 1 no direct fb, 2 frame buffer is not a scanout layer,
// 3 mutex group not found, 4 PMIC key latch unreadable.
//
static void
InitOwn (
  void
  )
{
  UINT64  Lo;
  UINT64  Hi;
  UINT32  Side;
  UINT32  Unit;
  UINT32  Layer;
  UINT32  I;
  UINT8   V;

  gLayerCount = 0;
  if (gFb == NULL) {
    gOwnWhy = 1;
    return;
  }

  if (!FindRegion ((UINT64)(UINTN)gFb, &Lo, &Hi)) {
    Lo = (UINT64)(UINTN)gFb;
    Hi = Lo + FB_CARVEOUT_BYTES;
  }

  for (Side = 0; Side < 2; Side++) {
    for (Unit = 0; Unit < OVL_UNIT_COUNT; Unit++) {
      UINTN   Base   = ((Side == 0) ? OVL_OVLSYS0_BASE : OVL_OVLSYS1_BASE) + Unit * OVL_UNIT_STRIDE;
      UINT32  SrcCon = Rd32 (Base + OVL_SRC_CON);

      if (((Rd32 (Base + OVL_EN) & 1U) == 0) || (SrcCon == 0) || ((SrcCon & ~3U) != 0) ||
          (Rd32 (Base + OVL_ROI_SIZE) != (gW | (gH << 16))))
      {
        continue;
      }

      for (Layer = 0; Layer < 2; Layer++) {
        UINT64  A;
        UINT32  P;

        if ((SrcCon & (1U << Layer)) == 0) {
          continue;
        }

        A  = Rd32 (Base + OVL_ADDR (Layer));
        A |= (UINT64)(Rd32 (Base + OVL_ADDR_MSB (Layer)) & 0xFU) << 32;
        P  = (Rd32 (Base + OVL_PITCH (Layer)) & 0xFFFFU) | ((Rd32 (Base + OVL_PITCH_MSB (Layer)) & 0xFU) << 16);
        AddLayer (A, P, Rd32 (Base + OVL_CON (Layer)), Rd32 (Base + OVL_SRC_SIZE (Layer)), Lo, Hi);
      }
    }
  }

  //
  // The frame buffer first, with the pitch the GOP reports.
  //
  for (I = 0; I < gLayerCount; I++) {
    if (gLayer[I] == gFb) {
      UINT32  *T  = gLayer[0];
      UINT32  Tp  = gLayerPitch[0];

      gLayer[0]      = gLayer[I];
      gLayerPitch[0] = gLayerPitch[I];
      gLayer[I]      = T;
      gLayerPitch[I] = Tp;
      break;
    }
  }

  if ((gLayerCount == 0) || (gLayer[0] != gFb) || (gLayerPitch[0] != gFbPitch)) {
    gOwnWhy = 2;
    return;
  }

  if (!ProbeMutex ()) {
    gOwnWhy = 3;
    return;
  }

  //
  // The key latches only fire when KeypadDeviceConstructor enabled them
  // (PMIC 0x90F |= key IRQs); without that the power key would be invisible.
  //
  if (!PmicRead (PMIC_KEY_STATUS, &V) || !PmicRead (PMIC_KEY_ENABLE, &V) ||
      ((V & PMIC_KEY_IRQS) != PMIC_KEY_IRQS))
  {
    gOwnWhy = 4;
    return;
  }

  gOwnWhy = 0;
}

//
// What Refresh() does after its mirror copy: arm the mutex group, then
// DSI_START 0 -> 1. At most once per PUSH_MIN_US unless forced.
//
static void
OwnPush (
  int  Force
  )
{
  UINT64  Now;
  UINT32  B;

  if (!gNeedPush || (gMode != MODE_OWN)) {
    return;
  }

  Now = NowUs ();
  if (!Force && (Now - gLastPushUs < PUSH_MIN_US)) {
    return;
  }

  __asm__ volatile ("dsb sy" ::: "memory");
  for (B = 0; B < 4; B++) {
    if ((gMutexMask & (1U << B)) != 0) {
      Wr32 (kMutexBanks[B] + MUTEX_EN (gMutexId), 1);
    }
  }

  Wr32 (DSI0_BASE + DSI_START, 0);
  __asm__ volatile ("dsb sy" ::: "memory");
  Wr32 (DSI0_BASE + DSI_START, 1);
  __asm__ volatile ("dsb sy" ::: "memory");
  gNeedPush   = 0;
  gLastPushUs = Now;
}

//
// Entering OWN: hold the firmware callbacks back for good, take over the
// keys (current volume-up level, PMIC latches drained).
//
static void
OwnEnter (
  void
  )
{
  UINT8  St;

  Gate ();
  gUpWas     = (Rd32 (GPIO_VOLUP_DIN) & GPIO_VOLUP_BIT) == 0;
  gUpSince   = NowMs ();
  gPmicFails = 0;
  if (PmicRead (PMIC_KEY_STATUS, &St) && ((St & PMIC_KEY_IRQS) != 0)) {
    PmicWrite (PMIC_KEY_STATUS, St & PMIC_KEY_IRQS);
  }
}

//
// Keys in OWN. Same result encoding as PollKeys; -2 = PMIC lost, leave OWN.
//
static int
DirectKeys (
  UINT64  Now,
  UINT64  KeyFrom
  )
{
  int    Result = 0;
  int    Up;
  UINT8  St;

  Up = (Rd32 (GPIO_VOLUP_DIN) & GPIO_VOLUP_BIT) == 0;
  if (Up && !gUpWas) {
    gUpSince = Now;
    if (Now >= KeyFrom) {
      Result ^= 1;
    }
  }

  gUpWas = Up;
  if (Up && (Now >= KeyFrom) && (Now - gUpSince >= UP_HOLD_QUIT_MS)) {
    return -1;
  }
  if (!PmicRead (PMIC_KEY_STATUS, &St)) {
    return (++gPmicFails >= 5) ? -2 : Result;
  }

  gPmicFails = 0;
  if ((St & PMIC_KEY_IRQS) != 0) {
    PmicWrite (PMIC_KEY_STATUS, St & PMIC_KEY_IRQS);
  }

  if (Now < KeyFrom) {
    return 0;
  }

  if ((St & 1U) != 0) {
    return -1;
  }

  if ((St & 2U) != 0) {
    Result ^= 2;
  }

  return Result;
}

//
// One rectangle to the screen through the current mode.
//
static void
OutRect (
  const UINT32  *Buf,
  UINT32        BufW,
  UINT32        X,
  UINT32        Y,
  UINT32        W,
  UINT32        H
  )
{
  UINT32  L;
  UINT32  Layers;
  UINT32  R;
  UINT32  C;

  if ((W == 0) || (H == 0)) {
    return;
  }

  if ((gMode < MODE_DIRECT) || (gFb == NULL)) {
    gGop->Blt (gGop, (UINT32 *)Buf, EfiBltBufferToVideo, 0, 0, X, Y, W, H, (UINTN)BufW * sizeof (UINT32));
    return;
  }

  Layers = (gMode == MODE_OWN) ? gLayerCount : 1;
  for (L = 0; L < Layers; L++) {
    UINT32  *Base  = (gMode == MODE_OWN) ? gLayer[L] : gFb;
    UINT32  Pitch  = (gMode == MODE_OWN) ? gLayerPitch[L] : gFbPitch;

    for (R = 0; R < H; R++) {
      const UINT32  *Src = Buf + (UINTN)R * BufW;
      UINT32        *Dst = Base + (UINTN)(Y + R) * Pitch + X;

      for (C = 0; C < W; C++) {
        Dst[C] = Src[C] | 0xFF000000U;     // stores only, never read the frame buffer
      }

      CleanRange (Dst, (UINTN)W * sizeof (UINT32));
    }
  }

  __asm__ volatile ("dsb sy" ::: "memory");
  if (gMode == MODE_OWN) {
    gNeedPush = 1;
  }
}

//
// Black screen: the GOP fill for the frame buffer, and in OWN the other
// layers too (the firmware mirror that would copy it is held back).
//
static void
OutClear (
  void
  )
{
  UINT32  L;
  UINT32  R;
  UINT32  C;

  ClearScreen ();
  if (gMode != MODE_OWN) {
    return;
  }

  for (L = 1; L < gLayerCount; L++) {
    for (R = 0; R < gH; R++) {
      UINT32  *Dst = gLayer[L] + (UINTN)R * gLayerPitch[L];

      for (C = 0; C < gW; C++) {
        Dst[C] = 0xFF000000U;
      }

      CleanRange (Dst, (UINTN)gW * sizeof (UINT32));
    }
  }

  __asm__ volatile ("dsb sy" ::: "memory");
  gNeedPush = 1;
}

// ---------------------------------------------------------------------------
// Video
// ---------------------------------------------------------------------------

//
// Dirty tracking: the source plane is cut into tiles of TILE_W x TILE_H
// pixels; a tile is dirty when any of its pixels differs from the plane last
// drawn. Per band of TILE_H rows the dirty tiles form runs (single clean
// tiles between dirty ones are absorbed), each run is one rectangle, and a
// band whose single run matches the band above extends that rectangle. Only
// those rectangles are upscaled and sent to the screen.
//
#define TILE_W     16U
#define TILE_H     8U
#define MAX_BANDS  64U

static UINT64  gBandMask[MAX_BANDS];

//
// Bilinear upscale of source rectangle [X0..X1] x [Y0..Y1] (inclusive) and
// out. Screen row Oy reads source rows gYMap[Oy] and gYMap[Oy] + 1 (gYMap is
// non-decreasing), so the rows to redo are those with gYMap in [Y0 - 1, Y1];
// the same for columns. The pixel right of the last column and the row below
// the last row are the zero padding of the plane, with weight 0 there.
//
static void
RenderRect (
  const UINT8  *Plane,
  INT64        S,
  UINT32       X0,
  UINT32       X1,
  UINT32       Y0,
  UINT32       Y1
  )
{
  UINT32  OyLo;
  UINT32  OyHi;
  UINT32  OxLo;
  UINT32  OxHi;
  UINT32  Oy;
  UINT32  Ox;
  UINT64  T0 = NowUs ();

  for (OyLo = 0; (OyLo < gVH) && (gYMap[OyLo] + 1 < Y0); OyLo++) {
  }

  for (OxLo = 0; (OxLo < gVW) && (gXMap[OxLo] + 1 < X0); OxLo++) {
  }

  if ((OyLo >= gVH) || (OxLo >= gVW)) {
    return;
  }

  for (OyHi = OyLo; (OyHi + 1 < gVH) && (gYMap[OyHi + 1] <= Y1); OyHi++) {
  }

  for (OxHi = OxLo; (OxHi + 1 < gVW) && (gXMap[OxHi + 1] <= X1); OxHi++) {
  }

  for (Oy = OyLo; Oy <= OyHi; Oy++) {
    const UINT8  *R0   = BaPlaneRow ((UINT8 *)Plane, S, gYMap[Oy]);
    const UINT8  *R1   = R0 + S;
    INT32        Wy    = (INT32)gYW[Oy];
    UINT32       *Line = gVideo + (UINTN)Oy * gVW;

    if ((Oy != OyLo) && (gYMap[Oy] == gYMap[Oy - 1]) && (gYW[Oy] == gYW[Oy - 1])) {
      UINT32  *Prev = Line - gVW;

      for (Ox = OxLo; Ox <= OxHi; Ox++) {
        Line[Ox] = Prev[Ox];
      }

      continue;
    }

    for (Ox = OxLo; Ox <= OxHi; Ox++) {
      UINT32  X = gXMap[Ox];
      INT32   A = R0[X];
      INT32   B = R0[X + 1];
      INT32   C = R1[X];
      INT32   D = R1[X + 1];
      INT32   Wx;
      INT32   Top;
      INT32   Bot;

      if ((A == B) && (A == C) && (A == D)) {
        Line[Ox] = A ? COLOR_WHITE : COLOR_BLACK;
        continue;
      }

      Wx  = (INT32)gXW[Ox];
      Top = (A << 8) + (B - A) * Wx;            // 0..256
      Bot = (C << 8) + (D - C) * Wx;
      Line[Ox] = gGray[((Top << 8) + (Bot - Top) * Wy + 128) >> 8];
    }
  }

  gRDraw   += NowUs () - T0;
  gRPixels += (UINT64)(OxHi - OxLo + 1) * (OyHi - OyLo + 1);
  OutRect (gVideo + (UINTN)OyLo * gVW + OxLo, gVW, gVX + OxLo, gVY + OyLo, OxHi - OxLo + 1, OyHi - OyLo + 1);
}

static void
RenderFrame (
  void
  )
{
  const UINT8  *Plane = gDec.Plane[gDec.Latest];
  INT64        S      = gDec.Stride;
  UINT32       H      = gDec.Info.Height;
  UINT32       W      = gDec.Info.Width;
  UINT32       Chunks = (W + TILE_W - 1) / TILE_W;
  UINT32       Bands  = (H + TILE_H - 1) / TILE_H;
  UINT64       All    = (Chunks >= 64) ? ~0ULL : ((1ULL << Chunks) - 1);
  UINT32       Y;
  UINT32       B;
  UINT32       C;
  int          Pending = 0;
  UINT32       PX0     = 0;
  UINT32       PX1     = 0;
  UINT32       PY0     = 0;
  UINT32       PY1     = 0;

  for (B = 0; B < Bands; B++) {
    gBandMask[B] = 0;
  }

  for (Y = 0; Y < H; Y++) {
    const UINT8  *A  = BaPlaneRow ((UINT8 *)Plane, S, Y);
    UINT8        *Sh = BaPlaneRow (gShown, S, Y);
    UINT64       M   = 0;

    if (!gShownValid) {
      M = All;
    } else if (!BaBytesEqual (A, Sh, W)) {
      for (C = 0; C < Chunks; C++) {
        UINT32  X0 = C * TILE_W;
        UINT32  N  = (W - X0 < TILE_W) ? W - X0 : TILE_W;

        if (!BaBytesEqual (A + X0, Sh + X0, N)) {
          M |= 1ULL << C;
        }
      }
    }

    if (M != 0) {
      BaCopyBytes (Sh, A, W);
      gBandMask[Y / TILE_H] |= M;
    }
  }

  gShownValid = 1;

  for (B = 0; B < Bands; B++) {
    UINT64  M  = gBandMask[B];
    UINT32  Y0 = B * TILE_H;
    UINT32  Y1 = ((Y0 + TILE_H < H) ? Y0 + TILE_H : H) - 1;
    UINT32  RunLo[32];
    UINT32  RunHi[32];
    UINT32  Runs = 0;
    UINT32  R;

    M |= (M >> 1) & (M << 1);     // absorb single clean tiles between dirty ones
    while ((M != 0) && (Runs < 32)) {
      UINT32  Lo  = (UINT32)__builtin_ctzll (M);
      UINT64  Sh  = M >> Lo;
      UINT32  Len = (~Sh == 0) ? 64 - Lo : (UINT32)__builtin_ctzll (~Sh);

      RunLo[Runs] = Lo * TILE_W;
      RunHi[Runs] = (((Lo + Len) * TILE_W < W) ? (Lo + Len) * TILE_W : W) - 1;
      Runs++;
      M &= ~(((Len >= 64) ? ~0ULL : ((1ULL << Len) - 1)) << Lo);
    }

    if (M != 0) {               // more than 32 runs: one rectangle for the rest
      RunHi[Runs - 1] = W - 1;
    }

    if ((Runs == 1) && Pending && (RunLo[0] == PX0) && (RunHi[0] == PX1) && (Y0 == PY1 + 1)) {
      PY1 = Y1;
      continue;
    }

    if (Pending) {
      RenderRect (Plane, S, PX0, PX1, PY0, PY1);
      Pending = 0;
    }

    if (Runs == 1) {
      PX0     = RunLo[0];
      PX1     = RunHi[0];
      PY0     = Y0;
      PY1     = Y1;
      Pending = 1;
      continue;
    }

    for (R = 0; R < Runs; R++) {
      RenderRect (Plane, S, RunLo[R], RunHi[R], Y0, Y1);
    }
  }

  if (Pending) {
    RenderRect (Plane, S, PX0, PX1, PY0, PY1);
  }
}

static void
DrawTitle (
  void
  )
{
  if (gTitleH == 0) {
    return;
  }

  Fill (gInfo, gW, 0, 0, gW, gTitleH, COLOR_BLACK);
  TextCentered (gInfo, gW, 0, "BAD APPLE!!", gTitleScale, COLOR_WHITE);
  OutRect (gInfo, gW, 0, gTitleY, gW, gTitleH);
}

static char *
AppendMs (
  char    *Out,
  UINT64  Us
  )
{
  UINT64  Tenths = (Us + 50) / 100;

  Out    = AppendUint (Out, (UINT32)(Tenths / 10));
  *Out++ = '.';
  *Out++ = (char)('0' + Tenths % 10);
  *Out   = '\0';
  return Out;
}

static void
DrawInfo (
  UINT32  Frame,
  int     Paused,
  int     Ended
  )
{
  char    Line[64];
  char    Stats[96];
  char    Hint[64];
  char    *P;
  UINT32  Lh;
  UINT32  BarY;
  UINT32  BarH;
  UINT32  StatsY;
  UINT32  Filled;
  UINT32  Total = gDec.Info.Frames;

  if (gInfoH == 0) {
    return;
  }

  Lh     = ANIM_GLYPH_H * gTextScale;
  BarH   = gTextScale * 4;
  BarY   = Lh + Lh / 2;
  StatsY = BarY + BarH + Lh / 2;
  Fill (gInfo, gW, 0, 0, gW, gInfoH, COLOR_BLACK);

  P = Line;
  if (Ended) {
    P = Append (P, "THE END");
  } else {
    P = AppendTime (P, Frame / gDec.Info.Fps);
    P = Append (P, " / ");
    P = AppendTime (P, Total / gDec.Info.Fps);
    if (!Paused && (gShownFps != 0)) {
      P = Append (P, "   ");
      P = AppendUint (P, gShownFps);
      P = Append (P, "/");
      P = AppendUint (P, gDec.Info.Fps);
      P = Append (P, " FPS");
    }
    if (Paused) {
      P = Append (P, "  PAUSED");
    }
  }

  TextCentered (gInfo, gW, 0, Line, gTextScale, Paused ? COLOR_BAR : COLOR_TEXT);

  Filled = (Total > 1) ? (UINT32)(((UINT64)gVW * (Ended ? Total : Frame)) / Total) : gVW;
  if (Filled > gVW) {
    Filled = gVW;
  }

  Fill (gInfo, gW, gVX, BarY, gVW, BarH, COLOR_TRACK);
  Fill (gInfo, gW, gVX, BarY, Filled, BarH, COLOR_BAR);

  //
  // Milliseconds per presented frame: decode, draw (upscale), out (blt or
  // store + clean, plus this panel), FW (time taken by firmware callbacks).
  //
  P = Append (Stats, ModeName (gMode));
  if (gShownFps != 0) {
    P = Append (P, " DEC ");
    P = AppendMs (P, gAvgDec);
    P = Append (P, " DRW ");
    P = AppendMs (P, gAvgDraw);
    P = Append (P, " OUT ");
    P = AppendMs (P, gAvgOut);
    P = Append (P, " FW ");
    P = AppendMs (P, gAvgFw);
    P = Append (P, " PX ");
    P = AppendUint (P, gAvgPx);
    P = Append (P, "%");
  }

  P = Append (Hint, "POWER: BACK  VOL+: PAUSE  VOL-: MODE");
  if (gOwnWhy != 0) {
    P = Append (P, "  NO-OWN ");
    P = AppendUint (P, gOwnWhy);
  }

  TextCentered (gInfo, gW, StatsY, Stats, gTextScale, COLOR_TEXT);
  TextCentered (gInfo, gW, StatsY + Lh + Lh / 2, Hint, gTextScale, COLOR_DIM);
  OutRect (gInfo, gW, 0, gInfoY, gW, gInfoH);
}

// ---------------------------------------------------------------------------
// Input and options
// ---------------------------------------------------------------------------

static UINT32
OptionValue (
  const CHAR16  *Options,
  UINTN         Chars,
  const char    *Name,
  UINT32        Default
  )
{
  UINTN  I;
  UINTN  NameLen = AsciiLen (Name);

  if (Options == NULL) {
    return Default;
  }

  for (I = 0; I + NameLen < Chars; I++) {
    UINTN   K;
    UINT32  V;
    int     Digits;

    if ((I != 0) && (Options[I - 1] != L' ')) {
      continue;
    }

    for (K = 0; K < NameLen; K++) {
      if (Options[I + K] != (CHAR16)Name[K]) {
        break;
      }
    }

    if ((K != NameLen) || (Options[I + K] != L'=')) {
      continue;
    }

    V      = 0;
    Digits = 0;
    for (K = I + NameLen + 1; (K < Chars) && (Options[K] >= L'0') && (Options[K] <= L'9'); K++) {
      V = V * 10 + (UINT32)(Options[K] - L'0');
      Digits++;
      if (V > 100000) {
        break;
      }
    }

    return Digits ? V : Default;
  }

  return Default;
}

//
// Bit 0 = toggle pause (volume up), bit 1 = next output mode (volume down),
// -1 = quit (power or any other key).
//
static int
PollKeys (
  UINT64  Now,
  UINT64  KeyFrom
  )
{
  EFI_INPUT_KEY  Key;
  int            Result = 0;

  if ((gST->ConIn == NULL) || (gST->ConIn->ReadKeyStroke == NULL)) {
    return 0;
  }

  while (!EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key))) {
    if (Now < KeyFrom) {
      continue;     // drain the key that started us
    }

    if (Key.ScanCode == SCAN_UP) {
      Result ^= 1;
    } else if (Key.ScanCode == SCAN_DOWN) {
      Result ^= 2;
    } else {
      return -1;
    }
  }

  return Result;
}

// ---------------------------------------------------------------------------
// Layout and memory
// ---------------------------------------------------------------------------

static UINTN
AlignUp (
  UINTN  V
  )
{
  return (V + 63) & ~(UINTN)63;
}

static EFI_STATUS
Layout (
  void
  )
{
  UINT32  W = gDec.Info.Width;
  UINT32  H = gDec.Info.Height;
  UINT32  Lh;
  UINT32  Reserve;
  UINT64  ScaleW;
  UINT64  ScaleH;
  UINT64  Scale;
  UINTN   PlaneBytes;
  UINTN   Bytes;
  UINT8   *Base;
  UINT32  I;

  if ((W > TILE_W * 64) || (H > TILE_H * MAX_BANDS)) {
    return EFI_UNSUPPORTED;
  }

  gTextScale  = (gW >= 1000) ? 3 : ((gW >= 600) ? 2 : 1);
  gTitleScale = gTextScale + 1;
  Lh          = ANIM_GLYPH_H * gTextScale;
  gTitleH     = ANIM_GLYPH_H * gTitleScale;
  gInfoH      = Lh + Lh / 2 + gTextScale * 4 + Lh / 2 + Lh + Lh / 2 + Lh;

  //
  // Room for the title above and the info panel below, if the screen has it.
  //
  Reserve = gInfoH + Lh;
  if (gH < H + 2 * Reserve) {
    Reserve = 0;
    gInfoH  = 0;
    gTitleH = 0;
  }

  ScaleW = ((UINT64)gW << 16) / W;
  ScaleH = ((UINT64)(gH - 2 * Reserve) << 16) / H;
  Scale  = (ScaleW < ScaleH) ? ScaleW : ScaleH;
  gVW    = (UINT32)(((UINT64)W * Scale) >> 16);
  gVH    = (UINT32)(((UINT64)H * Scale) >> 16);
  if (gVW > gW) {
    gVW = gW;
  }

  if (gVH > gH) {
    gVH = gH;
  }

  if ((gVW == 0) || (gVH == 0)) {
    return EFI_UNSUPPORTED;
  }

  gVX = (gW - gVW) / 2;
  gVY = (gH - gVH) / 2;
  if (gInfoH != 0) {
    gInfoY  = gVY + gVH + Lh;
    gTitleY = (gVY > gTitleH + Lh) ? gVY - gTitleH - Lh : 0;
  }

  PlaneBytes = (UINTN)BaPlaneBytes (&gDec.Info);
  Bytes      = AlignUp (PlaneBytes) * 3
               + AlignUp ((UINTN)gVW * gVH * sizeof (UINT32))
               + AlignUp ((UINTN)gW * ((gInfoH > gTitleH) ? gInfoH : gTitleH) * sizeof (UINT32))
               + AlignUp ((UINTN)gVW * sizeof (UINT32))
               + AlignUp ((UINTN)gVW * sizeof (UINT32))
               + AlignUp ((UINTN)gVH * sizeof (UINT32))
               + AlignUp ((UINTN)gVH * sizeof (UINT32));
  if (EFI_ERROR (gBS->AllocatePool (EfiLoaderData, Bytes + 64, &gPool)) || (gPool == NULL)) {
    gPool = NULL;
    return EFI_OUT_OF_RESOURCES;
  }

  Base            = (UINT8 *)(((UINTN)gPool + 63) & ~(UINTN)63);
  gDec.Plane[0]   = Base;
  Base           += AlignUp (PlaneBytes);
  gDec.Plane[1]   = Base;
  Base           += AlignUp (PlaneBytes);
  gShown          = Base;
  Base           += AlignUp (PlaneBytes);
  gVideo          = (UINT32 *)Base;
  Base           += AlignUp ((UINTN)gVW * gVH * sizeof (UINT32));
  gInfo           = (UINT32 *)Base;
  Base           += AlignUp ((UINTN)gW * ((gInfoH > gTitleH) ? gInfoH : gTitleH) * sizeof (UINT32));
  gXMap           = (UINT32 *)Base;
  Base           += AlignUp ((UINTN)gVW * sizeof (UINT32));
  gXW             = (UINT32 *)Base;
  Base           += AlignUp ((UINTN)gVW * sizeof (UINT32));
  gYMap           = (UINT32 *)Base;
  Base           += AlignUp ((UINTN)gVH * sizeof (UINT32));
  gYW             = (UINT32 *)Base;

  //
  // Pixel centres: source position = (o + 0.5) * src / dst - 0.5, in 1/256.
  // At the far edge the weight of the next pixel (the zero padding) is 0.
  //
  for (I = 0; I < gVW; I++) {
    INT64  Q = (INT64)((((UINT64)(2 * I + 1) * W) << 8) / (2ULL * gVW)) - 128;

    if (Q < 0) {
      Q = 0;
    }

    gXMap[I] = (UINT32)(Q >> 8);
    gXW[I]   = (UINT32)(Q & 255);
    if (gXMap[I] >= W - 1) {
      gXMap[I] = W - 1;
      gXW[I]   = 0;
    }
  }

  for (I = 0; I < gVH; I++) {
    INT64  Q = (INT64)((((UINT64)(2 * I + 1) * H) << 8) / (2ULL * gVH)) - 128;

    if (Q < 0) {
      Q = 0;
    }

    gYMap[I] = (UINT32)(Q >> 8);
    gYW[I]   = (UINT32)(Q & 255);
    if (gYMap[I] >= H - 1) {
      gYMap[I] = H - 1;
      gYW[I]   = 0;
    }
  }

  for (I = 0; I <= 256; I++) {
    UINT32  G = (I * 255 + 128) / 256;

    gGray[I] = (G << 16) | (G << 8) | G;
  }

  return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

//
// Statistics window: one second of presented frames.
//
typedef struct {
  UINT64  From;
  UINT64  Dec;
  UINT64  Draw;
  UINT64  Out;
  UINT64  Sleep;
  UINT64  Px;
  UINT32  Frames;
} PLAY_STATS;

static void
StatsReset (
  PLAY_STATS  *St
  )
{
  St->From   = NowUs ();
  St->Dec    = 0;
  St->Draw   = 0;
  St->Out    = 0;
  St->Sleep  = 0;
  St->Px     = 0;
  St->Frames = 0;
}

//
// Full redraw through the (new) current mode.
//
static void
Redraw (
  UINT32  Frame,
  int     Paused
  )
{
  if (gMode == MODE_OWN) {
    OwnEnter ();
  }

  OutClear ();
  DrawTitle ();
  DrawInfo (Frame, Paused, 0);
  gShownValid = 0;
  OwnPush (1);
}

static EFI_STATUS
Play (
  UINT32  Loop
  )
{
  UINT64      KeyFrom  = NowMs () + KEY_GRACE_MS;
  UINT64      Start;
  UINT64      PausedAt = 0;
  UINT64      LastInfo = 0;
  UINT32      Fps      = gDec.Info.Fps;
  UINT32      Frames   = gDec.Info.Frames;
  int         Paused   = 0;
  int         Drawn    = 0;
  PLAY_STATS  St;

  BaDecoderReset (&gDec, gStream);
  BaFillBytes (gShown, BaPlaneBytes (&gDec.Info), 0);
  gShownFps = 0;
  Redraw (0, 0);
  Start = NowMs ();
  StatsReset (&St);

  for ( ; ;) {
    UINT64  Now;
    UINT64  Target;
    UINT64  Next;
    UINT64  T0;
    UINT64  T1;
    UINT64  T2;
    int     Changed = 0;
    int     Key;

    //
    // SYNC / DIRECT: the one window per frame at TPL_APPLICATION, where the
    // display refresh and the keypad scan that were held back run. OWN keeps
    // them held back and pushes the frame itself.
    //
    if (gMode != MODE_OWN) {
      Ungate ();
    } else {
      OwnPush (0);
    }

    Now = NowMs ();
    MaybeFeedWatchdog (Now);
    Key = (gMode == MODE_OWN) ? DirectKeys (Now, KeyFrom) : PollKeys (Now, KeyFrom);
    if (Key == -1) {
      return EFI_SUCCESS;
    }

    if (Key == -2) {          // PMIC key latch lost: give the keys back to the firmware
      gOwnWhy = 5;
      gMode   = MODE_DIRECT;
      Key     = 0;
      Redraw (gDec.Decoded ? gDec.Decoded - 1 : 0, Paused);
      Drawn = 0;
      continue;
    }

    if ((Key & 2) != 0) {
      gMode     = NextMode (gMode);
      gShownFps = 0;
      Redraw (gDec.Decoded ? gDec.Decoded - 1 : 0, Paused);
      Drawn = 0;
      StatsReset (&St);
      continue;
    }

    if ((Key & 1) != 0) {
      if (Paused) {
        Start += Now - PausedAt;   // resume where it stopped
      } else {
        PausedAt = Now;
      }

      Paused = !Paused;
      DrawInfo (gDec.Decoded ? gDec.Decoded - 1 : 0, Paused, 0);
    }

    if (Paused) {
      OwnPush (0);
      SleepMs (20);
      StatsReset (&St);
      continue;
    }

    Target = ((Now - Start) * Fps) / 1000;
    if (Target >= Frames) {
      DrawInfo (Frames, 0, 1);
      OwnPush (1);
      WaitMs (END_MS);
      if (!Loop) {
        return EFI_SUCCESS;
      }

      BaDecoderReset (&gDec, gStream);
      gShownValid = 0;
      Drawn       = 0;
      Start       = NowMs ();
      gShownFps   = 0;
      StatsReset (&St);
      continue;
    }

    Gate ();

    //
    // Every frame has to be decoded (each is coded against the previous
    // one); only the newest is drawn.
    //
    T0 = NowUs ();
    {
      UINT32  Before = gDec.Decoded;

      while (gDec.Decoded <= Target) {
        Changed |= BaDecodeFrame (&gDec);
        MaybeFeedWatchdog (NowMs ());
      }

      if (gDec.Decoded != Before) {
        St.Frames++;   // one presented frame slot; skipped-over frames do not count
      }
    }

    T1       = NowUs ();
    gRDraw   = 0;
    gRPixels = 0;
    if (Changed || !Drawn) {
      RenderFrame ();
      Drawn = 1;
    }

    Now = NowMs ();
    if (Now - LastInfo >= INFO_MS) {
      DrawInfo (gDec.Decoded - 1, 0, 0);
      LastInfo = Now;
    }

    OwnPush (0);
    T2        = NowUs ();
    St.Dec   += T1 - T0;
    St.Draw  += gRDraw;
    St.Out   += (T2 - T1) - gRDraw;
    St.Px    += gRPixels;

    if (T2 - St.From >= 1000000) {
      UINT64  Wall = T2 - St.From;
      UINT64  Own  = St.Dec + St.Draw + St.Out + St.Sleep;
      UINT32  N    = St.Frames ? St.Frames : 1;

      gShownFps = (UINT32)(((UINT64)St.Frames * 1000000) / Wall);
      gAvgDec   = St.Dec / N;
      gAvgDraw  = St.Draw / N;
      gAvgOut   = St.Out / N;
      gAvgFw    = (Wall > Own) ? (Wall - Own) / N : 0;
      gAvgPx    = (UINT32)((St.Px * 100) / ((UINT64)N * gVW * gVH));
      StatsReset (&St);
      St.From = T2;
    }

    //
    // Wait for the next frame still gated, so SYNC / DIRECT let the refresh
    // run once per frame (at the top of the loop) and not every 20 ms.
    //
    Next = Start + ((UINT64)gDec.Decoded * 1000 + Fps - 1) / Fps;
    Now  = NowMs ();
    if (Next > Now) {
      UINT64  S0 = NowUs ();

      SleepMs ((Next - Now) > 20 ? 20 : (Next - Now));
      St.Sleep += NowUs () - S0;
    }
  }
}

EFI_STATUS
efi_main (
  EFI_HANDLE        ImageHandle,
  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_LOADED_IMAGE_PROTOCOL  *Loaded;
  const CHAR16               *Options = NULL;
  UINTN                      OptionChars = 0;
  UINTN                      BlobBytes;
  UINT32                     Loop;
  int                        Rc;
  EFI_STATUS                 Status;

  gST = SystemTable;
  gBS = SystemTable->BootServices;

  FeedWatchdog ();
  gBS->SetWatchdogTimer (0, 0, 0, NULL);

  gFreq     = ReadFrequency ();
  gT0       = ReadCounter ();
  gLastFeed = 0;

  if (!EFI_ERROR (gBS->HandleProtocol (ImageHandle, &mLoadedImageGuid, (void **)&Loaded)) &&
      (Loaded->LoadOptions != NULL) && (Loaded->LoadOptionsSize >= sizeof (CHAR16)))
  {
    Options     = (const CHAR16 *)Loaded->LoadOptions;
    OptionChars = Loaded->LoadOptionsSize / sizeof (CHAR16);
  }

  Loop  = OptionValue (Options, OptionChars, "loop", 0);
  gMode = OptionValue (Options, OptionChars, "mode", MODE_OWN);
  if (gMode >= MODE_COUNT) {
    gMode = MODE_OWN;
  }

  //
  // Same lookup order as the UniXNU menu.
  //
  gGop = NULL;
  if ((gST->ConsoleOutHandle == NULL) ||
      EFI_ERROR (gBS->HandleProtocol (gST->ConsoleOutHandle, &mGopGuid, (void **)&gGop)))
  {
    gGop = NULL;
  }

  if ((gGop == NULL) && EFI_ERROR (gBS->LocateProtocol (&mGopGuid, NULL, (void **)&gGop))) {
    gGop = NULL;
  }

  if ((gGop == NULL) || (gGop->Mode == NULL) || (gGop->Mode->Info == NULL)) {
    if ((gST->ConOut != NULL) && (gST->ConOut->OutputString != NULL)) {
      gST->ConOut->OutputString (gST->ConOut, (CHAR16 *)L"BadApple: no Graphics Output Protocol\r\n");
    }

    return EFI_UNSUPPORTED;
  }

  gW = gGop->Mode->Info->HorizontalResolution;
  gH = gGop->Mode->Info->VerticalResolution;
  if ((gW < 160) || (gH < 120)) {
    return EFI_UNSUPPORTED;
  }

  BlobBytes = (UINTN)(gBaDataEnd - gBaData);
  Rc        = BaParseHeader (gBaData, BlobBytes, &gDec.Info);
  if (Rc == BA_OK) {
    Rc = BaCheckStream (gBaData, &gDec.Info);
  }

  if (Rc != BA_OK) {
    char  Code[16];

    Code[0] = 'E';
    Code[1] = 'R';
    Code[2] = 'R';
    Code[3] = ' ';
    AppendUint (Code + 4, (UINT32)(-Rc));
    ShowMessage ("BAD APPLE: STREAM REJECTED", Code, "returning to the menu", 2);
    WaitMs (ERROR_MS);
    return EFI_CRC_ERROR;
  }

  gStream = gBaData + BA_HEADER_BYTES;
  Status  = Layout ();
  if (EFI_ERROR (Status)) {
    ShowMessage ("BAD APPLE: NO MEMORY", NULL, "returning to the menu", 2);
    WaitMs (ERROR_MS);
    return Status;
  }

  InitDirect ();
  InitOwn ();
  while (!ModeAvailable (gMode)) {
    gMode--;                  // GOP (0) is always there
  }

  Status = Play (Loop);
  Ungate ();                  // the held-back refresh mirrors and pushes what we drew

  gBS->FreePool (gPool);
  gPool = NULL;
  return Status;
}
