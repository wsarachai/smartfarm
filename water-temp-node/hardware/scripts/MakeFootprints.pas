{------------------------------------------------------------------------------
  MakeFootprints.pas  --  water-temp-node front-end PCB

  Builds the ten footprints that no vendor library covers, into the currently
  open PCB library.  Everything else (D1-D6, Q2, U3, U7) comes from the vendor
  libraries in hardware/lib/ -- see docs/pcb-altium.md section 3.5.
  Nothing is left after this runs.

  THIS FILE HAS EXACTLY TWO THINGS YOU CAN RUN
      CheckEnvironment   run this FIRST.  It touches nothing and tells you
                         whether the script engine works and which PCB library
                         is in front.
      MakeFootprints     the real one.
  Everything else takes a parameter, so Altium will not offer it in the Run
  Script list and you cannot start it by accident.  (An earlier version of this
  file had five runnable entry points and picking the wrong one did nothing at
  all, silently.)

  HOW TO RUN
    1. File > Open...  and open  WaterTempNode_Scripts.PrjScr  (next to this
       file).  It appears in the Projects panel with this script inside it.
       Altium will not run a loose .pas: the Run Script browser filters for
       "Script Project Files (*.PrjScr)", and the script list itself is built
       from open projects and open free documents, not from disk.
       (Alternative: File > Open..., set the file type to All Files (*.*), and
       open this .pas directly.  It opens as a free document and shows up in
       the list just the same.)
    2. Create the library if it does not exist yet:
       File > New > Library > PCB Library, then File > Save As...
       WaterTempNode_FE.PcbLib, in the same folder as WaterTempNode_FE.PrjPcb.
    3. Click the WaterTempNode_FE.PcbLib tab so it is the ACTIVE document.
       The script writes into whichever PCB library is in front.  If the script
       editor is in front instead, there is no active library and nothing can
       be created.
    4. DXP > Run Script...   -- the DXP menu is the leftmost item on the menu
       bar, before File, and Run Script is the LAST entry in it.  Not Tools.
       Run CheckEnvironment first, then MakeFootprints.
    5. Look in the PCB Library panel, not the drawing area.  New footprints
       appear in that list; the canvas keeps showing whichever one was already
       open.
    6. Check every footprint against the datasheet before using it.  Printing
       the library at 1:1 and laying the real parts on the paper is the check
       that actually catches errors (docs/pcb-altium.md section 3.7).

  WHERE THE NUMBERS COME FROM
    Every pad dimension below is quoted from a manufacturer document, named in
    the comment above it.  Nothing here was estimated.  Where a number could not
    be cited it is marked CHOSEN at the place it is used, and there are five:
      - the 2.2 mm pad diameter on the Phoenix headers (the datasheet gives the
        drilled hole, not the pad)
      - the 0.9 mm hole and 1.8 mm pad on the electrolytic
      - the 1.0 mm hole and 1.8 mm pad on J7
      - the 1.5 mm hole and 3.0 mm pad on the fuse holder, deliberately oversized
        because no vendor in that class publishes a terminal thickness
      - the 1.5 mm test pad
    Two OUTLINES are provisional rather than cited, and both matter for the board
    outline: J7's shroud and the fuse holder's body.

  THE HAND-SOLDER RULE
    This board is hand-soldered in a batch of five.  Every surface-mount land
    below is the manufacturer's recommended land with the OUTER end of each pad
    extended by 0.25 mm, which is where the iron tip and the solder go.  The gap
    between pads is never widened -- that gap is what makes the part self-align.
    The nominal (unextended) numbers are in the comments so both are checkable.
    This is the same idea as the "-M" (IPC density level Most) variants that
    ship in the CDSOD323 and DMP6023LE libraries.

  ON TRUSTING THIS FILE
    The dimensions are cited and can be checked against the sources.  The Altium
    API calls are the standard DelphiScript idiom; if any of them is wrong for
    this build, the script fails loudly -- a compile error or an exception, with
    nothing written.  That is the opposite of a wrong dimension, which succeeds
    silently and shows up when the part will not sit on the board.

  SOURCES
    0805      Vishay doc 28950, "Recommended Solder Pad Dimensions",
              rev. 12-Jul-2022, table "Based on IPC-7351, reflow soldering":
              G 1.00, Y 0.90, X 1.45, Z 2.80 mm.  Z = G + 2Y confirms the
              reading: 1.00 + 1.80 = 2.80.
    SOT-23    Alpha & Omega Semiconductor document PO-00001 rev. N,
              "SOT23 PACKAGE OUTLINE", RECOMMENDED LAND PATTERN:
              pads 0.80 x 0.80, bottom pads +/-0.95 from the centreline,
              rows 2.40 mm centre to centre.
    SMB       Bourns SMBJ Transient Voltage Suppressor Diode Series datasheet,
              "Recommended Footprint": A (gap) 2.69 max, B (pad height) 2.10
              min, C (pad width) 1.27 min.  Package is DO-214AA.
    Phoenix   Phoenix Contact MC 1,5/3-G-3,5, order no. 1844223, datasheet
              page 6 "Drilling plan/solder pad geometry": hole dia. 1.2 mm,
              pitch 3.5 mm, 2.45 mm beyond the outermost pin centre.
              Page 3 "Dimensions": width [w] 11.9 mm, length [l] 9.2 mm.
              The width confirms the drilling plan arithmetic exactly:
              (3-1) x 3.5 + 2 x 2.45 = 11.9 mm.  Page 7 states the rule as
              "a + 4,9", so the 4-pole part is 3 x 3.5 + 4.9 = 15.4 mm and the
              2-pole part (MC 1,5/2-G-3,5, order 1844210, 8 A) is 8.4 mm.
    Radial    Nichicon UVR series catalogue CAT.8100M: part code 1J = 63 V,
              101 = 100 uF, case code PD = 8 x 11.5 mm; and the lead table gives
              P = 3.5 mm, d = 0.6 mm for a diameter of 8 mm.
    Fuse      22.6 mm is the de-facto pitch for covered 5x20 PCB holders -- the
              PTF-78 family states it, and Schurter's THT equivalent
              (OG (Holder) 5x20, order 0031.8001, 10 A / 600 V) is the same class.

  ONE THING THIS FILE CANNOT KNOW
    The Phoenix body is 9.2 mm deep, but the datasheet page read here does not
    say where the pin row sits within that depth.  The silkscreen below centres
    the body on the pin row.  Before the board outline is fixed, check the
    dimensional drawing and shift it if the pins are offset -- it decides how
    far the connector overhangs the board edge, and ten of these set the size
    of the board (docs/pcb-altium.md section 6.2).
------------------------------------------------------------------------------}


{ ---------- helpers -- all take a parameter, so none of them is runnable --- }

Function NewComp(Lib : IPCB_Library; AName : String; ADesc : String) : IPCB_LibComponent;
Begin
    Result := PCBServer.CreatePCBLibComp;
    Result.Name        := AName;
    Result.Description := ADesc;
    Lib.RegisterComponent(Result);
End;


Procedure AddPad(Comp    : IPCB_LibComponent;
                 PadName : String;
                 Xmm, Ymm, XSize, YSize, HoleDia : Real;
                 IsRect  : Boolean);
Var
    Pad : IPCB_Pad;
Begin
    Pad := PCBServer.PCBObjectFactory(ePadObject, eNoDimension, eCreate_Default);
    Pad.X        := MMsToCoord(Xmm);
    Pad.Y        := MMsToCoord(Ymm);
    Pad.Mode     := ePadMode_Simple;
    Pad.TopXSize := MMsToCoord(XSize);
    Pad.TopYSize := MMsToCoord(YSize);

    If IsRect Then
        Pad.TopShape := eRectangular
    Else
        Pad.TopShape := eRounded;

    If HoleDia > 0 Then
    Begin
        Pad.HoleSize := MMsToCoord(HoleDia);
        Pad.Plated   := True;
        Pad.Layer    := eMultiLayer;
    End
    Else
    Begin
        Pad.HoleSize := 0;
        Pad.Layer    := eTopLayer;
    End;

    Pad.Name := PadName;
    Comp.AddPCBObject(Pad);
End;


Procedure AddSilk(Comp : IPCB_LibComponent; X1, Y1, X2, Y2 : Real);
Var
    Track : IPCB_Track;
Begin
    Track := PCBServer.PCBObjectFactory(eTrackObject, eNoDimension, eCreate_Default);
    Track.X1    := MMsToCoord(X1);
    Track.Y1    := MMsToCoord(Y1);
    Track.X2    := MMsToCoord(X2);
    Track.Y2    := MMsToCoord(Y2);
    Track.Width := MMsToCoord(0.15);      { docs/pcb-altium.md 6.3 }
    Track.Layer := eTopOverlay;
    Comp.AddPCBObject(Track);
End;


Procedure AddSilkBox(Comp : IPCB_LibComponent; X1, Y1, X2, Y2 : Real);
Begin
    AddSilk(Comp, X1, Y1, X2, Y1);
    AddSilk(Comp, X2, Y1, X2, Y2);
    AddSilk(Comp, X2, Y2, X1, Y2);
    AddSilk(Comp, X1, Y2, X1, Y1);
End;


{ ---------- 1. 0805 chip, hand solder ------------------------------------- }
{ R1-R25, R38-R40, C1, C2, C8, C10, C21 -- 35 places, the most-used land here.
  Vishay 28950 IPC-7351 reflow: gap G 1.00, pad 0.90 along the part axis,
  1.45 across, overall span Z 2.80.  Extending each outer end by 0.25 gives a
  pad 1.15 long, centres at 0.50 + 0.575 = 1.075, span 3.30. }

Procedure Make_Chip0805(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
    cx, xs, ys : Real;
Begin
    xs := 1.15;                       { 0.90 nominal + 0.25 hand solder }
    ys := 1.45;                       { across, straight from the table }
    cx := 1.075;                      { 1.00/2 + 1.15/2 }
    Comp := NewComp(Lib, 'CHIP0805-M',
        '0805 chip R/C, hand-solder land (Vishay 28950 IPC-7351 + 0.25 mm)');
    AddPad(Comp, '1', -cx, 0, xs, ys, 0, True);
    AddPad(Comp, '2',  cx, 0, xs, ys, 0, True);
    { silk sits in the 1.00 mm gap, clear of both pads by 0.15 }
    AddSilk(Comp, -0.45,  0.95, 0.45,  0.95);
    AddSilk(Comp, -0.45, -0.95, 0.45, -0.95);
End;


{ ---------- 2. SOT-23, hand solder ---------------------------------------- }
{ Q1 (AO3401A) and D10 (BZX84C12) -- one land, two parts.
  AOS PO-00001 rev N: pads 0.80 x 0.80, bottom pads at +/-0.95, rows 2.40 apart.
  Extending outward by 0.25 gives pads 1.05 tall with row centres at 1.325.
  Pin 1 bottom left, pin 2 bottom right, pin 3 top centre.
  D10 IS A ZENER IN A THREE-LEAD PACKAGE: check the BZX84C12 datasheet for
  which pins are anode and cathode before wiring it.  The land is the same
  either way; the schematic symbol is not. }

Procedure Make_SOT23(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
    cy, xs, ys : Real;
Begin
    xs := 0.80;
    ys := 1.05;                       { 0.80 nominal + 0.25 }
    cy := 1.325;                      { 2.40/2 + 0.25/2 }
    Comp := NewComp(Lib, 'SOT23-3-M',
        'SOT-23, hand-solder land (AOS PO-00001 rev N + 0.25 mm)');
    AddPad(Comp, '1', -0.95, -cy, xs, ys, 0, True);
    AddPad(Comp, '2',  0.95, -cy, xs, ys, 0, True);
    AddPad(Comp, '3',  0.00,  cy, xs, ys, 0, True);
    { body sides only -- top and bottom are where the pads are }
    AddSilk(Comp, -1.55, -0.80, -1.55, 0.80);
    AddSilk(Comp,  1.55, -0.80,  1.55, 0.80);
    { pin 1 tick }
    AddSilk(Comp, -1.75, -1.75, -1.35, -1.75);
End;


{ ---------- 3. SMB / DO-214AA, hand solder -------------------------------- }
{ D9, SMBJ33A.  Bourns SMBJ series "Recommended Footprint":
  A (gap) 2.69 max, B (pad height) 2.10 min, C (pad width) 1.27 min.
  Extending outward by 0.25 gives a pad 1.52 wide, centres at
  1.345 + 0.76 = 2.105, span 5.73.
  PAD 1 IS THE CATHODE -- the banded end.  D9 is unidirectional and it only
  works one way round (see hardware-interface.md section 5). }

Procedure Make_SMB(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
    cx, xs, ys : Real;
Begin
    xs := 1.52;                       { 1.27 min + 0.25 }
    ys := 2.10;
    cx := 2.105;                      { 2.69/2 + 1.52/2 }
    Comp := NewComp(Lib, 'SMB-DO214AA-M',
        'SMB/DO-214AA hand-solder land (Bourns SMBJ recommended + 0.25 mm). Pad 1 = cathode');
    AddPad(Comp, '1', -cx, 0, xs, ys, 0, True);
    AddPad(Comp, '2',  cx, 0, xs, ys, 0, True);
    { body edges, just outside the 3.94 max body width }
    AddSilk(Comp, -2.30,  2.10, 2.30,  2.10);
    AddSilk(Comp, -2.30, -2.10, 2.30, -2.10);
    { cathode bar, outside pad 1 }
    AddSilk(Comp, -3.10, -2.10, -3.10, 2.10);
End;


{ ---------- 4 & 5. Phoenix MC 1,5/n-G-3,5 headers ------------------------- }
{ J1-J6 are the 3-pole part (mates with MC 1,5/3-ST-3,5, order 1840379);
  J9-J12 are the 4-pole part (mates with MC 1,5/4-ST-3,5, order 1840382).
  Phoenix 1844223: hole 1.2 mm, pitch 3.5 mm, body 2.45 mm beyond the outer pin
  centres, body depth 9.2 mm.
  PAD DIAMETER 2.2 mm IS CHOSEN, NOT FROM THE DATASHEET.  It gives a 0.5 mm
  annular ring on a 1.2 mm hole, far above any fab minimum, and matches the
  deliberately-coarse choice this board makes everywhere else.
  Pin 1 is square so the orientation is readable on a bare board. }

Procedure Make_PhoenixHeader(Lib : IPCB_Library; Poles : Integer; CompName : String);
Var
    Comp   : IPCB_LibComponent;
    i      : Integer;
    x, x0, halfW, halfD : Real;
Begin
    x0    := -((Poles - 1) * 3.5) / 2;          { first pin; row centred on origin }
    halfW := (((Poles - 1) * 3.5) + 4.9) / 2;   { 11.9/2 for 3 poles, 15.4/2 for 4 }
    halfD := 4.6;                               { 9.2/2; ASSUMES body centred on pins }

    Comp := NewComp(Lib, CompName,
        'Phoenix MC 1,5/n-G-3,5 header, 3.5 mm pitch, 1.2 mm holes (datasheet 1844223); 2.2 mm pads chosen');

    For i := 0 To Poles - 1 Do
    Begin
        x := x0 + (i * 3.5);
        AddPad(Comp, IntToStr(i + 1), x, 0, 2.2, 2.2, 1.2, (i = 0));
    End;

    AddSilkBox(Comp, -halfW, -halfD, halfW, halfD);
    { pin 1 tick, outside the body }
    AddSilk(Comp, x0 - 0.5, -halfD - 0.5, x0 + 0.5, -halfD - 0.5);
End;


{ ---------- 7. Radial electrolytic, 8 mm can ------------------------------ }
{ C11 and C19 -- ONE part for both: Nichicon UVR1J101MPD, 100 uF 63 V.
  Nichicon UVR series catalogue CAT.8100M, dimension tables:
    - "1J" = 63 V, "101" = 100 uF, case code "PD"
    - the case-code table pairs PD with 8 x 11.5 mm
    - the lead table gives, for D = 8 mm:  P (lead spacing) = 3.5 mm,
      d (lead diameter) = 0.6 mm
  HOLE 0.9 mm AND PAD 1.8 mm ARE CHOSEN: 0.9 clears a 0.6 mm lead with the
  usual 0.3 mm allowance, and 1.8 leaves a 0.45 mm annular ring.

  WHY BOTH C11 AND C19 ARE THIS PART, when section 4a asks for 22 uF / 50 V at
  C19.  Two reasons, and they point the same way:
    1. C19 sits on 24V_PROT, the same node as C11, and section 4a's own rule for
       that node is "63 V, not 50 V -- D9 clamps as high as 53.3 V".  Traco's
       "22 uF / 50 V" is their generic wording, not an analysis of this node.
    2. A 22 uF 50 V X7R 1210 biased at 24-29 V loses well over half its
       capacitance, and Traco's 22 uF is a minimum, not a suggestion.
  100 uF / 63 V satisfies both, and collapses two part numbers into one.
  It roughly doubles the bulk capacitance behind Q2, so the hot-plug inrush
  energy goes from about 0.1 to 0.2 A^2s against F1's specified 0.5 A^2s -- see
  hardware-interface.md section 4a.  Still inside the margin, but that is the
  number to re-check if F1 ever gets smaller. }

Procedure Make_RadialCan8(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
    r, i : Real;
Begin
    Comp := NewComp(Lib, 'RADIAL-D8-P35',
        'Radial electrolytic, 8 mm can, 3.5 mm lead pitch (Nichicon UVR1J101MPD). Pad 1 = +');
    { pin 1 = positive, square, so polarity is readable on a bare board }
    AddPad(Comp, '1', -1.75, 0, 1.8, 1.8, 0.9, True);
    AddPad(Comp, '2',  1.75, 0, 1.8, 1.8, 0.9, False);

    { can outline, 8 mm diameter, drawn as an octagon out of straight silk }
    r := 4.0;
    i := r * 0.4142;                  { tan(22.5 deg) x r, for the octagon }
    AddSilk(Comp, -i,  r,  i,  r);
    AddSilk(Comp,  i,  r,  r,  i);
    AddSilk(Comp,  r,  i,  r, -i);
    AddSilk(Comp,  r, -i,  i, -r);
    AddSilk(Comp,  i, -r, -i, -r);
    AddSilk(Comp, -i, -r, -r, -i);
    AddSilk(Comp, -r, -i, -r,  i);
    AddSilk(Comp, -r,  i, -i,  r);
    { minus bar on the pad 2 side, the convention on the can itself }
    AddSilk(Comp, 2.6, -1.0, 2.6, 1.0);
End;


{ ---------- 8. J7 -- 2x20 shrouded box header, 2.54 mm -------------------- }
{ Revision 2026-09-04: J7 was a 2x19 (38-way) boxed header, which is not a size
  any IDC manufacturer makes.  It is now 2x20 (40-way), which is standard, and
  the cable is a 40-way ribbon with a 40-way IDC socket at BOTH ends.

  The Nucleo end then overhangs CN10 by one column, and that would normally be
  a serious hazard -- a socket sitting one column out puts every signal on the
  wrong pin.  It is not a hazard here, because of the key that section 1 of
  hardware-interface.md already specifies: CN10-6 is clipped off and hole 6 of
  the socket is plugged.  A plugged hole cannot go over a pin, and CN10-6 is the
  only missing pin, so exactly one alignment is possible.  The key was put there
  to stop the cable going on backwards; it now also stops it going on shifted.

  J7 PINS 39 AND 40 CONNECT TO NOTHING.  Their conductors hang past the end of
  CN10.  Give them no-connect flags on the schematic, the same as the other
  unused positions in section 2's table.

  Pitch 2.54 mm in both axes is the definition of the part.  HOLE 1.0 mm AND
  PAD 1.8 mm ARE CHOSEN, matching the other through-hole parts on this board.

  THE SHROUD OUTLINE BELOW IS PROVISIONAL.  Boxed headers put a plastic wall
  around the pin field and its size is vendor-specific; this draws the pin field
  plus 1 mm, which is smaller than any real shroud.  Before the board outline is
  fixed, take the real shroud from the header you buy -- it is about 8.6 mm
  across and a few mm longer than the pin field, and it is the thing that
  collides with the enclosure. }

Procedure Make_BoxHeader2x20(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
    i, col : Integer;
    x, x0, halfW : Real;
Begin
    x0    := -(19 * 2.54) / 2;        { 20 columns, centred: -24.13 }
    halfW := (19 * 2.54) / 2 + 1.0;   { pin field + 1 mm, PROVISIONAL }

    Comp := NewComp(Lib, 'HDR2X20-BOX',
        'Box header 2x20, 2.54 mm, shrouded. Pins 39/40 unconnected. Shroud outline provisional');

    For i := 1 To 40 Do
    Begin
        col := (i + 1) Div 2;                  { 1,2 -> col 1;  3,4 -> col 2 }
        x   := x0 + ((col - 1) * 2.54);
        If (i Mod 2) = 1 Then
            AddPad(Comp, IntToStr(i), x,  1.27, 1.8, 1.8, 1.0, (i = 1))
        Else
            AddPad(Comp, IntToStr(i), x, -1.27, 1.8, 1.8, 1.0, False);
    End;

    AddSilkBox(Comp, -halfW, -3.5, halfW, 3.5);
    { pin 1 tick, outside the outline }
    AddSilk(Comp, x0 - 0.5, 4.2, x0 + 0.5, 4.2);
End;


{ ---------- 9. F1 -- 5x20 mm cartridge fuse holder with cover ------------- }
{ 2 A time-lag, I^2t >= 0.5 A^2s (section 4a).  A one-piece PCB holder WITH A
  COVER, not two open clips: the fuse in a clip pair is held by spring pressure
  alone, and section 1 already says what vibration and thermal cycling do to a
  contact held that way -- this board lives on a pole in a misted greenhouse.

  PITCH 22.6 mm IS THE PART.  It is the de-facto standard spacing for covered
  5x20 PCB holders -- the PTF-78 family states it outright, and Schurter's THT
  equivalent (OG (Holder) 5x20, order 0031.8001, 10 A / 600 V, with cover) sits
  in the same group.

  HOLE 1.5 mm AND PAD 3.0 mm ARE CHOSEN, AND DELIBERATELY OVERSIZED.  No vendor
  in this class publishes the terminal thickness, and the generic holders vary.
  Rather than guess a number and be wrong by 0.2 mm, the land takes any of them:
  an oversized hole costs nothing on a hand-soldered board -- a little more
  solder -- and a hole that is 0.2 mm too small costs a board.

  THE BODY OUTLINE IS PROVISIONAL, like J7's shroud: 30 x 10 mm is typical for
  this class but is not from a drawing.  Measure the holder you buy before the
  board outline is fixed.  F1 sits in the 24 V corner with U7 and J14, and
  section 5 fixes the physical order F1 -> Q2 -> D9 -> C11 along that path. }

Procedure Make_FuseHolder5x20(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
Begin
    Comp := NewComp(Lib, 'FUSEHOLDER-5X20-P226',
        'Fuse holder, 5x20 mm cartridge, covered, 22.6 mm pitch. Holes oversized on purpose; body outline provisional');
    AddPad(Comp, '1', -11.3, 0, 3.0, 3.0, 1.5, True);
    AddPad(Comp, '2',  11.3, 0, 3.0, 3.0, 1.5, False);
    AddSilkBox(Comp, -15.0, -5.0, 15.0, 5.0);
End;


{ ---------- 10. Plain 1xN pin header, 2.54 mm ---------------------------- }
{ CN6 (1x8) and J13 (1x3).

  CN6 IS A CONNECTOR ON THIS BOARD, not just a drawing of the Nucleo's.  Since
  J8 was deleted (2026-09-04), the 3.3 V output leaves the front-end through an
  8-way header that mirrors the Nucleo's CN6 pin for pin, and the power lead is
  8-way to 8-way.  Only positions 4, 6 and 7 are wired (section 2, The CN6 power
  tap).

  THAT LEAD MUST BE KEYED, AND THE WAY TO DO IT IS ALREADY IN THIS DESIGN.
  CN6-1 is NC on the Nucleo.  Clip it and plug hole 1 of the socket, exactly as
  section 1 does with CN10-6.  Rotated 180 degrees the plug lands on position 8
  (VIN), which is populated, so the socket will not seat.  Without that, a
  reversed lead puts the board's 3.3 V onto the Nucleo's 5V pin and shorts NRST
  and IOREF to ground -- an MCU held in reset, permanently, by a cable.

  2.54 mm pitch is the definition of the part.  HOLE 1.0 mm AND PAD 1.8 mm ARE
  CHOSEN, the same as J7 so the board has one through-hole style for headers. }

Procedure Make_PinHeader1xN(Lib : IPCB_Library; Pins : Integer; CompName : String; Descr : String);
Var
    Comp : IPCB_LibComponent;
    i    : Integer;
    x, x0, halfW : Real;
Begin
    x0    := -((Pins - 1) * 2.54) / 2;
    halfW := ((Pins - 1) * 2.54) / 2 + 1.27;

    Comp := NewComp(Lib, CompName, Descr);

    For i := 1 To Pins Do
    Begin
        x := x0 + ((i - 1) * 2.54);
        AddPad(Comp, IntToStr(i), x, 0, 1.8, 1.8, 1.0, (i = 1));
    End;

    AddSilkBox(Comp, -halfW, -1.27, halfW, 1.27);
    AddSilk(Comp, x0 - 0.5, -2.0, x0 + 0.5, -2.0);      { pin 1 tick }
End;


{ ---------- 6. Test pad --------------------------------------------------- }
{ TP1-TP26 (hardware-interface.md section 4a).  A pad and nothing else, so a
  probe has somewhere to land.  1.5 mm IS CHOSEN: big enough for a probe tip
  with a hand on the other end, small enough that twenty-six of them cost no
  real area.  Place these as components so they appear in the BOM and the DRC
  sees them -- a free pad does neither. }

Procedure Make_TestPad(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
Begin
    Comp := NewComp(Lib, 'TESTPAD-1MM5',
        'Test point, 1.5 mm round SMD pad (size chosen)');
    AddPad(Comp, '1', 0, 0, 1.5, 1.5, 0, False);
End;


{ ---------- runnable: diagnostic ------------------------------------------ }
{ Run this first.  It creates nothing and changes nothing. }

Procedure CheckEnvironment;
Var
    WS   : IWorkspace;
    Doc  : IServerDocument;
    Lib  : IPCB_Library;
    Msg  : String;
Begin
    Msg := 'Script engine: running.' + Chr(13);

    { What does Altium think is in front?  This is the question that matters:
      "active" means FOCUSED, not merely open. }
    WS := GetWorkspace;
    If WS = Nil Then
        Msg := Msg + 'Workspace: NOT AVAILABLE' + Chr(13)
    Else
    Begin
        Doc := WS.DM_FocusedDocument;
        If Doc = Nil Then
            Msg := Msg + 'Focused document: none' + Chr(13)
        Else
            Msg := Msg + 'Focused document: ' + Doc.DM_FileName + Chr(13) +
                         'Its kind: [' + Doc.DM_DocumentKind + ']' + Chr(13);
    End;

    If PCBServer = Nil Then
        Msg := Msg + 'PCBServer: NOT LOADED' + Chr(13)
    Else
    Begin
        Msg := Msg + 'PCBServer: loaded' + Chr(13);
        Lib := PCBServer.GetCurrentPCBLibrary;
        If Lib = Nil Then
            Msg := Msg + 'GetCurrentPCBLibrary: nil  <-- this is the problem'
        Else
            Msg := Msg + 'Current PCB library: ' + Lib.Board.FileName +
                         Chr(13) + 'Ready. Run MakeFootprints.';
    End;

    ShowMessage(Msg);
End;


{ ---------- runnable: the real one ---------------------------------------- }

Procedure MakeFootprints;
Var
    Lib : IPCB_Library;
Begin
    Lib := PCBServer.GetCurrentPCBLibrary;

    If Lib = Nil Then
    Begin
        ShowMessage('No PCB library is active, so nothing was created.' +
                    Chr(13) + Chr(13) +
                    'Open (or create) WaterTempNode_FE.PcbLib and click its ' +
                    'tab so it is the document in front, then run again.');
        Exit;
    End;

    PCBServer.PreProcess;

    Make_Chip0805(Lib);
    Make_SOT23(Lib);
    Make_SMB(Lib);
    Make_PhoenixHeader(Lib, 2, 'PHX-MC15-2-G-35');
    Make_PhoenixHeader(Lib, 3, 'PHX-MC15-3-G-35');
    Make_PhoenixHeader(Lib, 4, 'PHX-MC15-4-G-35');
    Make_RadialCan8(Lib);
    Make_BoxHeader2x20(Lib);
    Make_FuseHolder5x20(Lib);
    Make_PinHeader1xN(Lib, 8, 'HDR1X8-P254',
        'Pin header 1x8, 2.54 mm. CN6 power tap - clip CN6-1 and plug hole 1 (section 1)');
    Make_PinHeader1xN(Lib, 3, 'HDR1X3-P254',
        'Pin header 1x3, 2.54 mm. J13 debug UART');
    Make_TestPad(Lib);

    PCBServer.PostProcess;
    Lib.Board.ViewManager_FullUpdate;

    ShowMessage('Created 12 footprints in ' + Lib.Board.FileName + Chr(13) +
                'Look in the PCB Library panel, not the canvas.' + Chr(13) +
                'Re-running is safe to repeat only into an empty library --' + Chr(13) +
                'delete the old ones first, or you get duplicates.' + Chr(13) + Chr(13) +
                'CHIP0805-M       R1-R25, R38-R40, C1, C2, C8, C10, C21' + Chr(13) +
                'SOT23-3-M        Q1, D10' + Chr(13) +
                'SMB-DO214AA-M    D9' + Chr(13) +
                'PHX-MC15-2-G-35  J14' + Chr(13) +
                'PHX-MC15-3-G-35  J1-J6' + Chr(13) +
                'PHX-MC15-4-G-35  J9-J12' + Chr(13) +
                'RADIAL-D8-P35    C11 and C19' + Chr(13) +
                'HDR2X20-BOX      J7  (shroud outline provisional)' + Chr(13) +
                'FUSEHOLDER-5X20-P226  F1  (body outline provisional)' + Chr(13) +
                'HDR1X8-P254      CN6' + Chr(13) +
                'HDR1X3-P254      J13' + Chr(13) +
                'TESTPAD-1MM5     TP1-TP26' + Chr(13) + Chr(13) +
                'Nothing is left to draw by hand.' + Chr(13) +
                'Check every one against the datasheet before use.');
End;
