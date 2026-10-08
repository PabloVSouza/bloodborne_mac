/* tools/cpu/census.c ELF...: x86-64 instruction census of executable PT_LOAD segments (linear
 * sweep with Zydis): mnemonic counts, ISA extensions, prefixes and other features an x86-64 to
 * arm64 translator must handle. */
#include <Zydis/Zydis.h>
#include <stdint.h>
/* The ELF64 structures used (macOS has no <elf.h>). */
typedef struct { unsigned char e_ident[16]; uint16_t e_type, e_machine; uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff; uint32_t e_flags; uint16_t e_ehsize, e_phentsize, e_phnum,
    e_shentsize, e_shnum, e_shstrndx; } Elf64_Ehdr;
typedef struct { uint32_t p_type, p_flags; uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz,
    p_align; } Elf64_Phdr;
enum { PT_LOAD = 1, PF_X = 1 };
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char name[64]; unsigned long count; } Entry;
static Entry mnem[4096], isa[256], feat[64];
static int nmnem, nisa, nfeat;
static void bump(Entry *t, int *n, const char *name) {
    for (int i = 0; i < *n; ++i) if (!strcmp(t[i].name, name)) { ++t[i].count; return; }
    snprintf(t[*n].name, sizeof(t[*n].name), "%s", name); t[(*n)++].count = 1;
}
static int cmp(const void *a, const void *b) {
    const Entry *x = a, *y = b; return x->count < y->count ? 1 : x->count > y->count ? -1 : 0;
}
static void dump(const char *title, Entry *t, int n, int limit) {
    qsort(t, n, sizeof(*t), cmp);
    printf("== %s (%d kinds)\n", title, n);
    for (int i = 0; i < n && i < limit; ++i) printf("%10lu %s\n", t[i].count, t[i].name);
}
int main(int argc, char **argv) {
    ZydisDecoder dec;
    ZydisDecoderInit(&dec, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    unsigned long total = 0, bad = 0;
    for (int a = 1; a < argc; ++a) {
        FILE *f = fopen(argv[a], "rb");
        if (!f) { perror(argv[a]); continue; }
        fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
        unsigned char *data = malloc(size);
        if (fread(data, 1, size, f) != (size_t)size) return 1;
        fclose(f);
        Elf64_Ehdr *eh = (Elf64_Ehdr *)data;
        for (int p = 0; p < eh->e_phnum; ++p) {
            Elf64_Phdr *ph = (Elf64_Phdr *)(data + eh->e_phoff + p * eh->e_phentsize);
            if (ph->p_type != PT_LOAD || !(ph->p_flags & PF_X) || ph->p_offset + ph->p_filesz > (unsigned long)size) continue;
            const unsigned char *code = data + ph->p_offset;
            size_t off = 0, len = ph->p_filesz;
            ZydisDecodedInstruction in;
            ZydisDecodedOperand ops[ZYDIS_MAX_OPERAND_COUNT];
            while (off < len) {
                if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&dec, code + off, len - off, &in, ops))) { ++bad; ++off; continue; }
                ++total;
                bump(mnem, &nmnem, ZydisMnemonicGetString(in.mnemonic));
                bump(isa, &nisa, ZydisISAExtGetString(in.meta.isa_ext));
                if (in.attributes & ZYDIS_ATTRIB_HAS_LOCK) bump(feat, &nfeat, "lock prefix");
                if (in.attributes & (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPE | ZYDIS_ATTRIB_HAS_REPNE)) bump(feat, &nfeat, "rep prefix");
                if (in.attributes & ZYDIS_ATTRIB_HAS_SEGMENT_FS) bump(feat, &nfeat, "fs segment");
                if (in.attributes & ZYDIS_ATTRIB_HAS_SEGMENT_GS) bump(feat, &nfeat, "gs segment");
                if (in.encoding == ZYDIS_INSTRUCTION_ENCODING_VEX) bump(feat, &nfeat, in.avx.vector_length == 256 ? "VEX.256" : "VEX.128");
                if (in.encoding == ZYDIS_INSTRUCTION_ENCODING_EVEX) bump(feat, &nfeat, "EVEX");
                if (in.meta.category == ZYDIS_CATEGORY_X87_ALU || in.meta.isa_set == ZYDIS_ISA_SET_X87) bump(feat, &nfeat, "x87");
                if (in.attributes & ZYDIS_ATTRIB_IS_RELATIVE) {
                    for (int o = 0; o < in.operand_count_visible; ++o)
                        if (ops[o].type == ZYDIS_OPERAND_TYPE_MEMORY && ops[o].mem.base == ZYDIS_REGISTER_RIP) { bump(feat, &nfeat, "rip-relative memory"); break; }
                }
                if (in.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE) {
                    int indirect = in.operand_count_visible && ops[0].type != ZYDIS_OPERAND_TYPE_IMMEDIATE;
                    if (in.mnemonic == ZYDIS_MNEMONIC_CALL) bump(feat, &nfeat, indirect ? "call indirect" : "call direct");
                    else if (in.mnemonic == ZYDIS_MNEMONIC_JMP) bump(feat, &nfeat, indirect ? "jmp indirect" : "jmp direct");
                }
                off += in.length;
            }
        }
        free(data);
    }
    printf("decoded %lu instructions (%lu undecodable bytes skipped)\n", total, bad);
    dump("features", feat, nfeat, 64);
    dump("ISA extensions", isa, nisa, 64);
    dump("mnemonics", mnem, nmnem, 4096);
    return 0;
}
