/* tools/cpu/encodings.c ELF [MNEMONIC,...] [MAX]: the distinct encodings (hex bytes per line) of
 * the instructions in an x86-64 ELF's executable PT_LOAD segments (linear sweep with Zydis), all of
 * them or those of the listed mnemonics, at most MAX (default 2000) per mnemonic. Input for
 * tests/fuzz_jit.c without running the game. */
#include <Zydis/Zydis.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* The ELF64 structures used (macOS has no <elf.h>). */
typedef struct { unsigned char e_ident[16]; uint16_t e_type, e_machine; uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff; uint32_t e_flags; uint16_t e_ehsize, e_phentsize, e_phnum,
    e_shentsize, e_shnum, e_shstrndx; } Elf64_Ehdr;
typedef struct { uint32_t p_type, p_flags; uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz,
    p_align; } Elf64_Phdr;
enum { PT_LOAD = 1, PF_X = 1 };

enum { SEEN_BITS = 22 };
static uint64_t seen[2u << SEEN_BITS];
static int is_new(const uint8_t *bytes, int length) {
    uint64_t k[2] = {0, 0};
    memcpy(k, bytes, (size_t)length);
    k[1] ^= (uint64_t)length << 56; /* never 0 */
    const uint64_t h = (k[0] * 0x9E3779B97F4A7C15ull) ^ (k[1] * 0xC2B2AE3D27D4EB4Full);
    for (uint64_t i = h >> (64 - SEEN_BITS);; i = (i + 1) & ((1u << SEEN_BITS) - 1)) {
        uint64_t *e = &seen[2 * i];
        if (e[0] == k[0] && e[1] == k[1]) return 0;
        if (!e[1]) { e[0] = k[0]; e[1] = k[1]; return 1; }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: encodings ELF [MNEMONIC,...] [MAX]\n"); return 2; }
    static uint8_t wanted[ZYDIS_MNEMONIC_MAX_VALUE + 1];
    static unsigned per[ZYDIS_MNEMONIC_MAX_VALUE + 1];
    const int all = argc < 3 || !strcmp(argv[2], "all");
    const unsigned max = argc > 3 ? (unsigned)atoi(argv[3]) : 2000;
    if (!all) {
        char *list = strdup(argv[2]);
        for (char *save = NULL, *tok = strtok_r(list, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
            int found = 0;
            for (int m = 1; m <= ZYDIS_MNEMONIC_MAX_VALUE; ++m)
                if (!strcmp(ZydisMnemonicGetString((ZydisMnemonic)m), tok)) { wanted[m] = 1; found = 1; }
            if (!found) fprintf(stderr, "unknown mnemonic %s\n", tok);
        }
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) { perror("read"); return 2; }
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
    ZydisDecoder dec;
    ZydisDecoderInit(&dec, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    unsigned long printed = 0;
    for (int i = 0; i < eh->e_phnum; ++i) {
        const Elf64_Phdr *ph = (const Elf64_Phdr *)(data + eh->e_phoff + (uint64_t)i * eh->e_phentsize);
        if (ph->p_type != PT_LOAD || !(ph->p_flags & PF_X) || ph->p_offset + ph->p_filesz > (uint64_t)size) continue;
        const uint8_t *code = data + ph->p_offset;
        for (uint64_t at = 0; at < ph->p_filesz;) {
            ZydisDecodedInstruction zi;
            if (!ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(&dec, NULL, code + at, ph->p_filesz - at, &zi))) { ++at; continue; }
            if ((all || wanted[zi.mnemonic]) && per[zi.mnemonic] < max && is_new(code + at, zi.length)) {
                ++per[zi.mnemonic];
                for (int k = 0; k < zi.length; ++k) printf("%02x", code[at + k]);
                putchar('\n');
                ++printed;
            }
            at += zi.length;
        }
    }
    fprintf(stderr, "%lu encodings\n", printed);
    return 0;
}
