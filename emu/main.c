#include "halmat.h"
#include "halmat_io.h"
#include "halmat_debug.h"
#include "hm_abend.h"

static halmat_t H;
static hm_abctx_t abctx;

static void usage(const char *prog)
{
    fprintf(stderr,
        "yaHALMAT - HALMAT Emulator\n"
        "Usage: %s [options] halmat.bin\n"
        "\n"
        "Options:\n"
        "  --disasm       Disassemble only (no execution)\n"
        "  --litfile F    Load literal table (resolves LIT references)\n"
        "  --common F     Load COMMON memory image (.bin or .bin.gz)\n"
        "  --unit N=PATH  Map logical unit N to file (stdin/stdout/stderr for std streams)\n"
        "  --ebcdic       Translate character output from EBCDIC CP 037 to ASCII\n"
        "  --debug        Enter debugger mode\n"
        "  --trace        Print each instruction as it executes\n"
        "  --dump         Print ABEND dump on runtime errors\n"
        "\n", prog);
}

int main(int argc, char *argv[])
{
    const char *halmat_file = NULL;
    const char *litfile = NULL;
    const char *common0 = NULL;
    int disasm_only = 0;
    int debug = 0;
    int trace = 0;

    halmat_init(&H);
    hm_ainit(&abctx);
    abctx.hm = &H;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--disasm") == 0) {
            disasm_only = 1;
        } else if (strcmp(argv[i], "--litfile") == 0 && i + 1 < argc) {
            litfile = argv[++i];
        } else if ((strcmp(argv[i], "--common") == 0 || strcmp(argv[i], "--common0") == 0) && i + 1 < argc) {
            common0 = argv[++i];
        } else if (strcmp(argv[i], "--unit") == 0 && i + 1 < argc) {
            i++;
            char *eq = strchr(argv[i], '=');
            if (!eq) {
                fprintf(stderr, "--unit requires N=PATH (e.g. --unit 6=output.txt)\n");
                return 1;
            }
            char *endptr;
            long unit_long = strtol(argv[i], &endptr, 10);
            if (endptr == argv[i] || endptr != eq) {
                fprintf(stderr, "--unit: invalid unit number in '%s'\n", argv[i]);
                return 1;
            }
            int unit_num = (int)unit_long;
            const char *path = eq + 1;
            if (unit_num < 0 || unit_num >= HALMAT_MAX_UNITS) {
                fprintf(stderr, "Unit number must be 0-%d\n", HALMAT_MAX_UNITS - 1);
                return 1;
            }
            if (strcmp(path, "stdin") == 0)
                H.units[unit_num].fp = stdin;
            else if (strcmp(path, "stdout") == 0)
                H.units[unit_num].fp = stdout;
            else if (strcmp(path, "stderr") == 0)
                H.units[unit_num].fp = stderr;
            else
                snprintf(H.units[unit_num].path, sizeof(H.units[unit_num].path), "%s", path);
        } else if (strcmp(argv[i], "--ebcdic") == 0) {
            H.translate_ebcdic = 1;
        } else if (strcmp(argv[i], "--debug") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "--trace") == 0) {
            trace = 1;
        } else if (strcmp(argv[i], "--dump") == 0) {
            abctx.dump = 1;
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

    if (!halmat_file)
        halmat_file = "halmat.bin";
    abctx.src = halmat_file;

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
        int dirlen = sep ? (int)(sep - halmat_file + 1) : 0;
        static const char *litnames[] = {
            "litfile0.bin", "litfile.bin", NULL
        };
        for (int li = 0; litnames[li]; li++) {
            if (sep)
                snprintf(autolit, sizeof(autolit), "%.*s%s", dirlen, halmat_file, litnames[li]);
            else
                snprintf(autolit, sizeof(autolit), "%s", litnames[li]);
            if (halmat_load_litfile(&H, autolit) == 0) break;
        }
    }

    if (common0) {
        int rc = halmat_load_common0(&H, common0);
        if (rc == -2) {
            fprintf(stderr, "Error: file not found: %s\n", common0);
            return 1;
        } else if (rc != 0) {
            fprintf(stderr, "Error: failed to load %s\n", common0);
            return 1;
        }
    } else {
        char autoc0[512];
        const char *sep3 = strrchr(halmat_file, '/');
        const char *sep4 = strrchr(halmat_file, '\\');
        if (sep4 && (!sep3 || sep4 > sep3)) sep3 = sep4;
        int dl3 = sep3 ? (int)(sep3 - halmat_file + 1) : 0;
        static const char *c0names[] = {
            "COMMON0.out.bin.gz", "COMMON0.out.bin", NULL
        };
        for (int ci = 0; c0names[ci]; ci++) {
            if (sep3)
                snprintf(autoc0, sizeof(autoc0), "%.*s%s", dl3, halmat_file, c0names[ci]);
            else
                snprintf(autoc0, sizeof(autoc0), "%s", c0names[ci]);
            if (halmat_load_common0(&H, autoc0) == 0) break;
        }
    }

    if (!H.mem_image_loaded) {
        char autosrc[512];
        const char *sep3 = strrchr(halmat_file, '/');
        const char *sep4 = strrchr(halmat_file, '\\');
        if (sep4 && (!sep3 || sep4 > sep3)) sep3 = sep4;
        if (sep3) {
            int dirlen = (int)(sep3 - halmat_file + 1);
            snprintf(autosrc, sizeof(autosrc), "%.*sSOURCECO.txt",
                     dirlen, halmat_file);
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
    halmat_free_common0(&H);

    if (H.halted < 0) {
        if (abctx.dump) {
            hm_abend(&abctx, hm_etou(H.halted));
        } else {
            fprintf(stderr, "yaHALMAT: execution error at PC=%u\n", H.pc);
        }
        return 1;
    }

    return 0;
}
