"""Build the rothko animation candidate (character animation from the UniXNU menu); never contacts a device.

  1. WSL: build-anim-efi.sh -> RothkoAnim.efi + BootEntries.efi, then check-pe.py (PE fields, entry = efi_main).
  2. ImageView.efi: reused unchanged from the image candidate (hash checked), as the second menu entry.
  2b. Bad Apple: the byte budget left on the payload volume is computed from the real file sizes, then
     WSL ../badapple/build-badapple-efi.sh encodes G:/cache/badapple.mov (first ladder step that fits,
     self-verified by decoding) and links BadApple.efi; check-pe.py, and the stream must be inside the EFI.
  3. boot_b: the v4 boot image, unchanged (same FD, entry dispatch enabled).
  4. vendor_boot_b: new FAT from make-anim-payload.py (Animation + Show image + Bad Apple + UEFI Boot Manager
     + v4 XNU + Reboot to fastboot + Reboot), packed into the stock vendor_boot by pack-diagnostic-vendor.py.
  5. One-click flash / rollback scripts from the v4 ones, with this delivery path and hash.

Run with Windows Python:
  python G:/New_Desktop/project/git/UniXNU-rothko/build/diagnostics/agent16/anim/build-anim.py
  [--firmware G:/cache/build_Ipados/out/rothko-fw-logo/integration-...]

--firmware: use the boot_b of a ../fw/build-logo-fw.py build (v4 firmware + boot logo) instead of the
v4 boot image; its build-result.json hash is checked and goes into the flash script.
"""
import argparse
import datetime
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DIAG = HERE.parents[1]
UEFI = Path('G:/cache/rothko-upstream-audit/uefi-rothko')
MAIN = Path('G:/cache/rothko-upstream-audit/UniXNU')
V4 = Path('G:/cache/build_Ipados/out/rothko-xnu-v4/integration-20261122-100404')
INPUTS = Path('G:/cache/build_Ipados/out/rothko-xnu-v4/inputs-20261122-100330')
IMAGE_EFI = Path('G:/cache/build_Ipados/out/rothko-image/integration-20261122-155810/efi/ImageView.efi')
IMAGE_EFI_SHA256 = '8dbaa61b8c682964dc1153fc93952cd17c2163c3665bdbf8c37ccb49136862a7'
STOCK_VENDOR_BOOT = Path('G:/cache/build_Ipados/backups/current-20261115-192206/vendor_boot_b.img')
AVB_FOOTER = Path('G:/cache/unixnu-port-cache/uefi-rothko/Resources/Scripts/avb_footer.py')
V4_BOOT_SHA256 = '96285068c57c68b6945cdc644a000bd84ea1583d3b8ba3a22f3ad9dc74aad82a'
V4_VENDOR_SHA256 = '837bbd21272fe0a25494bec503337520d884a15b1b5bec580dc3942b31de1c1b'
BADAPPLE_DIR = HERE.parent / 'badapple'
BADAPPLE_VIDEO = Path('G:/cache/badapple.mov')
KERNEL_FILES = ('kernel.macho', 'kernel.macho.adt', 'kernel.macho.trustcache')
PAYLOAD_LIMIT = 54 * 1024 * 1024        # same limit as make-anim-payload.py
CONFIG_ALLOWANCE = 8 * 1024             # unixnu.toml
BADAPPLE_CODE_ALLOWANCE = 192 * 1024    # BadApple.efi code, font, PE headers and alignment around the stream


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def wsl(p):
    p = Path(p).resolve().as_posix()
    return '/mnt/' + p[0].lower() + p[2:]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--firmware', type=Path, help='rothko-fw-logo build directory (default: v4 boot image)')
    args = ap.parse_args()
    if args.firmware:
        fw = json.loads((args.firmware / 'build-result.json').read_text(encoding='utf-8'))
        boot_src, fd_name = args.firmware / 'delivery', fw['fd_file']
        boot_expected, boot_kernel_sz, firmware = fw['boot_sha256'], fw['boot_kernel_sz'], 'logo:' + fw['fd_sha256']
    else:
        boot_src, fd_name = V4 / 'delivery', 'SILICIUM_UEFI-xnu-v4.fd'
        boot_expected, boot_kernel_sz, firmware = V4_BOOT_SHA256, '0x858b3', 'v4'
    if sha(boot_src / 'boot-diagnostic.img') != boot_expected:
        raise SystemExit(f'boot image hash changed in {boot_src}; stopping')
    print('BOOT', boot_expected, 'from', boot_src, flush=True)
    if sha(IMAGE_EFI) != IMAGE_EFI_SHA256:
        raise SystemExit('ImageView.efi from the image candidate changed; stopping')
    if not BADAPPLE_VIDEO.is_file():
        raise SystemExit(f'missing {BADAPPLE_VIDEO}')

    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    out = Path('G:/cache/build_Ipados/out/rothko-anim') / f'integration-{stamp}'
    efi_dir, delivery, oneclick = out / 'efi', out / 'delivery', out / 'oneclick'
    out.mkdir(parents=True)
    delivery.mkdir()
    oneclick.mkdir()
    for folder, prefix in ((HERE, 'source-'), (BADAPPLE_DIR, 'source-badapple-')):
        for script in folder.iterdir():
            if script.is_file():
                shutil.copy2(script, out / (prefix + script.name))

    def run(name, argv):
        with (out / name).open('x', encoding='utf-8') as log:
            log.write('COMMAND ' + json.dumps(argv) + '\n')
            log.flush()
            rc = subprocess.run(argv, stdout=log, stderr=subprocess.STDOUT).returncode
            log.write(f'\nEXIT {rc}\n')
        print(name, 'EXIT', rc, flush=True)
        if rc:
            print((out / name).read_text(encoding='utf-8', errors='replace')[-4000:])
            raise SystemExit(rc)

    run('efi-build.log', ['wsl.exe', '-d', 'Ubuntu', '--exec', 'bash', wsl(HERE / 'build-anim-efi.sh'),
                          '--root', wsl(UEFI), '--out', wsl(efi_dir)])
    anim = efi_dir / 'RothkoAnim.efi'
    entries = efi_dir / 'BootEntries.efi'
    fastboot = efi_dir / 'RebootFastboot.efi'
    run('efi-check.log', [sys.executable, '-B', str(HERE / 'check-pe.py'), str(anim), str(efi_dir / 'RothkoAnim.map')])
    run('entries-check.log', [sys.executable, '-B', str(HERE / 'check-pe.py'), str(entries), str(efi_dir / 'BootEntries.map')])
    run('fastboot-check.log', [sys.executable, '-B', str(HERE / 'check-pe.py'), str(fastboot), str(efi_dir / 'RebootFastboot.map')])
    anim_sha, entries_sha, fastboot_sha = sha(anim), sha(entries), sha(fastboot)

    # Bad Apple gets whatever the volume limit leaves after every other file.
    others = sum((INPUTS / n).stat().st_size for n in KERNEL_FILES)
    others += anim.stat().st_size + IMAGE_EFI.stat().st_size + entries.stat().st_size + fastboot.stat().st_size + CONFIG_ALLOWANCE
    budget = PAYLOAD_LIMIT - others - BADAPPLE_CODE_ALLOWANCE
    print('BADAPPLE_BUDGET', budget, 'bytes', flush=True)
    if budget < 256 * 1024:
        raise SystemExit(f'only {budget} bytes left for Bad Apple; stopping')
    ba_dir = efi_dir / 'badapple'
    run('badapple-build.log', ['wsl.exe', '-d', 'Ubuntu', '--exec', 'bash', wsl(BADAPPLE_DIR / 'build-badapple-efi.sh'),
                               '--root', wsl(UEFI), '--video', wsl(BADAPPLE_VIDEO), '--budget', str(budget),
                               '--out', wsl(ba_dir)])
    badapple = ba_dir / 'BadApple.efi'
    run('badapple-check.log', [sys.executable, '-B', str(HERE / 'check-pe.py'), str(badapple), str(ba_dir / 'BadApple.map')])
    ba_stream = (ba_dir / 'badapple.bin').read_bytes()
    ba_efi = badapple.read_bytes()
    ba_offset = ba_efi.find(ba_stream)
    if ba_stream[:4] != b'RBA1' or ba_offset < 0:
        raise SystemExit('BadApple.efi does not contain badapple.bin')
    ba_encode = json.loads((ba_dir / 'encode-result.json').read_text(encoding='utf-8').strip().splitlines()[-1])
    if not ba_encode.get('fits') or ba_encode.get('mismatched_frames') != 0:
        raise SystemExit(f'Bad Apple encode not verified: {ba_encode}')
    badapple_sha = sha(badapple)
    print('BADAPPLE', json.dumps(ba_encode), 'stream_offset', ba_offset, 'efi_bytes', len(ba_efi), flush=True)

    for name in ('boot-diagnostic.img', 'boot-diagnostic.body', 'boot-diagnostic.json', fd_name):
        shutil.copy2(boot_src / name, delivery / name)
    shutil.copy2(anim, delivery / 'RothkoAnim.efi')
    shutil.copy2(IMAGE_EFI, delivery / 'ImageView.efi')
    shutil.copy2(entries, delivery / 'BootEntries.efi')
    shutil.copy2(badapple, delivery / 'BadApple.efi')
    shutil.copy2(fastboot, delivery / 'RebootFastboot.efi')
    run('pack-payload.log', ['wsl.exe', '-d', 'Ubuntu', '--exec', 'env', 'PYTHONDONTWRITEBYTECODE=1', 'python3',
                             wsl(HERE / 'make-anim-payload.py'), '--main', wsl(MAIN), '--input-dir', wsl(INPUTS),
                             '--anim', wsl(anim), '--anim-sha256', anim_sha,
                             '--image', wsl(IMAGE_EFI), '--image-sha256', IMAGE_EFI_SHA256,
                             '--entries', wsl(entries), '--entries-sha256', entries_sha,
                             '--badapple', wsl(badapple), '--badapple-sha256', badapple_sha,
                             '--fastboot', wsl(fastboot), '--fastboot-sha256', fastboot_sha,
                             '--output-dir', wsl(delivery)])
    run('pack-vendor.log', [sys.executable, '-B', str(DIAG / 'pack-diagnostic-vendor.py'), '--main', str(MAIN),
                            '--stock', str(STOCK_VENDOR_BOOT), '--payload', str(delivery / 'payload-diagnostic.img'),
                            '--avb', str(AVB_FOOTER), '--output-dir', str(delivery)])
    boot, vendor = sha(delivery / 'boot-diagnostic.img'), sha(delivery / 'vendor_boot-diagnostic.img')
    if boot != boot_expected:
        raise SystemExit('copied boot image hash mismatch')

    # One-click scripts: the v4 scripts with this delivery path and vendor_boot hash.
    src = V4 / 'oneclick'
    common = (src / 'common.ps1').read_text(encoding='ascii')
    common = common.replace(str(V4 / 'delivery').replace('/', '\\'), str(delivery).replace('/', '\\'))
    common = common.replace(V4_VENDOR_SHA256, vendor).replace(V4_BOOT_SHA256, boot)
    common = common.replace('flash-v4.ps1', 'flash-anim.ps1')
    common = common.replace('rollback-v4.ps1', 'rollback-anim.ps1')
    if (V4_VENDOR_SHA256 in common or str(delivery).replace('/', '\\') not in common or vendor not in common
            or boot not in common):
        raise SystemExit('common.ps1 templating failed')
    (oneclick / 'common.ps1').write_text(common, encoding='ascii', newline='')
    for old, new in (('flash-v4.ps1', 'flash-anim.ps1'), ('rollback-v4.ps1', 'rollback-anim.ps1')):
        text = (src / old).read_text(encoding='ascii')
        text = text.replace('flash-v4', 'flash-anim').replace('rollback-v4', 'rollback-anim')
        text = text.replace('XNU diagnostic candidate v4 (no iPadOS root)', 'animation candidate (RothkoAnim + image + Bad Apple)')
        text = text.replace('flash v4', 'flash the animation candidate').replace('Flashing v4', 'Flashing the animation candidate')
        (oneclick / new).write_text(text, encoding='ascii', newline='')

    files = sorted(p for p in delivery.iterdir() if p.is_file())
    sums = ''.join(f'{sha(p)}  {p.name}\n' for p in files)
    (delivery / 'SHA256SUMS.txt').write_text(sums, encoding='ascii', newline='\n')
    result = {'candidate': 'rothko-anim', 'time': datetime.datetime.now().astimezone().isoformat(),
              'anim_sha256': anim_sha, 'image_efi_sha256': IMAGE_EFI_SHA256, 'entries_sha256': entries_sha,
              'badapple_sha256': badapple_sha, 'fastboot_sha256': fastboot_sha, 'badapple_budget': budget, 'badapple_encode': ba_encode,
              'badapple_video_sha256': sha(BADAPPLE_VIDEO),
              'boot_sha256': boot, 'boot_kernel_sz': boot_kernel_sz, 'firmware': firmware, 'vendor_boot_sha256': vendor,
              'images': {p.name: {'bytes': p.stat().st_size, 'sha256': sha(p)} for p in files},
              'oneclick': {p.name: sha(p) for p in sorted(oneclick.iterdir()) if p.is_file()},
              'hardware_tested': False}
    (out / 'build-result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(sums)
    print('OUT', out)
    print('FLASH', oneclick / 'flash-anim.ps1')
    print('ROLLBACK', oneclick / 'rollback-anim.ps1')


if __name__ == '__main__':
    main()
