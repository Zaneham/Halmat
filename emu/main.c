#include "halmat.h"
#include "halmat_io.h"
#include "halmat_debug.h"

static halmat_t H;

static void usage(const char *prog)
{
    fprintf(stderr,
        "yaHALMAT — HALMAT Emulator\n"
        "Usage: %s [options] halmat.bin\n"
        "\n"
        "Options:\n"
        "  --disasm     Disassemble only (no execution)\n"
        "  --litfile F  Load literal table (resolves LIT references)\n"
        "  --debug      Enter debugger mode\n"
        "  --trace      Print each instruction as it executes\n"
        "\n", prog);
}

int main(int argc, char *argv[])
{
    const char *halmat_file = NULL;
    const char *litfile = NULL;
    int disasm_only = 0;
    int debug = 0;
    int trace = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--disasm") == 0) {
            disasm_only = 1;
        } else if (strcmp(argv[i], "--litfile") == 0 && i + 1 < argc) {
            litfile = argv[++i];
        } else if (strcmp(argv[i], "--debug") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "--trace") == 0) {
            trace = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            halmat_file = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (!halmat_file) {
        usage(argv[0]);
        return 1;
    }

    halmat_init(&H);

    if (halmat_load(&H, halmat_file) != 0) {
        fprintf(stderr, "Failed to load %s\n", halmat_file);
        return 1;
    }

    if (litfile) {
        if (halmat_load_litfile(&H, litfile) != 0) {
            fprintf(stderr, "Failed to load literal table %s\n", litfile);
            return 1;
        }
    } else {
        char autolit[512];
        const char *sep = strrchr(halmat_file, '/');
        const char *sep2 = strrchr(halmat_file, '\\');
        if (sep2 && (!sep || sep2 > sep)) sep = sep2;
        if (sep) {
            int dirlen = (int)(sep - halmat_file + 1);
            snprintf(autolit, sizeof(autolit), "%.*slitfile.bin", dirlen, halmat_file);
        } else {
            snprintf(autolit, sizeof(autolit), "litfile.bin");
        }
        halmat_load_litfile(&H, autolit); /* silent failure OK */
    }

    {
        char autosrc[512];
        const char *sep3 = strrchr(halmat_file, '/');
        const char *sep4 = strrchr(halmat_file, '\\');
        if (sep4 && (!sep3 || sep4 > sep3)) sep3 = sep4;
        if (sep3) {
            int dirlen = (int)(sep3 - halmat_file + 1);
            snprintf(autosrc, sizeof(autosrc), "%.*sSOURCECO.txt", dirlen, halmat_file);
        } else {
            snprintf(autosrc, sizeof(autosrc), "SOURCECO.txt");
        }
        halmat_load_strings(&H, autosrc); /* silent failure OK */

        /* Fallback: out_<name>/halmat.bin → test_<name>.hal in parent dir */
        if (H.lit_str_pool_used <= 1 && sep3) {
            const char *dir_start = sep3;
            while (dir_start > halmat_file &&
                   *(dir_start - 1) != '/' && *(dir_start - 1) != '\\')
                dir_start--;
            int dir_name_len = (int)(sep3 - dir_start);
            if (dir_name_len > 4 && memcmp(dir_start, "out_", 4) == 0) {
                int parent_len = (int)(dir_start - halmat_file);
                snprintf(autosrc, sizeof(autosrc), "%.*stest_%.*s.hal",
                         parent_len, halmat_file,
                         dir_name_len - 4, dir_start + 4);
                halmat_load_strings(&H, autosrc);
            }
        }
    }

    halmat_build_flow_table(&H);

    if (disasm_only) {
        printf("HALMAT DISASSEMBLY: %s\n", halmat_file);
        printf("%u bytes, %u block(s)\n\n",
               H.num_blocks * HALMAT_BLOCK_BYTES, H.num_blocks);
        halmat_disasm(&H, stdout);
        return 0;
    }

    halmat_io_init(&H);

    if (debug) {
        halmat_debug_init(&H);
        while (!H.halted) {
            if (H.single_step || halmat_debug_check_breakpoint(&H)) {
                halmat_debug_prompt(&H);
                if (H.halted) break;
            }
            halmat_step(&H);
        }
    } else {
        if (trace) {
            while (!H.halted) {
                if (H.pc < H.code_len && HALMAT_IS_OP(H.code[H.pc])) {
                    uint32_t w = H.code[H.pc];
                    const char *name = halmat_popcode_name(HALMAT_POPCODE(w));
                    fprintf(stderr, "[%4u] %s  (numop=%u tag=%u)\n",
                            H.pc, name ? name : "???",
                            HALMAT_NUMOP(w), HALMAT_TAG(w));
                }
                halmat_step(&H);
            }
        } else {
            halmat_run(&H);
        }
    }

    halmat_io_shutdown(&H);

    if (H.halted < 0) {
        fprintf(stderr, "yaHALMAT: execution error at PC=%u\n", H.pc);
        return 1;
    }

    return 0;
}
