#!/usr/bin/env python3
"""Animation candidate FAT: "Animation" (RothkoAnim.efi, default), "Show image" (ImageView.efi),
"Bad Apple!!" (BadApple.efi, ../badapple, with its embedded 1-bit stream),
"UEFI Boot Manager" (the firmware's own BootManagerMenuApp, loaded from the FV by file GUID),
the unchanged v4 XNU entry, "Reboot to fastboot" (RebootFastboot.efi: TOPRGU+0x24 reboot-mode = 3,
as Android "reboot bootloader"; asks for the power key first) and Reboot.

The Boot Manager entry adds no file: "fv:<GUID>" is resolved by UniXNULib Volume.c ResolveFvFile
against the FV, and EEC25BDC-... is PcdBootManagerMenuFile (BootManagerMenuApp FILE_GUID) in the v4 FD.

BootEntries.efi runs as a UniXNU [[driver]] before the menu and registers "Rothko Image",
"Rothko Animation" and "Rothko Bad Apple" as Boot#### options (category APP) pointing at the
Tools/*.efi files, so the firmware Boot Manager lists them next to "UniXNU".

Same transport as ../image/make-image-payload.py (UniXNU write_volume, then byte-for-byte FAT readback).
Firmware must allow entry dispatch (PcdUniXNUMenuOnly=FALSE, the v4 FD). No OS/SystemOS/Data.
Run in WSL (needs mtools).
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path

EPOCH = 315532800
KERNEL_FILES = ('kernel.macho', 'kernel.macho.adt', 'kernel.macho.trustcache')
KERNEL_ARGS = ['-v', '-enable_kprintf_spam', 'serial=0', 'debug=0x14e', 'cpus=1',
               'kt_fixed_zones=120', 'offline-apfs-load=1', 'panic_on_crash_disabled=1',
               'sheng-data-persist=0', 'yunluo-read-verify=0', '-no_slto_panic']
CONFIG = """version = 1
[boot]
timeout = 10
default = "anim"
[kits]
autoload = false
[ui]
title = "Rothko animation"
scale = 3
[[driver]]
path = "Tools/BootEntries.efi"
connect = false
enabled = true
[[entry]]
id = "anim"
title = "Animation (power key returns)"
type = "efi"
path = "Tools/RothkoAnim.efi"
[[entry]]
id = "image"
title = "Show image (power key returns)"
type = "efi"
path = "Tools/ImageView.efi"
[[entry]]
id = "badapple"
title = "Bad Apple!! (power key returns)"
type = "efi"
path = "Tools/BadApple.efi"
[[entry]]
id = "uefi"
title = "UEFI Boot Manager"
type = "efi"
path = "fv:EEC25BDC-67F2-4D95-B1D5-F81B2039D11D"
[[entry]]
id = "xnu"
title = "Boot XNU (no iPadOS root)"
type = "xnu"
path = "Payloads/kernel.macho"
args = "{args}"
[[entry]]
id = "fastboot"
title = "Reboot to fastboot"
type = "efi"
path = "Tools/RebootFastboot.efi"
confirm = "Reboot to fastboot?"
[[entry]]
id = "reset"
title = "Reboot"
type = "action"
action = "reset"
"""


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--main', type=Path, required=True)
    ap.add_argument('--input-dir', type=Path, required=True)
    ap.add_argument('--anim', type=Path, required=True)
    ap.add_argument('--anim-sha256', required=True)
    ap.add_argument('--image', type=Path, required=True)
    ap.add_argument('--image-sha256', required=True)
    ap.add_argument('--entries', type=Path, required=True)
    ap.add_argument('--entries-sha256', required=True)
    ap.add_argument('--badapple', type=Path, required=True)
    ap.add_argument('--badapple-sha256', required=True)
    ap.add_argument('--fastboot', type=Path, required=True)
    ap.add_argument('--fastboot-sha256', required=True)
    ap.add_argument('--output-dir', type=Path, required=True)
    args = ap.parse_args()
    out, inputs, main_dir = args.output_dir.resolve(), args.input_dir.resolve(), args.main.resolve()
    names = ('diagnostic-stage', 'payload-diagnostic.img', 'payload-diagnostic.json')
    for name in names:
        if os.path.lexists(out / name):
            raise SystemExit(f'Refusing to overwrite {out / name}')
    for name in KERNEL_FILES:
        if not (inputs / name).is_file() or not (inputs / name).stat().st_size:
            raise SystemExit(f'missing input {inputs / name}')
    if sha(args.anim) != args.anim_sha256:
        raise SystemExit('RothkoAnim.efi hash mismatch')
    if sha(args.image) != args.image_sha256:
        raise SystemExit('ImageView.efi hash mismatch')
    if sha(args.entries) != args.entries_sha256:
        raise SystemExit('BootEntries.efi hash mismatch')
    if sha(args.badapple) != args.badapple_sha256:
        raise SystemExit('BadApple.efi hash mismatch')
    if sha(args.fastboot) != args.fastboot_sha256:
        raise SystemExit('RebootFastboot.efi hash mismatch')
    sys.path.insert(0, str(main_dir / 'scripts'))
    from unixnu.payload import write_volume, check_args

    kernel_args = check_args(KERNEL_ARGS)
    config = CONFIG.format(args=kernel_args)
    parsed = tomllib.loads(config)
    out.mkdir(parents=True, exist_ok=True)
    stage = out / names[0]
    base = stage / 'EFI/UniXNU'
    (base / 'Payloads').mkdir(parents=True)
    (base / 'Tools').mkdir()
    config_path = base / 'unixnu.toml'
    config_path.write_text(config, encoding='utf-8', newline='\n')
    files = {'EFI/UniXNU/unixnu.toml': config_path}
    for name in KERNEL_FILES:
        shutil.copyfile(inputs / name, base / 'Payloads' / name)
        files[f'EFI/UniXNU/Payloads/{name}'] = base / 'Payloads' / name
    shutil.copyfile(args.anim, base / 'Tools/RothkoAnim.efi')
    shutil.copyfile(args.image, base / 'Tools/ImageView.efi')
    shutil.copyfile(args.entries, base / 'Tools/BootEntries.efi')
    shutil.copyfile(args.badapple, base / 'Tools/BadApple.efi')
    shutil.copyfile(args.fastboot, base / 'Tools/RebootFastboot.efi')
    files['EFI/UniXNU/Tools/RothkoAnim.efi'] = base / 'Tools/RothkoAnim.efi'
    files['EFI/UniXNU/Tools/ImageView.efi'] = base / 'Tools/ImageView.efi'
    files['EFI/UniXNU/Tools/BootEntries.efi'] = base / 'Tools/BootEntries.efi'
    files['EFI/UniXNU/Tools/BadApple.efi'] = base / 'Tools/BadApple.efi'
    files['EFI/UniXNU/Tools/RebootFastboot.efi'] = base / 'Tools/RebootFastboot.efi'
    total = sum(p.stat().st_size for p in files.values())
    if total > 54 * 1024 * 1024:
        raise SystemExit(f'payload files total {total} bytes; too close to the 56 MiB volume')
    for path in [stage, *stage.rglob('*')]:
        os.utime(path, (EPOCH, EPOCH))
    with tempfile.TemporaryDirectory(prefix='.pack-payload-', dir=out) as folder:
        image = Path(folder) / 'payload.img'
        write_volume(stage, image, 'UNIXNU', 56)
        for index, (fat_path, source) in enumerate(files.items()):
            extracted = Path(folder) / f'verify-{index}'
            subprocess.run(['mcopy', '-i', str(image), '::/' + fat_path, str(extracted)], check=True)
            if sha(extracted) != sha(source):
                raise SystemExit(f'FAT readback mismatch: {fat_path}')
        report = dict(candidate='rothko-anim', menu_only=False, requires_menu_only_pcd=False,
                      main=str(main_dir), input_dir=str(inputs), payload_sha256=sha(image),
                      payload_bytes=image.stat().st_size, files_total_bytes=total,
                      config_sha256=sha(config_path), config=parsed, kernel_args=kernel_args,
                      anim_sha256=args.anim_sha256, image_sha256=args.image_sha256,
                      entries_sha256=args.entries_sha256, badapple_sha256=args.badapple_sha256,
                      fastboot_sha256=args.fastboot_sha256,
                      files={n: sha(p) for n, p in files.items()},
                      data_seed_present=False, root_prepared=False, hardware_tested=False)
        with image.open('rb') as source, (out / names[1]).open('xb') as dest:
            shutil.copyfileobj(source, dest)
    with (out / names[2]).open('x', encoding='utf-8') as f:
        f.write(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: report[k] for k in ('payload_sha256', 'payload_bytes', 'files_total_bytes', 'files')}, indent=2))


if __name__ == '__main__':
    main()
