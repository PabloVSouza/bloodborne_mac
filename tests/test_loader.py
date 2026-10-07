"""Loader check without the game: a minimal BBPROBE5 image (the format link_modules.py writes)
whose guest code runs on the guest main thread and verifies what the game relies on:
  - its stack is below 1 TiB;
  - the rewritten `mov rax, fs:[0]` (relocation kind 3) loads this thread's TCB (tcb[0] == TCB);
  - an import call reaches the runtime (exit(42) through the libc import table).
Usage: python3 tests/test_loader.py out/bb-probe
"""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

EXIT_IMPORT = 'uMei1W9uyNo#q#q'  # libc exit(), served by runtime.c
CODE, DATA, SIZE = 0x0, 0x1000, 0x2000
PROCPARAM, EXIT_SLOT, INIT = DATA, DATA + 0x100, 0x100


def guest_code():
    code = bytearray()

    def jump_to_slot():  # jmp [rip + slot]
        code.extend(b'\xff\x25' + struct.pack('<i', EXIT_SLOT - (CODE + len(code) + 6)))
    code += b'\x48\x89\xe0'                      # mov rax, rsp
    code += b'\x48\xc1\xe8\x28'                  # shr rax, 40
    code += b'\x75\x00'; stack_fail = len(code) - 1   # jnz fail
    code += b'\x65\x48\x8b\x04\x25\0\0\0\0'      # mov rax, gs:[disp32] (rewritten fs:[0])
    tls_slot = len(code) - 4
    code += b'\x48\x85\xc0'                      # test rax, rax
    code += b'\x74\x00'; null_fail = len(code) - 1    # jz fail
    code += b'\x48\x39\x00'                      # cmp [rax], rax
    code += b'\x75\x00'; self_fail = len(code) - 1    # jne fail
    code += b'\xbf' + struct.pack('<I', 42)      # mov edi, 42
    jump_to_slot()
    fail = len(code)
    for at in (stack_fail, null_fail, self_fail):
        code[at] = fail - (at + 1)
    code += b'\xbf' + struct.pack('<I', 1)       # mov edi, 1
    jump_to_slot()
    return bytes(code), tls_slot


def boot_file(path):
    code, tls_slot = guest_code()
    image = bytearray(SIZE)
    image[CODE:CODE + len(code)] = code
    image[INIT:INIT + 3] = b'\x31\xc0\xc3'       # module init: xor eax, eax; ret
    segments = [(CODE, 0x1000, 5), (DATA, 0x1000, 6)]
    names = [EXIT_IMPORT]
    relocs = [(EXIT_SLOT, 1, 0, 0), (CODE + tls_slot, 3, 0, 0)]
    with open(path, 'wb') as f:
        f.write(struct.pack('<8s6Q', b'BBPROBE5', SIZE, CODE, len(segments), len(relocs), len(names), 1))
        f.write(struct.pack('<Q', PROCPARAM))
        f.write(struct.pack('<4Q', 0, 0, 0, 0))                     # no eboot TLS
        f.write(struct.pack('<Q', 1))
        f.write(struct.pack('<7Q', 0, SIZE, INIT, 0, 0, 0, 0))       # one module, no TLS
        f.write(struct.pack('<Q', 0))                                # no native bindings
        for segment in segments:
            f.write(struct.pack('<3Q', *segment))
        for name in names:
            f.write(name.encode().ljust(128, b'\0'))
        for relocation in relocs:
            f.write(struct.pack('<QQqq', *relocation))
        f.write(image)


def main():
    probe = sys.argv[1] if len(sys.argv) > 1 else 'out/bb-probe'
    with tempfile.TemporaryDirectory() as directory:
        boot = Path(directory) / 'boot-test.bin'
        boot_file(boot)
        run = subprocess.run([probe, str(boot), '--cpu-only', '--timeout', '10'],
                             capture_output=True, text=True, timeout=60)
    if run.returncode != 42:
        sys.stdout.write(run.stdout)
        sys.stderr.write(run.stderr)
        print(f'FAIL: loader test exited with {run.returncode} (expected 42; 1 = stack or TCB check failed)')
        return 1
    print('PASS: loader (low guest stack, guest thread pointer, import call)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
