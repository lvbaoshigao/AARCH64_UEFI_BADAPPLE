"""Host check of RothkoAnim.efi: PE32+ AArch64 UEFI application, entry at efi_main.

python check-pe.py RothkoAnim.efi RothkoAnim.map   (exit 0 = ok, prints a JSON report)
"""
import hashlib
import json
import re
import struct
import sys
from pathlib import Path


def check(efi_path, map_path):
    efi = Path(efi_path).read_bytes()
    report = {'efi_bytes': len(efi), 'efi_sha256': hashlib.sha256(efi).hexdigest()}
    problems = []
    if efi[:2] != b'MZ':
        raise SystemExit('not an MZ image')
    pe = struct.unpack_from('<I', efi, 0x3C)[0]
    if efi[pe:pe + 4] != b'PE\0\0':
        raise SystemExit('no PE signature')
    machine, nsect, _, _, _, opt_size, chars = struct.unpack_from('<HHIIIHH', efi, pe + 4)
    opt = pe + 24
    magic = struct.unpack_from('<H', efi, opt)[0]
    entry = struct.unpack_from('<I', efi, opt + 16)[0]
    sect_align, file_align = struct.unpack_from('<II', efi, opt + 32)
    image_size = struct.unpack_from('<I', efi, opt + 56)[0]
    subsystem = struct.unpack_from('<H', efi, opt + 68)[0]
    n_dirs = struct.unpack_from('<I', efi, opt + 108)[0]
    reloc_rva, reloc_size = struct.unpack_from('<II', efi, opt + 112 + 5 * 8) if n_dirs > 5 else (0, 0)
    sections = []
    for i in range(nsect):
        o = opt + opt_size + 40 * i
        name = efi[o:o + 8].rstrip(b'\0').decode('ascii', 'replace')
        vsize, vaddr, rsize, rptr = struct.unpack_from('<IIII', efi, o + 8)
        flags = struct.unpack_from('<I', efi, o + 36)[0]
        sections.append({'name': name, 'vaddr': hex(vaddr), 'vsize': vsize, 'raw': rsize, 'flags': hex(flags)})
    report.update(machine=hex(machine), magic=hex(magic), subsystem=subsystem, entry=hex(entry),
                  characteristics=hex(chars), section_alignment=hex(sect_align), file_alignment=hex(file_align),
                  image_size=image_size, reloc_dir={'rva': hex(reloc_rva), 'size': reloc_size}, sections=sections)
    if machine != 0xAA64:
        problems.append('machine is not AArch64 (0xAA64)')
    if magic != 0x20B:
        problems.append('not PE32+')
    if subsystem != 10:
        problems.append('subsystem is not EFI_APPLICATION (10)')
    if sect_align < 0x1000:
        problems.append('section alignment below 4 KiB (memory protection needs 4 KiB)')
    if chars & 0x0001:
        problems.append('RELOCS_STRIPPED set: DxeCore would need the link address')
    # The ELF is linked at 0, so the PE entry RVA equals the efi_main address in the map.
    text = Path(map_path).read_text(encoding='utf-8', errors='replace')
    m = re.search(r'^\s*([0-9a-f]+)\s+[0-9a-f]+\s+[0-9a-f]+\s+\d+\s+efi_main\s*$', text, re.M)
    report['map_efi_main'] = hex(int(m.group(1), 16)) if m else None
    if not m or int(m.group(1), 16) != entry:
        problems.append('PE entry does not match efi_main in the link map')
    report['problems'] = problems
    return report


if __name__ == '__main__':
    result = check(sys.argv[1], sys.argv[2])
    print(json.dumps(result, indent=2))
    sys.exit(1 if result['problems'] else 0)
