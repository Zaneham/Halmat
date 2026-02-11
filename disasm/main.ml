(* HALMAT disassembler. 1800 x 32-bit BE words per block. *)

(* Literal table: 130 entries per page, three parallel FIXED arrays. *)
let lit_page_size = 130

type lit_table = {
  lit1: int array;  (* type: 0=CHAR, 1=ARITH, 2=BIT, 5=DOUBLE *)
  lit2: int array;
  lit3: int array;
  count: int;
}

let read_litfile filename =
  let ic = open_in_bin filename in
  let size = in_channel_length ic in
  let npages = size / (lit_page_size * 3 * 4) in
  let n = npages * lit_page_size in
  let lit1 = Array.make n 0 in
  let lit2 = Array.make n 0 in
  let lit3 = Array.make n 0 in
  seek_in ic 0;
  for pg = 0 to npages - 1 do
    let base = pg * lit_page_size in
    for i = 0 to lit_page_size - 1 do
      let b0 = input_byte ic in let b1 = input_byte ic in
      let b2 = input_byte ic in let b3 = input_byte ic in
      lit1.(base + i) <- (b0 lsl 24) lor (b1 lsl 16) lor (b2 lsl 8) lor b3
    done;
    for i = 0 to lit_page_size - 1 do
      let b0 = input_byte ic in let b1 = input_byte ic in
      let b2 = input_byte ic in let b3 = input_byte ic in
      lit2.(base + i) <- (b0 lsl 24) lor (b1 lsl 16) lor (b2 lsl 8) lor b3
    done;
    for i = 0 to lit_page_size - 1 do
      let b0 = input_byte ic in let b1 = input_byte ic in
      let b2 = input_byte ic in let b3 = input_byte ic in
      lit3.(base + i) <- (b0 lsl 24) lor (b1 lsl 16) lor (b2 lsl 8) lor b3
    done
  done;
  close_in ic;
  { lit1; lit2; lit3; count = n }

let decode_ibm_float w =
  let sign = if w land 0x80000000 <> 0 then -1.0 else 1.0 in
  let exp = (w lsr 24) land 0x7F in
  let frac = w land 0x00FFFFFF in
  if frac = 0 then 0.0
  else
    let mantissa = Float.of_int frac /. 16777216.0 in
    sign *. mantissa *. (16.0 ** Float.of_int (exp - 64))

let decode_ibm_double w_hi w_lo =
  let sign = if w_hi land 0x80000000 <> 0 then -1.0 else 1.0 in
  let exp = (w_hi lsr 24) land 0x7F in
  let frac_hi = w_hi land 0x00FFFFFF in
  let frac_lo = w_lo land 0xFFFFFFFF in
  if frac_hi = 0 && frac_lo = 0 then 0.0
  else
    let mantissa =
      (Float.of_int frac_hi *. 4294967296.0 +. Float.of_int frac_lo)
      /. 72057594037927936.0 (* 2^56 *)
    in
    sign *. mantissa *. (16.0 ** Float.of_int (exp - 64))

let format_float v =
  let s = Printf.sprintf "%.10g" v in
  if String.contains s '.' || String.contains s 'e' then s
  else s ^ ".0"

let lit_annotation lit idx =
  if idx < 0 || idx >= lit.count then ""
  else
    let typ = lit.lit1.(idx) in
    let v2 = lit.lit2.(idx) in
    match typ with
    | 0 ->
      let len = ((v2 lsr 24) land 0xFF) + 1 in
      Printf.sprintf "=CHAR(%d)" len
    | 1 ->
      let v = decode_ibm_float v2 in
      Printf.sprintf "=%s" (format_float v)
    | 2 ->
      Printf.sprintf "=BIT'%X'" v2
    | 5 ->
      let v = decode_ibm_double v2 lit.lit3.(idx) in
      Printf.sprintf "=%s" (format_float v)
    | _ ->
      Printf.sprintf "=?type%d" typ

(* Symbol table extracted from COMMON-*.out text dumps. *)
type sym_table = {
  names: string array;
  sym_count: int;
}

let read_symtab filename =
  let ic = open_in filename in
  let tbl = Hashtbl.create 64 in
  let cur_idx = ref (-1) in
  let max_idx = ref (-1) in
  (try while true do
    let line = input_line ic in
    let fields = String.split_on_char '\t' line in
    match fields with
    | "/" :: "SYM\xc3\xbc" :: idx_s :: _ | "/" :: "SYMuTAB" :: idx_s :: _ ->
      (match int_of_string_opt idx_s with
       | Some n -> cur_idx := n; if n > !max_idx then max_idx := n
       | None -> ())
    | "." :: "SYM_NAME" :: _ :: "CHARACTER" :: rest when !cur_idx >= 0 ->
      let s = String.concat "\t" rest in
      let name =
        if String.length s >= 2 && s.[0] = '\'' then
          let s = String.sub s 1 (String.length s - 1) in
          match String.index_opt s '\'' with
          | Some i -> String.sub s 0 i
          | None -> s
        else s
      in
      if name <> "" then Hashtbl.replace tbl !cur_idx name
    | _ -> ()
  done with End_of_file -> ());
  close_in ic;
  let n = !max_idx + 1 in
  let names = Array.make n "" in
  Hashtbl.iter (fun i name -> if i < n then names.(i) <- name) tbl;
  { names; sym_count = n }

let syt_annotation sym idx =
  if idx >= 0 && idx < sym.sym_count && sym.names.(idx) <> "" then
    Printf.sprintf "=%s" sym.names.(idx)
  else ""

let opcode_names = Hashtbl.create 256

let () =
  List.iter (fun (code, name) -> Hashtbl.replace opcode_names code name) [
    (* CLASS 0: Control/Subscript *)
    0x000, "NOP";    0x001, "EXTN";   0x002, "XREC";   0x003, "IMRK";
    0x004, "SMRK";   0x005, "PXRC";   0x007, "IFHD";
    0x008, "LBL";    0x009, "BRA";    0x00A, "FBRA";
    0x00B, "DCAS";   0x00C, "ECAS";   0x00D, "CLBL";
    0x00E, "DTST";   0x00F, "ETST";
    0x010, "DFOR";   0x011, "EFOR";   0x012, "CFOR";
    0x013, "DSMP";   0x014, "ESMP";   0x015, "AFOR";
    0x016, "CTST";   0x017, "ADLP";   0x018, "DLPE";
    0x019, "DSUB";   0x01A, "IDLP";   0x01B, "TSUB";
    0x01D, "PCAL";   0x01E, "FCAL";
    0x01F, "READ";   0x020, "RDAL";   0x021, "WRIT";   0x022, "FILE";
    0x025, "XXST";   0x026, "XXND";   0x027, "XXAR";
    0x02A, "TDEF";   0x02B, "MDEF";   0x02C, "FDEF";
    0x02D, "PDEF";   0x02E, "UDEF";   0x02F, "CDEF";
    0x030, "CLOS";   0x031, "EDCL";   0x032, "RTRN";
    0x033, "TDCL";   0x034, "WAIT";   0x035, "SGNL";
    0x036, "CANC";   0x037, "TERM";   0x038, "PRIO";   0x039, "SCHD";
    0x03C, "ERON";   0x03D, "ERSE";
    0x040, "MSHP";   0x041, "VSHP";   0x042, "SSHP";   0x043, "ISHP";
    0x045, "SFST";   0x046, "SFND";   0x047, "SFAR";
    0x04A, "BFNC";   0x04B, "LFNC";
    0x04D, "TNEQ";   0x04E, "TEQU";   0x04F, "TASN";
    0x051, "IDEF";   0x052, "ICLS";
    0x055, "NNEQ";   0x056, "NEQU";   0x057, "NASN";
    0x059, "PMHD";   0x05A, "PMAR";   0x05B, "PMIN";
    (* CLASS 1: Bit *)
    0x101, "BASN";   0x102, "BAND";   0x103, "BOR";
    0x104, "BNOT";   0x105, "BCAT";
    0x121, "BTOB";   0x122, "BTOQ";
    0x141, "CTOB";   0x142, "CTOQ";
    0x1A1, "STOB";   0x1A2, "STOQ";
    0x1C1, "ITOB";   0x1C2, "ITOQ";
    (* CLASS 2: Character *)
    0x201, "CASN";   0x202, "CCAT";
    0x221, "BTOC";   0x241, "CTOC";
    0x2A1, "STOC";   0x2C1, "ITOC";
    (* CLASS 3: Matrix *)
    0x301, "MASN";   0x329, "MTRA";   0x3CA, "MINV";
    0x368, "MMPR";   0x3A5, "MSPR";   0x3A6, "MSDV";
    0x362, "MADD";   0x363, "MSUB";   0x344, "MNEG";
    0x341, "MTOM";   0x371, "MDET";   0x373, "MIDN";
    0x387, "VVPR";   0x46C, "MVPR";
    (* CLASS 4: Vector *)
    0x401, "VASN";   0x46D, "VMPR";   0x4A5, "VSPR";
    0x58E, "VDOT";   0x48B, "VCRS";
    0x482, "VADD";   0x483, "VSUB";   0x444, "VNEG";   0x441, "VTOV";
    (* CLASS 5: Scalar *)
    0x501, "SASN";   0x5AB, "SADD";   0x5AC, "SSUB";
    0x5AD, "SSPR";   0x5AE, "SSDV";   0x5AF, "SEXP";   0x5B0, "SNEG";
    0x571, "SIEX";   0x572, "SPEX";   0x5A1, "STOS";   0x5C1, "ITOS";
    0x521, "BTOS";   0x541, "CTOS";
    (* CLASS 6: Integer *)
    0x601, "IASN";   0x6CB, "IADD";   0x6CC, "ISUB";
    0x6CD, "IIPR";   0x6D0, "INEG";   0x6D2, "IPEX";   0x6C1, "ITOI";
    0x6A1, "STOI";   0x621, "BTOI";   0x641, "CTOI";
    (* CLASS 7: Conditional *)
    0x720, "BTRU";
    0x726, "BEQU";   0x725, "BNEQ";
    0x746, "CEQU";   0x745, "CNEQ";   0x74A, "CLT";
    0x748, "CGT";    0x747, "CNGT";   0x749, "CNLT";
    0x766, "MEQU";   0x765, "MNEQ";
    0x786, "VEQU";   0x785, "VNEQ";
    0x7A6, "SEQU";   0x7A5, "SNEQ";   0x7AA, "SLT";
    0x7A8, "SGT";    0x7A7, "SNGT";   0x7A9, "SNLT";
    0x7C6, "IEQU";   0x7C5, "INEQ";   0x7CA, "ILT";
    0x7C8, "IGT";    0x7C7, "INGT";   0x7C9, "INLT";
    0x7E2, "CAND";   0x7E3, "COR";    0x7E4, "CNOT";
    (* CLASS 8: Initialization *)
    0x801, "STRI";   0x802, "SLRI";   0x803, "ELRI";   0x804, "ETRI";
    0x821, "BINT";   0x841, "CINT";   0x861, "MINT";   0x881, "VINT";
    0x8A1, "SINT";   0x8C1, "IINT";
    0x8E1, "NINT";   0x8E2, "TINT";   0x8E3, "EINT";
  ]

let class_names = [|
  "CTRL"; "BIT"; "CHAR"; "MAT"; "VEC"; "SCAL"; "INT"; "COND"; "INIT"
|]

let qual_names = [|
  "---"; "SYT"; "INL"; "VAC"; "XPT"; "LIT"; "IMD"; "AST";
  "CSZ"; "ASZ"; "OFF"; "Q11"; "Q12"; "Q13"; "Q14"; "Q15"
|]

let read_word_be ic =
  let b0 = input_byte ic in
  let b1 = input_byte ic in
  let b2 = input_byte ic in
  let b3 = input_byte ic in
  (b0 lsl 24) lor (b1 lsl 16) lor (b2 lsl 8) lor b3

let read_block ic =
  let words = Array.make 1800 0 in
  (try
    for i = 0 to 1799 do
      words.(i) <- read_word_be ic
    done
  with End_of_file -> ());
  words

type decoded =
  | Operator of { tag: int; numop: int; popcode: int; copt: int }
  | Operand of { data: int; tag1: int; qual: int; tag2: int }

let decode_word w =
  if w land 1 = 0 then
    Operator {
      tag     = (w lsr 24) land 0xFF;
      numop   = (w lsr 16) land 0xFF;
      popcode = (w lsr 4) land 0xFFF;
      copt    = (w lsr 1) land 0x7;
    }
  else
    Operand {
      data = (w lsr 16) land 0xFFFF;
      tag1 = (w lsr 8) land 0xFF;
      qual = (w lsr 4) land 0xF;
      tag2 = (w lsr 1) land 0x7;
    }

let opcode_name code =
  match Hashtbl.find_opt opcode_names code with
  | Some s -> s
  | None -> Printf.sprintf "?%03X" code

let class_name cls =
  if cls >= 0 && cls < Array.length class_names then class_names.(cls)
  else Printf.sprintf "C%d" cls

let qual_name q =
  if q >= 0 && q < Array.length qual_names then qual_names.(q)
  else Printf.sprintf "Q%d" q

let disasm_block lit sym blk_num words =
  let w1 = words.(1) in
  let atom_fault = (w1 lsr 16) land 0xFFFF in
  Printf.printf "=== BLOCK %d === (%d atoms, words 2..%d)\n\n"
    blk_num atom_fault atom_fault;
  Printf.printf "  %-5s  %-10s  %-6s  %-5s  %-6s  %s\n"
    "ADDR" "RAW" "TYPE" "TAG" "COPT" "DECODED";
  Printf.printf "  %s\n" (String.make 70 '-');
  let i = ref 2 in
  while !i <= atom_fault do
    let w = words.(!i) in
    match decode_word w with
    | Operator { tag; numop; popcode; copt } ->
      let cls = (popcode lsr 8) land 0xF in
      let name = opcode_name popcode in
      let tag_str = if tag > 0 then Printf.sprintf "T=%d" tag else "" in
      let copt_str = if copt > 0 then Printf.sprintf "C=%d" copt else "" in
      Printf.printf "  %4d:  %08X  %-6s  %-5s  %-6s  %s  (%s/%s, %d ops)\n"
        !i w (class_name cls) tag_str copt_str
        name (class_name cls) name numop;
      let j = ref 1 in
      while !j <= numop && !i + !j <= atom_fault do
        let ow = words.(!i + !j) in
        (match decode_word ow with
        | Operand { data; tag1; qual; tag2 } ->
          let q = qual_name qual in
          let tag_info =
            if tag1 > 0 || tag2 > 0 then
              Printf.sprintf " [T1=%d T2=%d]" tag1 tag2
            else ""
          in
          let annot =
            match qual with
            | 5 -> (match lit with Some lt -> lit_annotation lt data | None -> "")
            | 1 -> (match sym with Some st -> syt_annotation st data | None -> "")
            | _ -> ""
          in
          Printf.printf "         %08X    op%-2d               %s(%d)%s%s\n"
            ow !j q data annot tag_info
        | Operator _ ->
          Printf.printf "         %08X    op%-2d               <unexpected operator>\n"
            ow !j);
        incr j
      done;
      i := !i + numop + 1
    | Operand { data; tag1; qual; tag2 } ->
      Printf.printf "  %4d:  %08X  STRAY               %s(%d) [T1=%d T2=%d]\n"
        !i w (qual_name qual) data tag1 tag2;
      incr i
  done;
  Printf.printf "\n"

let () =
  let files = ref [] in
  let raw = ref false in
  let litfile = ref "" in
  let common = ref "" in
  let specs = [
    ("--raw", Arg.Set raw, "Dump raw words (no decode)");
    ("--litfile", Arg.Set_string litfile, "Literal table (resolves LIT references)");
    ("--common", Arg.Set_string common, "COMMON dump (resolves SYT references)");
  ] in
  Arg.parse specs (fun f -> files := f :: !files)
    "disasm [--raw] [--litfile FILE] [--common FILE] FILE...";
  let files = List.rev !files in
  if files = [] then begin
    Printf.eprintf "Usage: disasm [--raw] [--litfile FILE] [--common FILE] FILE...\n";
    Printf.eprintf "Decodes HALMAT binary blocks (1800 x 32-bit words per block)\n";
    exit 1
  end;
  let lit = if !litfile <> "" then Some (read_litfile !litfile) else None in
  let sym = if !common <> "" then Some (read_symtab !common) else None in
  List.iter (fun filename ->
    let ic = open_in_bin filename in
    let size = in_channel_length ic in
    let nblocks = (size + 7199) / 7200 in
    Printf.printf "HALMAT DISASSEMBLY: %s\n" (Filename.basename filename);
    Printf.printf "%d bytes, %d block(s)\n\n" size nblocks;
    if !raw then begin
      for blk = 0 to nblocks - 1 do
        let words = read_block ic in
        Printf.printf "--- Block %d ---\n" blk;
        for i = 0 to 1799 do
          if words.(i) <> 0 then
            Printf.printf "  %4d: %08X\n" i words.(i)
        done
      done
    end else begin
      for blk = 0 to nblocks - 1 do
        let words = read_block ic in
        disasm_block lit sym blk words
      done
    end;
    close_in ic
  ) files
