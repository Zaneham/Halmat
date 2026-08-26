(* HALMAT pattern analyzer.
   Walks the HALMAT stream and identifies control flow groups:
   - FOR loops (DFOR..EFOR, with optional CFOR/AFOR)
   - IF/ELSE (FBRA..BRA..LBL)
   - DO CASE (DCAS..CLBL..ECAS)
   - DO WHILE/UNTIL (DTST..CTST..ETST)
   - Simple DO (DSMP..ESMP)
   - I/O groups (XXST..XXAR..WRIT/READ..XXND)
   - Procedure/Function defs (PDEF/FDEF..CLOS)
   - Procedure/Function calls (XXST..XXAR..PCAL/FCAL..XXND)
   - Program/Module defs (MDEF..CLOS)
   - Initialization (CINT/IINT/SINT etc.)

   Uses INL flow labels to match start/end pairs. *)

(* ---- Binary reading (shared with disasm) ---- *)

let lit_page_size = 130

type lit_table = {
  lit1: int array;
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

let format_float v =
  let s = Printf.sprintf "%.10g" v in
  if String.contains s '.' || String.contains s 'e' then s
  else s ^ ".0"

let lit_value lit idx =
  if idx < 0 || idx >= lit.count then "<out of range>"
  else
    let typ = lit.lit1.(idx) in
    let v2 = lit.lit2.(idx) in
    match typ with
    | 0 ->
      let len = ((v2 lsr 24) land 0xFF) + 1 in
      Printf.sprintf "CHAR(%d)" len
    | 1 ->
      let v = decode_ibm_float v2 in
      format_float v
    | 2 ->
      Printf.sprintf "BIT'%X'" v2
    | _ ->
      Printf.sprintf "?type%d" typ

(* Symbol table from COMMON dump *)
type sym_table = { names: string array; sym_count: int }

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

let sym_name sym idx =
  if idx >= 0 && idx < sym.sym_count && sym.names.(idx) <> "" then
    sym.names.(idx)
  else Printf.sprintf "#%d" idx

(* ---- HALMAT decoding ---- *)

type operator = {
  addr: int;
  tag: int;
  numop: int;
  popcode: int;
  copt: int;
  operands: operand array;
}
and operand = {
  data: int;
  tag1: int;
  qual: int;
  tag2: int;
}

let qual_name = function
  | 0 -> "---" | 1 -> "SYT" | 2 -> "INL" | 3 -> "VAC"
  | 4 -> "XPT" | 5 -> "LIT" | 6 -> "IMD" | 7 -> "AST"
  | 8 -> "CSZ" | 9 -> "ASZ" | 10 -> "OFF" | q -> Printf.sprintf "Q%d" q

(* Opcode constants *)
let xnop  = 0x000 let xextn = 0x001 let xxrec = 0x002
let ximrk = 0x003 let xsmrk = 0x004 let xpxrc = 0x005
let xifhd = 0x007 let xlbl  = 0x008 let xbra  = 0x009
let xfbra = 0x00A let xdcas = 0x00B let xecas = 0x00C
let xclbl = 0x00D let xdtst = 0x00E let xetst = 0x00F
let xdfor = 0x010 let xefor = 0x011 let xcfor = 0x012
let xdsmp = 0x013 let xesmp = 0x014 let xafor = 0x015
let xctst = 0x016
let xdsub = 0x019 let xidlp = 0x01A let xtsub = 0x01B
let xpcal = 0x01D let xfcal = 0x01E
let xread = 0x01F let xrdal = 0x020 let xwrit = 0x021
let xxfile = 0x022
let xxxst = 0x025 let xxxnd = 0x026 let xxxar = 0x027
let xtdef = 0x02A let xmdef = 0x02B let xfdef = 0x02C
let xpdef = 0x02D let xudef = 0x02E let xcdef = 0x02F
let xclos = 0x030 let xedcl = 0x031 let xrtrn = 0x032

let opcode_name = function
  | 0x000 -> "NOP"  | 0x001 -> "EXTN" | 0x002 -> "XREC"
  | 0x003 -> "IMRK" | 0x004 -> "SMRK" | 0x005 -> "PXRC"
  | 0x007 -> "IFHD" | 0x008 -> "LBL"  | 0x009 -> "BRA"
  | 0x00A -> "FBRA" | 0x00B -> "DCAS" | 0x00C -> "ECAS"
  | 0x00D -> "CLBL" | 0x00E -> "DTST" | 0x00F -> "ETST"
  | 0x010 -> "DFOR" | 0x011 -> "EFOR" | 0x012 -> "CFOR"
  | 0x013 -> "DSMP" | 0x014 -> "ESMP" | 0x015 -> "AFOR"
  | 0x016 -> "CTST"
  | 0x017 -> "ADLP" | 0x018 -> "DLPE"
  | 0x019 -> "DSUB" | 0x01A -> "IDLP" | 0x01B -> "TSUB"
  | 0x01D -> "PCAL" | 0x01E -> "FCAL"
  | 0x01F -> "READ" | 0x020 -> "RDAL" | 0x021 -> "WRIT"
  | 0x022 -> "FILE"
  | 0x025 -> "XXST" | 0x026 -> "XXND" | 0x027 -> "XXAR"
  | 0x02A -> "TDEF" | 0x02B -> "MDEF" | 0x02C -> "FDEF"
  | 0x02D -> "PDEF" | 0x02E -> "UDEF" | 0x02F -> "CDEF"
  | 0x030 -> "CLOS" | 0x031 -> "EDCL" | 0x032 -> "RTRN"
  | 0x033 -> "TDCL" | 0x034 -> "WAIT" | 0x035 -> "SGNL"
  | 0x036 -> "CANC" | 0x037 -> "TERM" | 0x038 -> "PRIO"
  | 0x039 -> "SCHD"
  | 0x03C -> "ERON" | 0x03D -> "ERSE"
  | 0x040 -> "MSHP" | 0x041 -> "VSHP" | 0x042 -> "SSHP"
  | 0x043 -> "ISHP"
  | 0x045 -> "SFST" | 0x046 -> "SFND" | 0x047 -> "SFAR"
  | 0x04A -> "BFNC" | 0x04B -> "LFNC"
  | 0x04D -> "TNEQ" | 0x04E -> "TEQU" | 0x04F -> "TASN"
  | 0x051 -> "IDEF" | 0x052 -> "ICLS"
  | 0x055 -> "NNEQ" | 0x056 -> "NEQU" | 0x057 -> "NASN"
  | 0x059 -> "PMHD" | 0x05A -> "PMAR" | 0x05B -> "PMIN"
  | c -> Printf.sprintf "?%03X" c

(* Parse a block of 1800 words into a list of operators *)
let parse_block words =
  let atom_fault = (words.(1) lsr 16) land 0xFFFF in
  let ops = ref [] in
  let i = ref 2 in
  while !i <= atom_fault do
    let w = words.(!i) in
    if w land 1 = 0 then begin
      let tag = (w lsr 24) land 0xFF in
      let numop = (w lsr 16) land 0xFF in
      let popcode = (w lsr 4) land 0xFFF in
      let copt = (w lsr 1) land 0x7 in
      let operands = Array.init numop (fun j ->
        let ow = words.(!i + j + 1) in
        { data = (ow lsr 16) land 0xFFFF;
          tag1 = (ow lsr 8) land 0xFF;
          qual = (ow lsr 4) land 0xF;
          tag2 = (ow lsr 1) land 0x7 }
      ) in
      ops := { addr = !i; tag; numop; popcode; copt; operands } :: !ops;
      i := !i + numop + 1
    end else
      incr i
  done;
  List.rev !ops

(* ---- Pattern types ---- *)

type pattern =
  | For_loop of {
      dfor: operator;
      efor: operator;
      cfor: operator option;  (* WHILE/UNTIL condition *)
      afors: operator list;   (* discrete values *)
      body: operator list;    (* operators between DFOR and EFOR *)
      dotype: int;            (* 0=discrete, 1=iter step=1, 2=iter explicit *)
      has_while: bool;
      has_until: bool;
    }
  | If_else of {
      fbra: operator;
      then_body: operator list;
      bra: operator option;   (* present if ELSE exists *)
      else_body: operator list;
      lbl: operator;          (* closing LBL *)
    }
  | Do_case of {
      dcas: operator;
      clbls: operator list;
      ecas: operator;
      cases: operator list list;  (* body per case *)
    }
  | Do_while of {
      dtst: operator;
      ctst: operator option;
      etst: operator;
      body: operator list;
      is_until: bool;
    }
  | Simple_do of {
      dsmp: operator;
      esmp: operator;
      body: operator list;
    }
  | Io_group of {
      xxst: operator;
      xxars: operator list;
      io_op: operator;     (* WRIT/READ/RDAL *)
      xxnd: operator;
    }
  | Block_def of {
      def_op: operator;   (* MDEF/PDEF/FDEF/TDEF/UDEF/CDEF *)
      clos: operator;
      body: operator list;
    }

(* ---- Pattern detection ---- *)

(* Get INL value from first operand *)
let get_inl op =
  if op.numop >= 1 && op.operands.(0).qual = 2 (* INL *)
  then Some op.operands.(0).data
  else None

(* Find matching EFOR for a DFOR by INL *)
let find_efor ops dfor_inl start_idx =
  let rec scan i = function
    | [] -> None
    | op :: rest ->
      if op.popcode = xefor then
        match get_inl op with
        | Some inl when inl = dfor_inl -> Some (i, op)
        | _ -> scan (i+1) rest
      else scan (i+1) rest
  in
  scan start_idx ops

(* Find matching ECAS for a DCAS *)
let find_ecas ops dcas_inl start_idx =
  let rec scan i = function
    | [] -> None
    | op :: rest ->
      if op.popcode = xecas then Some (i, op)
      else scan (i+1) rest
  in
  scan start_idx ops

(* Find matching ETST for a DTST *)
let find_etst ops start_idx =
  let rec scan i = function
    | [] -> None
    | op :: rest ->
      if op.popcode = xetst then Some (i, op)
      else scan (i+1) rest
  in
  scan start_idx ops

(* Find matching ESMP for a DSMP *)
let find_esmp ops start_idx =
  let rec scan i = function
    | [] -> None
    | op :: rest ->
      if op.popcode = xesmp then Some (i, op)
      else scan (i+1) rest
  in
  scan start_idx ops

(* Find matching LBL for an FBRA *)
let find_lbl ops fbra_inl start_idx =
  let rec scan i = function
    | [] -> None
    | op :: rest ->
      if op.popcode = xlbl then
        (match get_inl op with
         | Some inl when inl = fbra_inl -> Some (i, op)
         | _ -> scan (i+1) rest)
      else scan (i+1) rest
  in
  scan start_idx ops

(* Find XXND closing an XXST *)
let find_xxnd ops start_idx =
  let rec scan i = function
    | [] -> None
    | op :: rest ->
      if op.popcode = xxxnd then Some (i, op)
      else scan (i+1) rest
  in
  scan start_idx ops

(* Find CLOS matching a def *)
let find_clos ops def_syt start_idx =
  let rec scan i = function
    | [] -> None
    | op :: rest ->
      if op.popcode = xclos then Some (i, op)
      else scan (i+1) rest
  in
  scan start_idx ops

(* ---- Analysis output ---- *)

let operand_str ?(lit=None) ?(sym=None) op =
  let q = qual_name op.qual in
  let annot = match op.qual with
    | 5 -> (match lit with
            | Some lt -> " = " ^ lit_value lt op.data
            | None -> "")
    | 1 -> (match sym with
            | Some st -> " = " ^ sym_name st op.data
            | None -> "")
    | _ -> ""
  in
  Printf.sprintf "%s(%d)%s" q op.data annot

let print_operator ?(lit=None) ?(sym=None) indent op =
  let name = opcode_name op.popcode in
  let tag_s = if op.tag > 0 then Printf.sprintf " T=%d" op.tag else "" in
  Printf.printf "%s[%3d] %s%s" indent op.addr name tag_s;
  Array.iteri (fun i operand ->
    if i = 0 then Printf.printf "  "
    else Printf.printf ", ";
    Printf.printf "%s" (operand_str ~lit ~sym operand)
  ) op.operands;
  Printf.printf "\n"

let analyze_patterns ops lit sym =
  let n = Array.length ops in

  (* Collect all patterns found *)
  let patterns = ref [] in

  (* Walk the operator stream *)
  let i = ref 0 in
  while !i < n do
    let op = ops.(!i) in
    (match op.popcode with

    (* ---- FOR loop ---- *)
    | p when p = xdfor ->
      let dotype = op.tag land 3 in
      let has_while = (op.tag lsr 4) land 1 = 1 in
      let has_until = (op.tag lsr 4) land 1 = 2 in
      let inl = match get_inl op with Some v -> v | None -> -1 in
      (* Find matching EFOR *)
      let body = ref [] in
      let cfor_op = ref None in
      let afors = ref [] in
      let j = ref (!i + 1) in
      let efor_op = ref None in
      while !j < n && !efor_op = None do
        let jop = ops.(!j) in
        if jop.popcode = xefor then begin
          match get_inl jop with
          | Some v when v = inl -> efor_op := Some jop
          | _ -> body := jop :: !body; incr j
        end else begin
          if jop.popcode = xcfor then cfor_op := Some jop;
          if jop.popcode = xafor then afors := jop :: !afors;
          body := jop :: !body;
          incr j
        end
      done;
      (match !efor_op with
       | Some efor ->
         patterns := For_loop {
           dfor = op; efor;
           cfor = !cfor_op;
           afors = List.rev !afors;
           body = List.rev !body;
           dotype;
           has_while; has_until;
         } :: !patterns;
         i := !j + 1
       | None -> incr i)

    (* ---- IF/ELSE ---- *)
    | p when p = xfbra ->
      let inl = match get_inl op with Some v -> v | None -> -1 in
      (* Scan for matching LBL *)
      let then_body = ref [] in
      let bra_op = ref None in
      let else_body = ref [] in
      let in_else = ref false in
      let j = ref (!i + 1) in
      let lbl_op = ref None in
      while !j < n && !lbl_op = None do
        let jop = ops.(!j) in
        if jop.popcode = xlbl && jop.tag = 1 then begin
          (* Check if this is the exit LBL *)
          match get_inl jop with
          | Some v ->
            (* If we have a BRA, the exit LBL has the BRA's INL *)
            let exit_inl = match !bra_op with
              | Some b -> (match get_inl b with Some v -> v | None -> inl)
              | None -> inl
            in
            if v = exit_inl || v = inl then
              lbl_op := Some jop
            else begin
              if !in_else then else_body := jop :: !else_body
              else then_body := jop :: !then_body;
              incr j
            end
          | None -> incr j
        end else if jop.popcode = xlbl && jop.tag = 0 then begin
          (* False-branch LBL — start of ELSE *)
          match get_inl jop with
          | Some v when v = inl ->
            in_else := true;
            incr j
          | _ ->
            if !in_else then else_body := jop :: !else_body
            else then_body := jop :: !then_body;
            incr j
        end else if jop.popcode = xbra && not !in_else then begin
          bra_op := Some jop;
          incr j
        end else begin
          if !in_else then else_body := jop :: !else_body
          else then_body := jop :: !then_body;
          incr j
        end
      done;
      (match !lbl_op with
       | Some lbl ->
         patterns := If_else {
           fbra = op;
           then_body = List.rev !then_body;
           bra = !bra_op;
           else_body = List.rev !else_body;
           lbl;
         } :: !patterns;
         i := !j + 1
       | None -> incr i)

    (* ---- I/O group ---- *)
    | p when p = xxxst ->
      let j = ref (!i + 1) in
      let xxars = ref [] in
      let io_op = ref None in
      let xxnd_op = ref None in
      while !j < n && !xxnd_op = None do
        let jop = ops.(!j) in
        if jop.popcode = xxxar then
          xxars := jop :: !xxars
        else if jop.popcode = xwrit || jop.popcode = xread || jop.popcode = xrdal then
          io_op := Some jop
        else if jop.popcode = xxxnd then
          xxnd_op := Some jop;
        incr j
      done;
      (match !io_op, !xxnd_op with
       | Some io, Some xxnd ->
         patterns := Io_group {
           xxst = op;
           xxars = List.rev !xxars;
           io_op = io;
           xxnd = xxnd;
         } :: !patterns;
         i := !j
       | _ -> incr i)

    (* ---- DO WHILE/UNTIL ---- *)
    | p when p = xdtst ->
      let j = ref (!i + 1) in
      let body = ref [] in
      let ctst_op = ref None in
      let etst_op = ref None in
      while !j < n && !etst_op = None do
        let jop = ops.(!j) in
        if jop.popcode = xetst then etst_op := Some jop
        else begin
          if jop.popcode = xctst then ctst_op := Some jop;
          body := jop :: !body
        end;
        incr j
      done;
      (match !etst_op with
       | Some etst ->
         patterns := Do_while {
           dtst = op;
           ctst = !ctst_op;
           etst;
           body = List.rev !body;
           is_until = op.tag land 1 = 1;
         } :: !patterns;
         i := !j
       | None -> incr i)

    (* ---- DO CASE ---- *)
    | p when p = xdcas ->
      let j = ref (!i + 1) in
      let clbls = ref [] in
      let body = ref [] in
      let ecas_op = ref None in
      while !j < n && !ecas_op = None do
        let jop = ops.(!j) in
        if jop.popcode = xecas then ecas_op := Some jop
        else begin
          if jop.popcode = xclbl then clbls := jop :: !clbls;
          body := jop :: !body
        end;
        incr j
      done;
      (match !ecas_op with
       | Some ecas ->
         patterns := Do_case {
           dcas = op;
           clbls = List.rev !clbls;
           ecas;
           cases = [List.rev !body]; (* simplified: all body as one list *)
         } :: !patterns;
         i := !j
       | None -> incr i)

    (* ---- Simple DO ---- *)
    | p when p = xdsmp ->
      let j = ref (!i + 1) in
      let body = ref [] in
      let esmp_op = ref None in
      while !j < n && !esmp_op = None do
        let jop = ops.(!j) in
        if jop.popcode = xesmp then esmp_op := Some jop
        else body := jop :: !body;
        incr j
      done;
      (match !esmp_op with
       | Some esmp ->
         patterns := Simple_do {
           dsmp = op; esmp; body = List.rev !body;
         } :: !patterns;
         i := !j
       | None -> incr i)

    (* ---- Block def (MDEF/PDEF/FDEF/TDEF/UDEF/CDEF) ---- *)
    | p when p = xmdef || p = xpdef || p = xfdef ||
             p = xtdef || p = xudef || p = xcdef ->
      let j = ref (!i + 1) in
      let body = ref [] in
      let clos_op = ref None in
      while !j < n && !clos_op = None do
        let jop = ops.(!j) in
        if jop.popcode = xclos then clos_op := Some jop
        else body := jop :: !body;
        incr j
      done;
      (match !clos_op with
       | Some clos ->
         patterns := Block_def {
           def_op = op; clos; body = List.rev !body;
         } :: !patterns;
         i := !j
       | None -> incr i)

    | _ -> incr i);
  done;
  List.rev !patterns

(* ---- Pretty-print patterns ---- *)

let dfor_type_str dotype =
  match dotype with
  | 0 -> "discrete (value list)"
  | 1 -> "iterative (implicit BY 1)"
  | 2 -> "iterative (explicit BY)"
  | _ -> Printf.sprintf "unknown(%d)" dotype

let def_type_str popcode =
  match popcode with
  | p when p = xmdef -> "PROGRAM/MODULE"
  | p when p = xpdef -> "PROCEDURE"
  | p when p = xfdef -> "FUNCTION"
  | p when p = xtdef -> "TASK"
  | p when p = xudef -> "UPDATE"
  | p when p = xcdef -> "COMPOOL"
  | _ -> "BLOCK"

let io_type_str popcode =
  match popcode with
  | p when p = xwrit -> "WRITE"
  | p when p = xread -> "READ"
  | p when p = xrdal -> "READALL"
  | _ -> "I/O"

let get_body_ops = function
  | For_loop { body; _ } -> body
  | If_else { then_body; else_body; _ } -> then_body @ else_body
  | Do_case { cases; _ } -> List.concat cases
  | Do_while { body; _ } -> body
  | Simple_do { body; _ } -> body
  | Io_group _ -> []
  | Block_def { body; _ } -> body

let rec print_pattern lit sym depth pat =
  let indent = String.make (depth * 2) ' ' in
  let body_ops = get_body_ops pat in
  let sub_patterns = analyze_patterns (Array.of_list body_ops) lit sym in
  (match pat with
  | For_loop { dfor; efor; cfor; afors; body; dotype; has_while; has_until } ->
    Printf.printf "%s┌── FOR LOOP (%s)%s%s\n"
      indent (dfor_type_str dotype)
      (if has_while then " + WHILE" else "")
      (if has_until then " + UNTIL" else "");
    Printf.printf "%s│   addr %d..%d\n" indent dfor.addr efor.addr;
    if dfor.numop >= 2 then
      Printf.printf "%s│   loop_var: %s\n" indent
        (operand_str ~lit:(Some lit) ~sym:(Some sym) dfor.operands.(1));
    if dfor.numop >= 3 then
      Printf.printf "%s│   from:     %s\n" indent
        (operand_str ~lit:(Some lit) ~sym:(Some sym) dfor.operands.(2));
    if dfor.numop >= 4 then
      Printf.printf "%s│   to:       %s\n" indent
        (operand_str ~lit:(Some lit) ~sym:(Some sym) dfor.operands.(3));
    if dfor.numop >= 5 then
      Printf.printf "%s│   by:       %s\n" indent
        (operand_str ~lit:(Some lit) ~sym:(Some sym) dfor.operands.(4));
    if afors <> [] then begin
      Printf.printf "%s│   values:   " indent;
      List.iter (fun a ->
        if a.numop >= 1 then
          Printf.printf "%s  " (operand_str ~lit:(Some lit) ~sym:(Some sym) a.operands.(0))
      ) afors;
      Printf.printf "\n"
    end;
    (match cfor with
     | Some c -> Printf.printf "%s│   condition: %s at addr %d\n" indent
         (if has_until then "UNTIL" else "WHILE") c.addr
     | None -> ());
    Printf.printf "%s│   INL: %d (exit label)\n" indent
      (match get_inl dfor with Some v -> v | None -> -1);
    Printf.printf "%s│   body: %d operators\n" indent (List.length body);
    List.iter (print_pattern lit sym (depth+1)) sub_patterns;
    Printf.printf "%s└── EFOR at addr %d\n" indent efor.addr

  | If_else { fbra; then_body; bra; else_body; lbl } ->
    let has_else = bra <> None in
    Printf.printf "%s┌── IF%s\n" indent (if has_else then "/ELSE" else "");
    Printf.printf "%s│   addr %d..%d\n" indent fbra.addr lbl.addr;
    if fbra.numop >= 2 then
      Printf.printf "%s│   condition: %s\n" indent
        (operand_str ~lit:(Some lit) ~sym:(Some sym) fbra.operands.(1));
    Printf.printf "%s│   then: %d operators\n" indent (List.length then_body);
    if has_else then
      Printf.printf "%s│   else: %d operators\n" indent (List.length else_body);
    List.iter (print_pattern lit sym (depth+1)) sub_patterns;
    Printf.printf "%s└── LBL at addr %d\n" indent lbl.addr

  | Do_case { dcas; clbls; ecas; _ } ->
    Printf.printf "%s┌── DO CASE\n" indent;
    Printf.printf "%s│   addr %d..%d\n" indent dcas.addr ecas.addr;
    if dcas.numop >= 2 then
      Printf.printf "%s│   selector: %s\n" indent
        (operand_str ~lit:(Some lit) ~sym:(Some sym) dcas.operands.(1));
    Printf.printf "%s│   cases: %d (CLBL count)\n" indent (List.length clbls);
    List.iter (print_pattern lit sym (depth+1)) sub_patterns;
    Printf.printf "%s└── ECAS at addr %d\n" indent ecas.addr

  | Do_while { dtst; ctst; etst; body; is_until } ->
    Printf.printf "%s┌── DO %s\n" indent (if is_until then "UNTIL" else "WHILE");
    Printf.printf "%s│   addr %d..%d\n" indent dtst.addr etst.addr;
    (match ctst with
     | Some c ->
       if c.numop >= 1 then
         Printf.printf "%s│   condition: %s\n" indent
           (operand_str ~lit:(Some lit) ~sym:(Some sym) c.operands.(0))
     | None -> ());
    Printf.printf "%s│   body: %d operators\n" indent (List.length body);
    List.iter (print_pattern lit sym (depth+1)) sub_patterns;
    Printf.printf "%s└── ETST at addr %d\n" indent etst.addr

  | Simple_do { dsmp; esmp; body } ->
    Printf.printf "%s┌── SIMPLE DO\n" indent;
    Printf.printf "%s│   addr %d..%d, body: %d operators\n" indent
      dsmp.addr esmp.addr (List.length body);
    List.iter (print_pattern lit sym (depth+1)) sub_patterns;
    Printf.printf "%s└── ESMP at addr %d\n" indent esmp.addr

  | Io_group { xxst; xxars; io_op; xxnd } ->
    Printf.printf "%s┌── %s (unit %s)\n" indent (io_type_str io_op.popcode)
      (if io_op.numop >= 1 then
         operand_str ~lit:(Some lit) ~sym:(Some sym) io_op.operands.(0)
       else "?");
    Printf.printf "%s│   addr %d..%d\n" indent xxst.addr xxnd.addr;
    List.iter (fun ar ->
      if ar.numop >= 1 then
        Printf.printf "%s│   arg: %s\n" indent
          (operand_str ~lit:(Some lit) ~sym:(Some sym) ar.operands.(0))
    ) xxars;
    Printf.printf "%s└── XXND at addr %d\n" indent xxnd.addr

  | Block_def { def_op; clos; body } ->
    let name = if def_op.numop >= 1 && def_op.operands.(0).qual = 1 then
      sym_name sym def_op.operands.(0).data
    else "?" in
    Printf.printf "%s┌── %s %s\n" indent (def_type_str def_op.popcode) name;
    Printf.printf "%s│   addr %d..%d, body: %d operators\n" indent
      def_op.addr clos.addr (List.length body);
    List.iter (print_pattern lit sym (depth+1)) sub_patterns;
    Printf.printf "%s└── CLOS at addr %d\n" indent clos.addr)

(* ---- Main ---- *)

let read_word_be ic =
  let b0 = input_byte ic in
  let b1 = input_byte ic in
  let b2 = input_byte ic in
  let b3 = input_byte ic in
  (b0 lsl 24) lor (b1 lsl 16) lor (b2 lsl 8) lor b3

let read_block ic =
  let words = Array.make 1800 0 in
  (try for i = 0 to 1799 do words.(i) <- read_word_be ic done
   with End_of_file -> ());
  words

let () =
  let files = ref [] in
  let litfile = ref "" in
  let common = ref "" in
  let specs = [
    ("--litfile", Arg.Set_string litfile, "Literal table file");
    ("--common", Arg.Set_string common, "COMMON dump (symbol table)");
  ] in
  Arg.parse specs (fun f -> files := f :: !files)
    "patterns [--litfile FILE] [--common FILE] FILE...";
  let files = List.rev !files in
  if files = [] then begin
    Printf.eprintf "Usage: patterns [--litfile FILE] [--common FILE] FILE...\n";
    Printf.eprintf "Identifies control flow patterns in HALMAT binary.\n";
    exit 1
  end;
  let lit = if !litfile <> "" then read_litfile !litfile
    else { lit1 = [||]; lit2 = [||]; lit3 = [||]; count = 0 } in
  let sym = if !common <> "" then read_symtab !common
    else { names = [||]; sym_count = 0 } in
  List.iter (fun filename ->
    let ic = open_in_bin filename in
    let size = in_channel_length ic in
    let nblocks = (size + 7199) / 7200 in
    Printf.printf "HALMAT PATTERN ANALYSIS: %s\n" (Filename.basename filename);
    Printf.printf "%d bytes, %d block(s)\n\n" size nblocks;
    for blk = 0 to nblocks - 1 do
      let words = read_block ic in
      let atom_fault = (words.(1) lsr 16) land 0xFFFF in
      Printf.printf "=== BLOCK %d === (%d atoms)\n\n" blk atom_fault;
      let ops = parse_block words in
      let ops_arr = Array.of_list ops in
      let patterns = analyze_patterns ops_arr lit sym in
      List.iter (print_pattern lit sym 0) patterns;
      Printf.printf "\n"
    done;
    close_in ic
  ) files
