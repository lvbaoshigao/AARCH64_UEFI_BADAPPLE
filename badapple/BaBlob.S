/*
 * The "RBA1" stream played by BadApple.c. BA_DATA_PATH is passed by
 * build-badapple-efi.sh as a quoted absolute path.
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
  .section .rodata.badapple_stream, "a", %progbits
  .balign 16
  .global gBaData
  .hidden gBaData
gBaData:
  .incbin BA_DATA_PATH
  .global gBaDataEnd
  .hidden gBaDataEnd
gBaDataEnd:
  .byte 0
  .section .note.GNU-stack, "", %progbits
