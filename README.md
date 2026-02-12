# HALMAT

A reconstructed specification of the Space Shuttle's compiler intermediate
language, produced by writing a PL/I F parser from scratch using techniques
learned from CBT Tape, then pointing it at 630 source files of a compiler
last touched in the 1990s, written in a language from 1969, targeting
hardware from 1964.

## What Is This

HALMAT is the intermediate representation used internally by the HAL/S-FC
compiler. HAL/S is the programming language of the Space Shuttle. Every
line of flight software ran through HALMAT on its way to object code.

It was never documented. Not publicly, not in any IBM manual, not in any
NASA technical report anyone can find. The only prior attempt is an ~850-line
sketch by Ron Burkey of the Virtual AGC project, which covers two of the
nine instruction classes and marks the remaining seven as "TODO".

This repository contains the complete HALMAT instruction set - all 180
opcodes across all 9 classes - reconstructed from the compiler's own source
code, by parsing every file, tracing every reference, and verifying against
actual binary output. The spec also documents the control flow patterns:
the structured groups of opcodes that implement FOR loops, IF/ELSE, CASE
statements, WHILE/UNTIL loops, I/O operations, function calls, and array
subscripting. Every pattern is verified against compiled HALMAT binary
from real HAL/S programs. The spec is ~4700 lines.

## How This Happened

The HAL/S compiler is written in XPL/I, which is an extended dialect of XPL,
which is a strict subset of PL/I, which is a language IBM designed in 1964.
Parsers for XPL exist inside compilers (notably Burkey's XCOM-I, which is how
HAL/S-FC was compiled in the first place), but there was no standalone,
grammar-driven parser built from the XPL BNF that could be pointed at
arbitrary source files for analysis. No `npm install xpl-parser`. No crate.

So I wrote one.

I studied vintage compiler construction from the CBT Tape archive,
the community-maintained collection of IBM mainframe software that's been
passed around on magnetic tape since the 1970s. The XPL BNF, the XCOM
bootstrap compiler, the PL/360 table-driven parser, all CBT Tape. I used
those lessons to build an OCaml parser (menhir LR(1) + ocamllex) that
handles every XPL/I extension the HAL/S compiler uses: BASED records,
DYNAMIC arrays, LITERALLY macros, spaced comparison operators, the lot.

It parses all 630 XPL files across all seven compiler passes. 630 out of
630. 100%.

Then I turned it loose on the compiler source to extract every HALMAT
opcode definition, cross-reference every opcode against every pass, and
generate the specification you see here. I also wrote a binary disassembler
that decodes the actual HALMAT output -including IBM System/360
hexadecimal floating-point literals from 1964, because of course the literal
table stores floats in a format that predates IEEE 754 by two decades.

The disassembler resolves symbol table entries from COMMON memory dumps and
literal values from the binary literal table, so the output reads like
annotated pseudocode. You can trace every HALMAT instruction back to the
HAL/S source line that produced it.

## Contents

```
HALMAT-SPEC.md          Full instruction set reference (~4700 lines, 180 opcodes,
                        7 verified control flow patterns)

disasm/main.ml          Binary HALMAT disassembler (OCaml)
patterns/main.ml        Control flow pattern analyser - walks HALMAT binary and
                        identifies nested FOR/IF/WHILE/CASE/DO/IO/call groups

data/
  halmat_disasm.txt     Annotated disassembly of HELLO.hal
  optmat_disasm.txt     Optimised HALMAT (after Phase 1.5)
  auxmat_disasm.txt     Auxiliary matrix output
  halmat_opcodes.txt    Opcode table (hex values, classes, names)
  xref.txt              Cross-reference: 268 opcode uses across 630 source files
  test_*.hal            HAL/S test programs for pattern verification
  out_*/halmat.bin      Compiled HALMAT binary from each test program
```

## The Instruction Set

180 opcodes, 9 classes:

| Class | Name | What It Does |
|-------|------|--------------|
| 0 | Control | Programme structure, flow control, I/O, subscripts, scope |
| 1 | Bit | Bit string ops -assign, AND, OR, NOT, CAT, type conversions |
| 2 | Character | String assign, concatenation, type conversions |
| 3 | Matrix | Matrix arithmetic, transpose, inverse, determinant, identity |
| 4 | Vector | Vector arithmetic, dot product, cross product |
| 5 | Scalar | Scalar arithmetic, exponentiation, type conversions |
| 6 | Integer | Integer arithmetic, type conversions |
| 7 | Conditional | Comparisons across all types, boolean connectives |
| 8 | Initialisation | Structure/array init, typed constant init |

Classes 1-7 were undocumented before this project.

## Word Format

32-bit big-endian. Bit 0 tells you what you're looking at.

**Operator** (bit 0 = 0):
```
[TAG:8][NUMOP:8][CLASS:4][OPCODE:8][COPT:3][0:1]
```

**Operand** (bit 0 = 1):
```
[DATA:16][TAG1:8][QUAL:4][TAG2:3][1:1]
```

Operand qualifiers:
| Q | Name | Points To |
|---|------|-----------|
| 1 | SYT | Symbol table entry |
| 2 | INL | Internal flow number |
| 3 | VAC | Virtual accumulator (the operator that produced this value) |
| 4 | XPT | Extended pointer |
| 5 | LIT | Literal table entry |
| 6 | IMD | Immediate value |
| 7 | AST | Atom stack pointer |

Blocks are 1800 words (7200 bytes). Word 1 holds the atom count. Atoms
start at word 2.

## Example

Here is a HAL/S programme:

```hal
HELLO: PROGRAM;
   DECLARE I INTEGER;
   DECLARE MY_NAME CHARACTER(20) INITIAL('RON BURKEY');
   DO FOR I = 1 TO 5;
      WRITE(6) I, 'HELLO, WORLD!';
   END;
CLOSE HELLO;
```

And here is what the compiler actually produces -decoded HALMAT with
symbol and literal resolution:

```
MDEF  SYT(1)=HELLO                         -- HELLO: PROGRAM
CINT  SYT(3)=MY_NAME  LIT(2)=CHAR(10)     -- INITIAL('RON BURKEY')
EDCL                                        -- end of declarations
DFOR  INL(1)  SYT(2)=I  LIT(5)=1.0  LIT(6)=5.0   -- DO FOR I = 1 TO 5
  XXST  IMD(2)                              -- begin WRITE arguments
  XXAR  SYT(2)=I                            -- arg: I
  XXAR  LIT(8)=CHAR(13)                     -- arg: 'HELLO, WORLD!'
  WRIT  IMD(6)                              -- WRITE to unit 6
  XXND                                      -- end arguments
EFOR  INL(1)                                -- END (of DO FOR)
CLOS  SYT(1)=HELLO                         -- CLOSE HELLO
```

Every instruction maps to the source. The literal `1.0` is stored as IBM
System/360 hexadecimal float `0x41100000`. The string `'RON BURKEY'` is
`CHAR(10)` -ten bytes, indexed into a literal table that uses a format
designed for punch-card batch processing on a machine with 256KB of core
memory. The disassembler reads all of it.

## Methodology

1. **Parse** all 630 XPL/I source files with a purpose-built LR(1) parser,
   informed by compiler construction techniques from CBT Tape (XCOM, PL/360,
   the XPL BNF grammar).

2. **Extract** opcode definitions -BIT(16) INITIAL constants with
   X-prefixed names from `##DRIVER.xpl` -yielding 180 opcodes.

3. **Cross-reference** every opcode against all seven passes (PASS1–4, OPT,
   AUX, FLO) to find emission, consumption, and optimisation sites.

4. **Disassemble** HALMAT binary output with literal table decoding (IBM
   S/360 hex float) and symbol resolution from COMMON memory dumps.

5. **Merge** all evidence with Burkey's existing documentation into a
   unified reference.

6. **Compile** targeted HAL/S test programs through the Python PASS1 port
   (one for each control flow construct: IF/ELSE, DO WHILE, DO UNTIL,
   DO CASE, discrete FOR, simple DO, function calls, array subscripts,
   nested combinations). Disassemble the binary output. Run it through
   a purpose-built pattern analyser that walks the HALMAT stream and
   identifies the grouped opcodes. Verify every claim in the spec against
   the actual compiled binary - TAG values, operand orders, flow label
   allocation, sentinel markers, all of it.

## Prior Art

Ron Burkey's Virtual AGC project preserved the HAL/S compiler source code
and produced the first HALMAT documentation (`yaHAL-S/HALMAT.md`). That
document is thorough on Class 0 and Class 8 but explicitly marks Classes
1–7 as unfinished. Burkey also wrote a Python decoder (`HALMAT.py`) that
handles basic disassembly. This project fills the remaining gaps by going
back to the compiler source itself.

## Source Material

All source material is from the Virtual AGC project:

- **Compiler source**: `yaShuttle/Source Code/PASS.REL32V0/` (630 XPL files)
- **Existing docs**: `yaShuttle/yaHAL-S/HALMAT.md` (~850 lines, partial)
- **Regression test**: `PASS.REL32V0/regression/` (HELLO.hal, halmat.bin,
  litfilea.bin, COMMON-PASS1.out)

## Acknowledgements

Ron Burkey and the Virtual AGC project for the extraordinary work of
preserving the HAL/S compiler and getting it running again. Without that
effort, there would be nothing to reverse-engineer.

The CBT Tape community for keeping vintage IBM software alive and accessible.
The XPL BNF and XCOM sources were essential to building the parser that made
this possible.
