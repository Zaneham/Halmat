#include "halmat.h"

typedef struct { uint32_t code; const char *name; } opcode_entry_t;

static const opcode_entry_t opcode_table[] = {
    /* Class 0: Control/Subscript */
    { 0x000, "NOP"  }, { 0x001, "EXTN" }, { 0x002, "XREC" }, { 0x003, "IMRK" },
    { 0x004, "SMRK" }, { 0x005, "PXRC" }, { 0x007, "IFHD" },
    { 0x008, "LBL"  }, { 0x009, "BRA"  }, { 0x00A, "FBRA" },
    { 0x00B, "DCAS" }, { 0x00C, "ECAS" }, { 0x00D, "CLBL" },
    { 0x00E, "DTST" }, { 0x00F, "ETST" },
    { 0x010, "DFOR" }, { 0x011, "EFOR" }, { 0x012, "CFOR" },
    { 0x013, "DSMP" }, { 0x014, "ESMP" }, { 0x015, "AFOR" },
    { 0x016, "CTST" }, { 0x017, "ADLP" }, { 0x018, "DLPE" },
    { 0x019, "DSUB" }, { 0x01A, "IDLP" }, { 0x01B, "TSUB" },
    { 0x01D, "PCAL" }, { 0x01E, "FCAL" },
    { 0x01F, "READ" }, { 0x020, "RDAL" }, { 0x021, "WRIT" }, { 0x022, "FILE" },
    { 0x025, "XXST" }, { 0x026, "XXND" }, { 0x027, "XXAR" },
    { 0x02A, "TDEF" }, { 0x02B, "MDEF" }, { 0x02C, "FDEF" },
    { 0x02D, "PDEF" }, { 0x02E, "UDEF" }, { 0x02F, "CDEF" },
    { 0x030, "CLOS" }, { 0x031, "EDCL" }, { 0x032, "RTRN" },
    { 0x033, "TDCL" }, { 0x034, "WAIT" }, { 0x035, "SGNL" },
    { 0x036, "CANC" }, { 0x037, "TERM" }, { 0x038, "PRIO" }, { 0x039, "SCHD" },
    { 0x03C, "ERON" }, { 0x03D, "ERSE" },
    { 0x040, "MSHP" }, { 0x041, "VSHP" }, { 0x042, "SSHP" }, { 0x043, "ISHP" },
    { 0x045, "SFST" }, { 0x046, "SFND" }, { 0x047, "SFAR" },
    { 0x04A, "BFNC" }, { 0x04B, "LFNC" },
    { 0x04D, "TNEQ" }, { 0x04E, "TEQU" }, { 0x04F, "TASN" },
    { 0x051, "IDEF" }, { 0x052, "ICLS" },
    { 0x055, "NNEQ" }, { 0x056, "NEQU" }, { 0x057, "NASN" },
    { 0x059, "PMHD" }, { 0x05A, "PMAR" }, { 0x05B, "PMIN" },
    /* Class 1: Bit */
    { 0x101, "BASN" }, { 0x102, "BAND" }, { 0x103, "BOR"  },
    { 0x104, "BNOT" }, { 0x105, "BCAT" },
    { 0x121, "BTOB" }, { 0x122, "BTOQ" },
    { 0x141, "CTOB" }, { 0x142, "CTOQ" },
    { 0x1A1, "STOB" }, { 0x1A2, "STOQ" },
    { 0x1C1, "ITOB" }, { 0x1C2, "ITOQ" },
    /* Class 2: Character */
    { 0x201, "CASN" }, { 0x202, "CCAT" },
    { 0x221, "BTOC" }, { 0x241, "CTOC" },
    { 0x2A1, "STOC" }, { 0x2C1, "ITOC" },
    /* Class 3: Matrix */
    { 0x301, "MASN" }, { 0x329, "MTRA" }, { 0x3CA, "MINV" },
    { 0x368, "MMPR" }, { 0x3A5, "MSPR" }, { 0x3A6, "MSDV" },
    { 0x362, "MADD" }, { 0x363, "MSUB" }, { 0x344, "MNEG" },
    { 0x341, "MTOM" }, { 0x371, "MDET" }, { 0x373, "MIDN" },
    { 0x387, "VVPR" }, { 0x46C, "MVPR" },
    /* Class 4: Vector */
    { 0x401, "VASN" }, { 0x46D, "VMPR" }, { 0x4A5, "VSPR" },
    { 0x58E, "VDOT" }, { 0x48B, "VCRS" },
    { 0x482, "VADD" }, { 0x483, "VSUB" }, { 0x444, "VNEG" }, { 0x441, "VTOV" },
    /* Class 5: Scalar */
    { 0x501, "SASN" }, { 0x5AB, "SADD" }, { 0x5AC, "SSUB" },
    { 0x5AD, "SSPR" }, { 0x5AE, "SSDV" }, { 0x5AF, "SEXP" }, { 0x5B0, "SNEG" },
    { 0x571, "SIEX" }, { 0x572, "SPEX" }, { 0x5A1, "STOS" }, { 0x5C1, "ITOS" },
    { 0x521, "BTOS" }, { 0x541, "CTOS" },
    /* Class 6: Integer */
    { 0x601, "IASN" }, { 0x6CB, "IADD" }, { 0x6CC, "ISUB" },
    { 0x6CD, "IIPR" }, { 0x6D0, "INEG" }, { 0x6D2, "IPEX" }, { 0x6C1, "ITOI" },
    { 0x6A1, "STOI" }, { 0x621, "BTOI" }, { 0x641, "CTOI" },
    /* Class 7: Conditional */
    { 0x720, "BTRU" },
    { 0x726, "BEQU" }, { 0x725, "BNEQ" },
    { 0x746, "CEQU" }, { 0x745, "CNEQ" }, { 0x74A, "CLT"  },
    { 0x748, "CGT"  }, { 0x747, "CNGT" }, { 0x749, "CNLT" },
    { 0x766, "MEQU" }, { 0x765, "MNEQ" },
    { 0x786, "VEQU" }, { 0x785, "VNEQ" },
    { 0x7A6, "SEQU" }, { 0x7A5, "SNEQ" }, { 0x7AA, "SLT"  },
    { 0x7A8, "SGT"  }, { 0x7A7, "SNGT" }, { 0x7A9, "SNLT" },
    { 0x7C6, "IEQU" }, { 0x7C5, "INEQ" }, { 0x7CA, "ILT"  },
    { 0x7C8, "IGT"  }, { 0x7C7, "INGT" }, { 0x7C9, "INLT" },
    { 0x7E2, "CAND" }, { 0x7E3, "COR"  }, { 0x7E4, "CNOT" },
    /* Class 8: Initialization */
    { 0x801, "STRI" }, { 0x802, "SLRI" }, { 0x803, "ELRI" }, { 0x804, "ETRI" },
    { 0x821, "BINT" }, { 0x841, "CINT" }, { 0x861, "MINT" }, { 0x881, "VINT" },
    { 0x8A1, "SINT" }, { 0x8C1, "IINT" },
    { 0x8E1, "NINT" }, { 0x8E2, "TINT" }, { 0x8E3, "EINT" },
    { 0, NULL }
};

static const char *class_names[] = {
    "CTRL", "BIT", "CHAR", "MAT", "VEC", "SCAL", "INT", "COND", "INIT"
};

static const char *qual_names[] = {
    "---", "SYT", "INL", "VAC", "XPT", "LIT", "IMD", "AST",
    "CSZ", "ASZ", "OFF", "Q11", "Q12", "Q13", "Q14", "Q15"
};

const char *halmat_popcode_name(uint32_t popcode)
{
    for (int i = 0; opcode_table[i].name != NULL; i++) {
        if (opcode_table[i].code == popcode)
            return opcode_table[i].name;
    }
    return NULL;
}

const char *halmat_class_name(uint32_t cls)
{
    if (cls < 9)
        return class_names[cls];
    return "???";
}

const char *halmat_qual_name(uint32_t qual)
{
    if (qual < 16)
        return qual_names[qual];
    return "???";
}

static void lit_annotation(halmat_t *H, uint32_t idx, char *buf, int bufsize)
{
    if (idx >= H->lit_count) {
        buf[0] = '\0';
        return;
    }

    int typ = H->lit[idx].lit1;
    int32_t v2 = H->lit[idx].lit2;

    switch (typ) {
    case 0: {
        int len = ((v2 >> 24) & 0xFF) + 1;
        snprintf(buf, bufsize, "=CHAR(%d)", len);
        break;
    }
    case 1: {
        double v = ibm_float_to_double((uint32_t)v2);
        snprintf(buf, bufsize, "=%g", v);
        break;
    }
    case 2:
        snprintf(buf, bufsize, "=BIT'%X'", v2);
        break;
    case 5: {
        double v = ibm_double_to_double((uint32_t)v2, (uint32_t)H->lit[idx].lit3);
        snprintf(buf, bufsize, "=%g", v);
        break;
    }
    default:
        snprintf(buf, bufsize, "=?type%d", typ);
        break;
    }
}

void halmat_disasm(halmat_t *H, FILE *out)
{
    for (uint32_t blk = 0; blk < H->num_blocks; blk++) {
        uint32_t base = blk * HALMAT_BLOCK_WORDS;
        uint32_t w1 = H->code[base + 1];
        uint32_t atom_fault = (w1 >> 16) & 0xFFFF;

        fprintf(out, "=== BLOCK %u === (%u atoms, words 2..%u)\n\n",
                blk, atom_fault, atom_fault);
        fprintf(out, "  %-5s  %-10s  %-6s  %-5s  %-6s  %s\n",
                "ADDR", "RAW", "TYPE", "TAG", "COPT", "DECODED");
        fprintf(out, "  ");
        for (int k = 0; k < 70; k++) fputc('-', out);
        fputc('\n', out);

        uint32_t i = base + 2;
        uint32_t end = base + atom_fault;

        while (i <= end) {
            uint32_t w = H->code[i];

            if (HALMAT_IS_OP(w)) {
                uint32_t tag    = HALMAT_TAG(w);
                uint32_t numop  = HALMAT_NUMOP(w);
                uint32_t pop    = HALMAT_POPCODE(w);
                uint32_t cls    = HALMAT_CLASS(w);
                uint32_t copt   = HALMAT_COPT(w);

                const char *name = halmat_popcode_name(pop);
                char namebuf[16];
                if (!name) {
                    snprintf(namebuf, sizeof(namebuf), "?%03X", pop);
                    name = namebuf;
                }

                const char *clsname = halmat_class_name(cls);
                char tag_str[16] = "";
                char copt_str[16] = "";
                if (tag > 0) snprintf(tag_str, sizeof(tag_str), "T=%u", tag);
                if (copt > 0) snprintf(copt_str, sizeof(copt_str), "C=%u", copt);

                fprintf(out, "  %4u:  %08X  %-6s  %-5s  %-6s  %s  (%s/%s, %u ops)\n",
                        i, w, clsname, tag_str, copt_str,
                        name, clsname, name, numop);

                for (uint32_t j = 1; j <= numop && (i + j) <= end; j++) {
                    uint32_t ow = H->code[i + j];
                    if (HALMAT_IS_OPERAND(ow)) {
                        uint32_t data = HALMAT_DATA(ow);
                        uint32_t tag1 = HALMAT_TAG1(ow);
                        uint32_t qual = HALMAT_QUAL(ow);
                        uint32_t tag2 = HALMAT_TAG2(ow);

                        const char *qname = halmat_qual_name(qual);
                        char annot[64] = "";

                        if (qual == QUAL_LIT)
                            lit_annotation(H, data, annot, sizeof(annot));

                        char taginfo[32] = "";
                        if (tag1 > 0 || tag2 > 0)
                            snprintf(taginfo, sizeof(taginfo),
                                     " [T1=%u T2=%u]", tag1, tag2);

                        fprintf(out, "         %08X    op%-2u               %s(%u)%s%s\n",
                                ow, j, qname, data, annot, taginfo);
                    } else {
                        fprintf(out, "         %08X    op%-2u               <unexpected operator>\n",
                                ow, j);
                    }
                }

                i += numop + 1;
            } else {
                /* Stray operand word */
                uint32_t data = HALMAT_DATA(w);
                uint32_t tag1 = HALMAT_TAG1(w);
                uint32_t qual = HALMAT_QUAL(w);
                uint32_t tag2 = HALMAT_TAG2(w);
                fprintf(out, "  %4u:  %08X  STRAY               %s(%u) [T1=%u T2=%u]\n",
                        i, w, halmat_qual_name(qual), data, tag1, tag2);
                i++;
            }
        }
        fprintf(out, "\n");
    }
}
