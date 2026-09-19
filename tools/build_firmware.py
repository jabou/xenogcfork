#!/usr/bin/env python3
"""
build_firmware.py - assemble the XenoGC ATmega8 image with a new XenoBoot
payload, without recompiling the AVR firmware or the drivecode.

Base image : XenoAT/Bin/XenoBoot.1.03a.v1.hex (the v1 firmware)
Payload    : XenoShell/build/XenoBoot_packed.bin (make -C XenoShell)
Outputs    : build/XenoAT.hex            image for ISP programmers (avrdude)
             XenoFlash/data/XenoAT.bin  same image, embedded by the flasher build

Why not just rebuild the AVR part?  Its code contains the timing critical
handshake with the drive's MN102 CPU and the v1 build is known to inject
reliably; recompiling it with a different avr-gcc could change that.  So the
AVR code and the drivecode are taken verbatim from the v1 image and only the
bytes that encode the payload size are patched:

  AVR image layout (v1, 7510 bytes):
    0x0000  AVR code (1356 bytes)
    0x054C  qcode.bin     - MN102 drivecode incl. the PPC apploader hook
    0x0E70  upload.bin    - MN102 stage 1 uploader
    0x0F50  payload       - "credits" binary = the XenoBoot loader (this is
                            what we replace)

  Patched bytes:
    AVR 0x045C/0x0460  ldi r19,lo / ldi r19,hi of the payload END address.
                       XenoAT.c computes creditssize = ((end - start) & 0xFFFE) + 2
                       from the credits/credits_end symbols; gcc turned that into
                       these two immediates.
    AVR 0x0D10         inside qcode.bin (offset 0x7C4): PPC 'li r5, CREDITS_SIZE'
                       in QLiteIPL.asm.c SUB_DVDReadMemBlock = number of bytes the
                       apploader hook copies from drive RAM 0x40D7FC to 0x816FFFFC.
                       Must be payload length + 4.

The in-console flasher (XenoFlash/) embeds XenoFlash/data/XenoAT.bin as ordinary
data and shows the image size and CRC32 on screen before writing; the CRC32
printed by this script must match it.

Build order: make -C XenoShell; python3 tools/build_firmware.py; make -C XenoFlash

usage: tools/build_firmware.py [payload.bin]
"""
import hashlib, os, struct, sys, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

AVR_FLASH = 8192
PAYLOAD_OFF = 0x0F50
LDI_LO_OFF, LDI_HI_OFF = 0x045C, 0x0460     # ldi r19, lo/hi(credits_end)
QCODE_OFF = 0x054C
QCODE_LI_OFF = 0x7C4                        # li r5, CREDITS_SIZE inside qcode.bin
DRIVE_PAYLOAD_ADDR = 0x40D800
DRIVE_PAYLOAD_LIMIT = 0x40EBE6              # FwHLECmdBuffer: firmware data starts here
ORIG_XENOAT_LEN = 7510
BASE_HEX = 'XenoAT/Bin/XenoBoot.1.03a.v1.hex'     # vingt-2's XenoBoot v1 firmware = AVR code + drivecode we keep



def die(msg):
    sys.exit("build_firmware: " + msg)


def sha(b):
    return hashlib.sha256(b).hexdigest()


def hex2bin(path):
    data = bytearray()
    for line in open(path):
        line = line.strip()
        if not line.startswith(':'):
            continue
        n, addr, typ = int(line[1:3], 16), int(line[3:7], 16), int(line[7:9], 16)
        if typ == 0:
            if len(data) < addr:
                data.extend(b'\xff' * (addr - len(data)))
            data[addr:addr + n] = bytes.fromhex(line[9:9 + 2 * n])
    return bytes(data)


def bin2hex(data):
    out = []
    for a in range(0, len(data), 16):
        chunk = data[a:a + 16]
        rec = bytes([len(chunk), a >> 8, a & 0xFF, 0]) + chunk
        out.append(':%s%02X' % (rec.hex().upper(), (-sum(rec)) & 0xFF))
    out.append(':00000001FF')
    return '\n'.join(out) + '\n'


def ldi_r19(k):
    """AVR 'ldi r19, k' = 1110 KKKK dddd KKKK with d = 19-16 = 3, little endian."""
    op = 0xE000 | ((k & 0xF0) << 4) | (3 << 4) | (k & 0x0F)
    return bytes([op & 0xFF, op >> 8])


def main():
    payload_path = sys.argv[1] if len(sys.argv) > 1 else 'XenoShell/build/XenoBoot_packed.bin'
    payload = open(payload_path, 'rb').read()
    while len(payload) % 4:
        payload += b'\0'
    plen = len(payload)

    # --- reference AVR image -------------------------------------------------
    avr = bytearray(hex2bin(BASE_HEX))
    if len(avr) != ORIG_XENOAT_LEN:
        die('%s has unexpected size %d' % (BASE_HEX, len(avr)))
    qcode = open('XenoAT/source/qcode.bin', 'rb').read()
    if avr[QCODE_OFF:QCODE_OFF + len(qcode)] != qcode:
        die('qcode.bin not found at 0x%X in the AVR image' % QCODE_OFF)
    old_end = ORIG_XENOAT_LEN - 2   # credits_end symbol = 0x1D54
    if avr[LDI_LO_OFF:LDI_LO_OFF + 2] != ldi_r19(old_end & 0xFF) or avr[LDI_HI_OFF:LDI_HI_OFF + 2] != ldi_r19(old_end >> 8):
        die('payload size immediates not where expected')
    li_off = QCODE_OFF + QCODE_LI_OFF
    if avr[li_off:li_off + 4] != bytes.fromhex('38a00f04'):
        die('CREDITS_SIZE instruction not found in qcode')

    # --- limits ----------------------------------------------------------------
    new_end = PAYLOAD_OFF + plen
    if new_end + 2 > AVR_FLASH:
        die('payload %d bytes does not fit: max %d' % (plen, AVR_FLASH - PAYLOAD_OFF - 2))
    if DRIVE_PAYLOAD_ADDR + plen + 2 > DRIVE_PAYLOAD_LIMIT:
        die('payload does not fit in drive RAM')

    # --- build image -------------------------------------------------------------
    img = bytearray(avr[:PAYLOAD_OFF]) + payload
    img += b'\xff' * (AVR_FLASH - len(img))
    img[LDI_LO_OFF:LDI_LO_OFF + 2] = ldi_r19(new_end & 0xFF)
    img[LDI_HI_OFF:LDI_HI_OFF + 2] = ldi_r19(new_end >> 8)
    img[li_off:li_off + 4] = struct.pack('>I', 0x38A00000 | (plen + 4))
    img = bytes(img)

    # everything outside the patched bytes and the payload area must be identical
    for i in range(PAYLOAD_OFF):
        if img[i] != avr[i] and i not in range(LDI_LO_OFF, LDI_LO_OFF + 2) and i not in range(LDI_HI_OFF, LDI_HI_OFF + 2) and i not in range(li_off, li_off + 4):
            die('unexpected difference at 0x%X' % i)

    os.makedirs('build', exist_ok=True)
    open('build/XenoAT.hex', 'w').write(bin2hex(img))

    # --- flasher input --------------------------------------------------------------
    open('XenoFlash/data/XenoAT.bin', 'wb').write(img)

    print('payload           : %s, %d bytes (limit %d)' % (payload_path, plen, AVR_FLASH - PAYLOAD_OFF - 2))
    print('AVR payload end   : 0x%04X, upload size %d bytes, CREDITS_SIZE %d' % (new_end, plen + 2, plen + 4))
    print('build/XenoAT.hex  : %d bytes image, sha256 %s' % (len(img), sha(img)))
    print('image CRC32       : %08X   <- the flasher shows this on screen; it must match' % (zlib.crc32(img) & 0xFFFFFFFF))
    print('XenoFlash/data/XenoAT.bin written; now: make -C XenoFlash  -> XenoFlash/XenoFlash.dol')


if __name__ == '__main__':
    main()
