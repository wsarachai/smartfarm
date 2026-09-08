{------------------------------------------------------------------------------
  MakeFootprintsPT.pas  --  water-temp-node  STM32WL_PT  (self-etched board)

  The through-hole footprint set for the hand-etched board described in
  docs/hardware-interface-proto.md and docs/pcb-home-etch.md.  Sibling of
  MakeFootprints.pas, which builds the SMD set for STM32WL_FE.  Do not run both
  into the same library: the pad sizes are deliberately different.

  THIS FILE HAS EXACTLY TWO THINGS YOU CAN RUN
      CheckEnvironmentPT   run this FIRST.  Touches nothing, tells you which
                           PCB library is in front.
      MakeFootprintsPT     the real one.
  Everything else takes a parameter, so Altium will not offer it in the Run
  Script list and you cannot start it by accident.

  HOW TO RUN
    1. File > Open...  STM32WL_Scripts.PrjScr  (next to this file).
    2. File > New > Library > PCB Library, save as STM32WL_PT.PcbLib in the
       same folder as STM32WL_PT.PrjPcb.
    3. Click the STM32WL_PT.PcbLib tab so it is the ACTIVE document.  The
       script writes into whichever PCB library is in front.
    4. DXP > Run Script...   CheckEnvironmentPT first, then MakeFootprintsPT.
    5. Look in the PCB Library panel, not the canvas.

  WHY THESE PADS ARE BIGGER THAN THE FE BOARD'S
    Every number here follows the design rules in docs/pcb-home-etch.md Stage 1,
    which exist because the board is etched and drilled by hand:
        pad, general (0.8-1.1 mm hole) ....... 2.2-2.4 mm
        pad, on 2.54 mm pitch ................ 1.9 mm   (0.64 mm gap - the
                                               tightest thing on the board)
        annular ring, minimum ................ 0.55 mm  (the drill wanders)
    An FE-board pad on a hand-drilled hole breaks out through the edge.

  WHERE THE NUMBERS COME FROM, AND THE ONE THING THAT IS DIFFERENT HERE
    MakeFootprints.pas could cite a land pattern for every SMD part, because a
    reflow land is a manufacturer's recommendation.  Most of the parts on THIS
    board are LEADED, and a leaded part has no land pattern: you bend the leads
    to whatever spacing the footprint uses.  So for axial and radial parts the
    pitch below is CHOSEN, and that is not a weakness -- the part conforms to
    the footprint, not the other way round.  What must still be right is the
    HOLE, because that is set by the lead, and every hole here is one of the
    four drill sizes the process document commits to: 0.8 / 1.0 / 1.1 / 1.3 mm.

    Cited:      2.54 mm header pitch, 3.5 mm Phoenix MC 1,5 pitch,
                2.54 mm TO-220 and TO-92 lead pitch, SOT-23 land (carried over
                from MakeFootprints.pas with its Alpha & Omega citation).
    CHOSEN:     every axial and radial pitch, every pad diameter, and every
                hole diameter, from the drill set above.

  WHAT THIS SCRIPT DELIBERATELY DOES NOT BUILD
    Two footprint details still need a part in your hand.  Guessing them would
    put a wrong number into a file that looks authoritative, which is the
    failure mode docs/pcb-altium.md section 1 is about.

    Both modules came off this list on 2026-09-08, when they were measured:
    MOD-TCA9548A (section 13) and MOD-DFR0570 (section 14).  Both turned out to
    sit on a 0.1 in grid, so both drop onto ordinary female header strip and
    can be lifted out again -- and neither needed a drill size the process
    document had not already committed to.

      MOD-DFR0570    DFRobot publish 16.5 x 22 mm and "2.54 mm pin plug-in"
                     (dfrobot.com/product-1767.html) but NOT the pin count,
                     order or row positions.  Measure the part.  Note the max
                     peak output is 2.4 A on DFRobot's own page, not the 3 A
                     some distributors list -- irrelevant at our 214 mA, but
                     the datasheet you cite should be theirs.
      TO220 tab hole The pin row below IS standard.  The mounting hole is not
                     dimensioned here because a clip-on heatsink changes it,
                     and Q3 needs one (hardware-interface-proto.md section 4.2).
                     Add a 3.5 mm non-plated hole after you have the heatsink.
      DO-15 body     The P6KE33A pin row is built; its silkscreen body outline
                     is omitted rather than guessed.

  ON POLARITY MARKS
    This board has no silkscreen (docs/pcb-home-etch.md).  The overlay drawn by
    this script exists ONLY so the footprints are readable inside Altium and on
    the 1:1 assembly drawing that becomes build-sheet-proto.md.  None of it is
    manufactured.  That is exactly why the build sheet is a gate before power.
------------------------------------------------------------------------------}


{ ---------- helpers -- all take a parameter, so none of them is runnable --- }

Function NewCompPT(Lib : IPCB_Library; AName : String; ADesc : String) : IPCB_LibComponent;
Begin
    Result := PCBServer.CreatePCBLibComp;
    Result.Name        := AName;
    Result.Description := ADesc;
    Lib.RegisterComponent(Result);
End;


Procedure AddPadPT(Comp    : IPCB_LibComponent;
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
        Pad.Plated   := True;      { no plating in reality -- see note below }
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

{ Plated := True is a lie about this board, told on purpose.  There is no
  plating; every hole is soldered on one face and any via is a wire.  But
  Altium uses the flag for connectivity, and a False here makes the top-layer
  pour refuse to connect to pads it must connect to.  The rule that keeps the
  drawing honest is a LAYOUT rule, not a footprint flag: pcb-home-etch.md
  Stage 1 rule 2 -- no top-layer track may terminate on a pad the component
  body covers. }


Procedure AddSilkPT(Comp : IPCB_LibComponent; X1, Y1, X2, Y2 : Real);
Var
    Track : IPCB_Track;
Begin
    Track := PCBServer.PCBObjectFactory(eTrackObject, eNoDimension, eCreate_Default);
    Track.X1    := MMsToCoord(X1);
    Track.Y1    := MMsToCoord(Y1);
    Track.X2    := MMsToCoord(X2);
    Track.Y2    := MMsToCoord(Y2);
    Track.Width := MMsToCoord(0.15);
    Track.Layer := eTopOverlay;
    Comp.AddPCBObject(Track);
End;


Procedure AddSilkBoxPT(Comp : IPCB_LibComponent; X1, Y1, X2, Y2 : Real);
Begin
    AddSilkPT(Comp, X1, Y1, X2, Y1);
    AddSilkPT(Comp, X2, Y1, X2, Y2);
    AddSilkPT(Comp, X2, Y2, X1, Y2);
    AddSilkPT(Comp, X1, Y2, X1, Y1);
End;


{ Pin 1 square, the rest round.  On a board with no silkscreen this is the only
  orientation cue that survives into copper, and it survives because a square
  pad is still square after etching. }
Procedure AddRowPT(Comp : IPCB_LibComponent; Count : Integer;
                   X0, Y0, Pitch, PadDia, HoleDia : Real; StartNum : Integer);
Var
    i : Integer;
Begin
    For i := 0 To Count - 1 Do
        AddPadPT(Comp, IntToStr(StartNum + i), X0 + i * Pitch, Y0,
                 PadDia, PadDia, HoleDia, (StartNum + i) = 1);
End;


{ ---------- 1. axial resistor, 10.16 mm pitch ----------------------------- }
{ R1-R25, R38-R42 -- 30 places, the most-used footprint on this board.
  Pitch CHOSEN 10.16 mm (0.4"): a 1/4 W body is ~6.5 mm, so the leads leave the
  body straight and there is room to grip one with tweezers while soldering.
  Hole 0.8 mm CHOSEN -- a 1/4 W lead is ~0.55 mm, and 0.8 is already in the
  drill set for vias, so this adds no bit change across 60 holes.
  Pad 2.2 mm CHOSEN -> 0.7 mm annular ring. }

Procedure Make_AxialR(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
Begin
    Comp := NewCompPT(Lib, 'AXIAL-R-P1016',
        'Axial resistor 1/4 W, 10.16 mm pitch, hand-etched pads (0.8 mm hole)');
    AddPadPT(Comp, '1', -5.08, 0, 2.2, 2.2, 0.8, True);
    AddPadPT(Comp, '2',  5.08, 0, 2.2, 2.2, 0.8, False);
    AddSilkBoxPT(Comp, -3.25, -1.25, 3.25, 1.25);
    AddSilkPT(Comp, -5.08, 0, -3.25, 0);
    AddSilkPT(Comp,  3.25, 0,  5.08, 0);
End;


{ ---------- 2. DO-41 axial diode, 7.62 mm pitch --------------------------- }
{ D10 1N4742A, D11 1N4750A, D12 1N4744A -- the three 1 W Zeners.
  Pitch CHOSEN 7.62 mm (0.3"): 1N47xx body is ~5 mm.
  Hole 0.8 mm CHOSEN -- 1N47xx lead is ~0.55 mm.
  The cathode band is at pad 2 and the silk bar marks it.  D10's polarity is
  the one hardware-interface.md section 5 calls out as already having been got
  wrong once, on the FE board, in SOT-23. }

Procedure Make_DO41(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
Begin
    Comp := NewCompPT(Lib, 'DO41-P762',
        'DO-41 axial diode / 1 W Zener, 7.62 mm pitch. Pad 2 = CATHODE (band)');
    AddPadPT(Comp, '1', -3.81, 0, 2.2, 2.2, 0.8, True);
    AddPadPT(Comp, '2',  3.81, 0, 2.2, 2.2, 0.8, False);
    AddSilkBoxPT(Comp, -2.30, -1.20, 2.30, 1.20);
    AddSilkPT(Comp, 1.55, -1.20, 1.55, 1.20);      { cathode band }
    AddSilkPT(Comp, -3.81, 0, -2.30, 0);
    AddSilkPT(Comp,  2.30, 0,  3.81, 0);
End;


{ ---------- 3. DO-15 axial diode, 12.7 mm pitch --------------------------- }
{ D9 P6KE33A, the 600 W TVS.  A DO-15 lead is thicker than a DO-41's, so this
  is the 1.1 mm drill -- the same bit the Phoenix headers use.
  Pitch CHOSEN 12.7 mm (0.5").  Pad 2.4 mm CHOSEN -> 0.65 mm ring.
  Body outline deliberately omitted: see the header of this file.
  Pad 2 = cathode = the banded end = 24V_PROT.  Fitted the other way it is a
  forward diode across the 24 V rail (hardware-interface.md section 5). }

Procedure Make_DO15(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
Begin
    Comp := NewCompPT(Lib, 'DO15-P1270',
        'DO-15 axial TVS (P6KE33A), 12.7 mm pitch. Pad 2 = CATHODE = 24V_PROT');
    AddPadPT(Comp, '1', -6.35, 0, 2.4, 2.4, 1.1, True);
    AddPadPT(Comp, '2',  6.35, 0, 2.4, 2.4, 1.1, False);
    AddSilkPT(Comp, -6.35, 0, -3.60, 0);
    AddSilkPT(Comp,  3.60, 0,  6.35, 0);
    AddSilkPT(Comp,  2.60, -1.8, 2.60, 1.8);       { cathode band }
End;


{ ---------- 4. ceramic capacitor, 5.08 mm pitch --------------------------- }
{ C2, C8, C10, C22 -- the 100 nF decouplers.
  Pitch CHOSEN 5.08 mm (0.2") rather than the 2.54 that small ceramics are
  sold on, because at 2.54 the two 2.2 mm pads leave a 0.34 mm gap, under the
  0.5 mm etch rule.  The leads bend outwards; the part does not care. }

Procedure Make_CeramicC(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
Begin
    Comp := NewCompPT(Lib, 'CERAMIC-P508',
        'Ceramic capacitor, 5.08 mm pitch (leads bent out from 2.54)');
    AddPadPT(Comp, '1', -2.54, 0, 2.2, 2.2, 0.8, True);
    AddPadPT(Comp, '2',  2.54, 0, 2.2, 2.2, 0.8, False);
    AddSilkBoxPT(Comp, -2.60, -2.00, 2.60, 2.00);
End;


{ ---------- 5. radial electrolytic, 8 mm can, 3.5 mm pitch ---------------- }
{ C11 (100 uF 63 V, on 24V_PROT) and C19 (100 uF 35 V, on 24V_PRE).
  Pitch 3.5 mm and can diameter 8 mm are the part's, from the same UVR1J101MPD
  used on the FE board (hardware-interface.md section 4a).
  Hole 1.0 mm and pad 2.2 mm CHOSEN.
  PAD 1 IS POSITIVE.  Reversed, a 100 uF electrolytic on a 24 V rail vents --
  one of the three ways hardware-interface.md section 5 says this node gets
  destroyed, and there is no silkscreen here to prevent it. }

Procedure Make_RadialCan(Lib : IPCB_Library; DiaMM, PitchMM : Real; AName, ADesc : String);
Var
    Comp : IPCB_LibComponent;
    h, r : Real;
Begin
    h := PitchMM / 2.0;
    r := DiaMM / 2.0;
    Comp := NewCompPT(Lib, AName, ADesc);
    AddPadPT(Comp, '1', -h, 0, 2.2, 2.2, 1.0, True);    { + }
    AddPadPT(Comp, '2',  h, 0, 2.2, 2.2, 1.0, False);   { - }
    AddSilkBoxPT(Comp, -r, -r, r, r);
    AddSilkPT(Comp, -r - 0.6, 0.0, -r - 0.2, 0.0);      { '+' arm }
    AddSilkPT(Comp, -r - 0.4, -0.2, -r - 0.4, 0.2);
End;


{ ---------- 6. TO-220 vertical -------------------------------------------- }
{ Q2 IRF9540N (tab = 24V_PROT) and Q3 IRF740 (tab = 24V_PRE, plus heatsink).
  Lead pitch 2.54 mm is the package standard.  Hole 1.3 mm CHOSEN -- a TO-220
  lead is ~0.9 x 0.5 mm and 1.1 is tight for a hand-drilled hole.
  Pad 2.4 mm CHOSEN.  At 2.54 pitch that leaves only a 0.14 mm gap, so the
  pads are STAGGERED: pins 1 and 3 sit 2.0 mm below pin 2.  The leads bend.
  Mounting hole omitted -- see the header. }

Procedure Make_TO220(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
Begin
    Comp := NewCompPT(Lib, 'TO220-VERT-STAG',
        'TO-220 vertical, 2.54 mm leads staggered 2.0 mm for etch clearance. TAB = PIN 2 = DRAIN, LIVE');
    AddPadPT(Comp, '1', -2.54, -2.0, 2.4, 2.4, 1.3, True);
    AddPadPT(Comp, '2',  0.00,  0.0, 2.4, 2.4, 1.3, False);
    AddPadPT(Comp, '3',  2.54, -2.0, 2.4, 2.4, 1.3, False);
    AddSilkBoxPT(Comp, -5.10, 1.60, 5.10, 6.30);        { body, 10.2 x 4.7 }
End;


{ ---------- 7. TO-92 inline ----------------------------------------------- }
{ Q4 BC547, the current-limit sense transistor.  Leads spread from 1.27 to
  2.54 mm inline, which is what a TO-92 is normally fitted on.
  Hole 0.8 mm, pad 2.0 mm CHOSEN. }

Procedure Make_TO92(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
Begin
    Comp := NewCompPT(Lib, 'TO92-INLINE-P254',
        'TO-92 inline, 2.54 mm. BC547: 1=C 2=B 3=E (flat face toward pin 1 side)');
    AddRowPT(Comp, 3, -2.54, 0, 2.54, 2.0, 0.8, 1);
    AddSilkPT(Comp, -2.60, 1.30, 2.60, 1.30);           { flat face }
    AddSilkPT(Comp, -2.60, 1.30, -2.60, -1.00);
    AddSilkPT(Comp,  2.60, 1.30,  2.60, -1.00);
End;


{ ---------- 8. pin header 1xN, 2.54 mm ------------------------------------ }
{ CN6 (1x8, the power tap) and J13 (1x3, debug UART).
  Pitch 2.54 mm cited.  Hole 1.0 mm, pad 1.9 mm CHOSEN -- this is the 0.64 mm
  gap that pcb-home-etch.md Stage 1 calls the tightest thing on the board, and
  the reason nothing routes between adjacent pins. }

Procedure Make_Header1xN(Lib : IPCB_Library; N : Integer; AName, ADesc : String);
Var
    Comp : IPCB_LibComponent;
    x0   : Real;
Begin
    x0 := -1.27 * (N - 1);
    Comp := NewCompPT(Lib, AName, ADesc);
    AddRowPT(Comp, N, x0, 0, 2.54, 1.9, 1.0, 1);
    AddSilkBoxPT(Comp, x0 - 1.27, -1.27, x0 + 2.54 * (N - 1) + 1.27, 1.27);
End;


{ ---------- 9. boxed header 2x20, 2.54 mm --------------------------------- }
{ J7, the ribbon to morpho CN10.  Forty holes, the hardest drilling on the
  board.  Odd pins in the lower row, even in the upper, matching the CN10 map
  in hardware-interface.md section 2 -- get this wrong and every signal moves.
  Shroud outline 52.5 x 15.0 mm is PROVISIONAL, carried over from
  MakeFootprints.pas where it is also marked provisional. }

Procedure Make_BoxHeader2x20(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
    i    : Integer;
    x    : Real;
Begin
    Comp := NewCompPT(Lib, 'HDR2X20-BOX-PT',
        'Boxed IDC header 2x20, 2.54 mm, hand-etch pads. J7 -> CN10. Shroud outline PROVISIONAL');
    For i := 0 To 19 Do
    Begin
        x := -24.13 + i * 2.54;
        AddPadPT(Comp, IntToStr(2 * i + 1), x, 0.00, 1.9, 1.9, 1.0, (i = 0));
        AddPadPT(Comp, IntToStr(2 * i + 2), x, 2.54, 1.9, 1.9, 1.0, False);
    End;
    AddSilkBoxPT(Comp, -26.25, -5.00, 26.25, 10.00);
End;


{ ---------- 10. Phoenix MC 1,5 pluggable header --------------------------- }
{ J1-J6 (probes, 3P), J9-J12 (I2C branches, 4P), J14 (24 V in, 2P).
  3.5 mm pitch is the MC 1,5/x-G-3,5 series pitch, cited.  Hole 1.1 mm and
  pad 2.4 mm CHOSEN -- the datasheet gives the drilled hole, not the pad, the
  same gap MakeFootprints.pas records for the FE board. }

Procedure Make_PhoenixHdr(Lib : IPCB_Library; N : Integer; AName : String);
Var
    Comp : IPCB_LibComponent;
    x0   : Real;
Begin
    x0 := -1.75 * (N - 1);
    Comp := NewCompPT(Lib, AName,
        'Phoenix MC 1,5/' + IntToStr(N) + '-G-3,5 pluggable header, hand-etch pads');
    AddRowPT(Comp, N, x0, 0, 3.5, 2.4, 1.1, 1);
    AddSilkBoxPT(Comp, x0 - 1.75, -3.30, x0 + 3.5 * (N - 1) + 1.75, 4.50);
End;


{ ---------- 11. fuse holder, 5x20 mm cartridge ---------------------------- }
{ F1, a T2A cartridge in a PTF-78 style clip pair.  Pitch 22.6 mm and the
  1.5 mm hole / 3.0 mm pad are all CHOSEN and deliberately oversized: no vendor
  in this class publishes a terminal thickness.  Carried over unchanged from
  MakeFootprints.pas, where the same note is made.
  1.5 mm is NOT in the four-bit drill set -- open the 1.3 mm holes with a
  1.5 mm bit, or ream them.  Two holes, so it is not worth a fifth size. }

Procedure Make_FuseHolder(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
Begin
    Comp := NewCompPT(Lib, 'FUSEHOLDER-5X20-P226',
        'Fuse clips for 5x20 mm cartridge, 22.6 mm pitch. Hole/pad CHOSEN oversize. Body outline PROVISIONAL');
    AddPadPT(Comp, '1', -11.3, 0, 3.0, 3.0, 1.5, True);
    AddPadPT(Comp, '2',  11.3, 0, 3.0, 3.0, 1.5, False);
    AddSilkBoxPT(Comp, -13.0, -3.5, 13.0, 3.5);
End;


{ ---------- 12. SOT-23, hand solder --------------------------------------- }
{ Q1 AO3401A, the VSENS gate -- the one part on this board that stays SMD
  because no through-hole P-FET fully enhances at V_GS = -3.3 V
  (hardware-interface-proto.md section 5).
  Land carried over verbatim from MakeFootprints.pas: Alpha & Omega document
  PO-00001 rev. N recommended land, outer end of each pad extended 0.25 mm.
  Its 0.35 mm gaps are the finest feature on the board and pcb-home-etch.md
  Stage 6 uses them as the etch-time canary. }

Procedure Make_SOT23(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
Begin
    Comp := NewCompPT(Lib, 'SOT23-3-M',
        'SOT-23 hand-solder land (AOS PO-00001 rev N + 0.25 mm). Q1 only');
    AddPadPT(Comp, '1', -0.95, -1.10, 0.70, 1.10, 0, True);
    AddPadPT(Comp, '2',  0.95, -1.10, 0.70, 1.10, 0, False);
    AddPadPT(Comp, '3',  0.00,  1.10, 0.70, 1.10, 0, False);
    AddSilkBoxPT(Comp, -0.80, -0.65, 0.80, 0.65);
End;


{ ---------- 13. TCA9548A breakout module, SKU-0260-1 ---------------------- }
{ U3.  Measured from the part, 2026-09-08: board 22.0 x 31.0 mm, two rows of
  twelve on 2.54 mm pitch, rows 17.78 mm apart centre to centre.

  THE PAD NAMES ARE THE CHIP'S PIN NUMBERS, NOT THE MODULE'S PIN ORDER.
  This is the whole reason this footprint needs its own procedure instead of a
  generic 2x12 header.  U3's schematic symbol is the TCA9548A chip symbol from
  ..\Lib\TCA9548APWR\TCA9548APWR.SchLib, whose pins are numbered 1-24 per
  TI SCPS207H Table 4-1.  Name these pads 1..24 down the columns instead and
  every net lands on the wrong pin, with no DRC error anywhere -- the netlist
  is consistent, it is just wrong.

  PIN 1 IS NOT THE ORIENTATION MARK HERE, AND THAT IS DELIBERATE.
  Chip pin 1 is A0, which sits sixth down the left column.  The square pad is
  VIN (chip pin 24) at the top left, because the square pad has to mean "this
  corner", and on a board with no silkscreen it is the only orientation cue
  that survives etching.

  Column X:  left -8.89, right +8.89        (17.78 / 2)
  Row Y:     +13.97 down to -13.97 in 2.54 steps   (11 x 2.54 = 27.94 span)

      pos   left   pad        right  pad
       1    VIN     24         SC7    20      <- VIN pad is square
       2    GND     12         SD7    19
       3    SDA     23         SC6    18
       4    SCL     22         SD6    17
       5    RST      3         SC5    16
       6    A0       1         SD5    15
       7    A1       2         SC4    14
       8    A2      21         SD4    13
       9    SD0      4         SC3    11      <- channels 0-3, the four we use,
      10    SC0      5         SD3    10         are all in the bottom third
      11    SD1      6         SC2     9
      12    SC1      7         SD2     8

  NOTE THE ORDER FLIPS BETWEEN THE COLUMNS: left runs SD then SC, right runs
  SC then SD.  Channels 0 and 1 leave on the left, 2 and 3 on the right. }

Procedure Make_ModTCA9548A(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
    xl, xr : Real;
Begin
    xl := -8.89;
    xr :=  8.89;
    Comp := NewCompPT(Lib, 'MOD-TCA9548A',
        'TCA9548A breakout SKU-0260-1, 22 x 31 mm, 2x12 @ 2.54, rows 17.78. Pads named by CHIP pin (TI SCPS207H)');

    { left column, top to bottom }
    AddPadPT(Comp, '24', xl,  13.97, 1.9, 1.9, 1.0, True);    { VIN  - square }
    AddPadPT(Comp, '12', xl,  11.43, 1.9, 1.9, 1.0, False);   { GND  }
    AddPadPT(Comp, '23', xl,   8.89, 1.9, 1.9, 1.0, False);   { SDA  }
    AddPadPT(Comp, '22', xl,   6.35, 1.9, 1.9, 1.0, False);   { SCL  }
    AddPadPT(Comp,  '3', xl,   3.81, 1.9, 1.9, 1.0, False);   { RST  }
    AddPadPT(Comp,  '1', xl,   1.27, 1.9, 1.9, 1.0, False);   { A0   }
    AddPadPT(Comp,  '2', xl,  -1.27, 1.9, 1.9, 1.0, False);   { A1   }
    AddPadPT(Comp, '21', xl,  -3.81, 1.9, 1.9, 1.0, False);   { A2   }
    AddPadPT(Comp,  '4', xl,  -6.35, 1.9, 1.9, 1.0, False);   { SD0  }
    AddPadPT(Comp,  '5', xl,  -8.89, 1.9, 1.9, 1.0, False);   { SC0  }
    AddPadPT(Comp,  '6', xl, -11.43, 1.9, 1.9, 1.0, False);   { SD1  }
    AddPadPT(Comp,  '7', xl, -13.97, 1.9, 1.9, 1.0, False);   { SC1  }

    { right column, top to bottom }
    AddPadPT(Comp, '20', xr,  13.97, 1.9, 1.9, 1.0, False);   { SC7  }
    AddPadPT(Comp, '19', xr,  11.43, 1.9, 1.9, 1.0, False);   { SD7  }
    AddPadPT(Comp, '18', xr,   8.89, 1.9, 1.9, 1.0, False);   { SC6  }
    AddPadPT(Comp, '17', xr,   6.35, 1.9, 1.9, 1.0, False);   { SD6  }
    AddPadPT(Comp, '16', xr,   3.81, 1.9, 1.9, 1.0, False);   { SC5  }
    AddPadPT(Comp, '15', xr,   1.27, 1.9, 1.9, 1.0, False);   { SD5  }
    AddPadPT(Comp, '14', xr,  -1.27, 1.9, 1.9, 1.0, False);   { SC4  }
    AddPadPT(Comp, '13', xr,  -3.81, 1.9, 1.9, 1.0, False);   { SD4  }
    AddPadPT(Comp, '11', xr,  -6.35, 1.9, 1.9, 1.0, False);   { SC3  }
    AddPadPT(Comp, '10', xr,  -8.89, 1.9, 1.9, 1.0, False);   { SD3  }
    AddPadPT(Comp,  '9', xr, -11.43, 1.9, 1.9, 1.0, False);   { SC2  }
    AddPadPT(Comp,  '8', xr, -13.97, 1.9, 1.9, 1.0, False);   { SD2  }

    AddSilkBoxPT(Comp, -11.0, -15.5, 11.0, 15.5);             { 22 x 31 board }
    AddSilkPT(Comp, -11.0, 12.5, -8.0, 15.5);                 { cut corner = VIN end }
End;


{ ---------- 14. DFRobot DFR0570 buck module ------------------------------- }
{ U7.  Measured 2026-09-08 and then snapped to the 0.1 in grid the module is
  plainly laid out on -- every measurement came within 0.4 mm of a grid value,
  and the clincher is that the outer and inner spans differ by exactly twice
  the stated pair pitch:

      measured   grid            measured   grid
      17.5  ->   17.78 (0.7")    8.00  ->   7.62 (0.3")
      12.5  ->   12.70 (0.5")    2.54  ->   2.54 (0.1")
                 12.70 - 7.62 = 5.08 = 2 x 2.54

  It also agrees with DFRobot's own "2.54 mm pin plug-in", so the module drops
  onto a female header strip like U3 does and can be lifted out again.

  ORIENTATION.  The 17.78 mm column separation runs along the module's 22.5 mm
  dimension, NOT its 17 mm one -- centres would fall off the board otherwise.
  So in this footprint X spans 22.5 mm and Y spans 17.0 mm.

  EIGHT HOLES, FOUR NETS.  Vin+ and Vo+ are doubled and GND is quadrupled,
  which is the module carrying 3 A; at our 46 mA one of each would do, and all
  of them still get soldered.

      pad  net    X       Y          pad  net    X      Y
       1   Vin+  -8.89   +6.35        5   Vo+   +8.89  +6.35
       2   Vin+  -8.89   +3.81        6   Vo+   +8.89  +3.81
       3   GND   -8.89   -3.81        7   GND   +8.89  -3.81
       4   GND   -8.89   -6.35        8   GND   +8.89  -6.35

  ON THE SCHEMATIC, wire 1+2 to 24V_PRE, 5+6 to the 3.3 V rail, and 3+4+7+8 to
  GND.  The symbol in STM32WL_PT.SchLib needs eight pins numbered to match --
  do not collapse them to three, or the footprint and symbol stop agreeing and
  Altium will not tell you which one it believes.

  Pad 1 is Vin+ and is square.  Getting this footprint mirrored puts the bank
  on the 3.3 V rail with the Nucleo downstream, so buzz it out before the
  module is fitted (hardware-interface-proto.md section 7). }

Procedure Make_ModDFR0570(Lib : IPCB_Library);
Var
    Comp : IPCB_LibComponent;
    xc, yo, yi : Real;
Begin
    xc := 8.89;      { 17.78 / 2 }
    yo := 6.35;      { 12.70 / 2, outer }
    yi := 3.81;      {  7.62 / 2, inner }

    Comp := NewCompPT(Lib, 'MOD-DFR0570',
        'DFRobot DFR0570 buck module, 22.5 x 17 mm, 8 holes on 0.1in grid. 1,2=Vin+  5,6=Vo+  3,4,7,8=GND');

    AddPadPT(Comp, '1', -xc,  yo, 1.9, 1.9, 1.0, True);    { Vin+ square }
    AddPadPT(Comp, '2', -xc,  yi, 1.9, 1.9, 1.0, False);   { Vin+ }
    AddPadPT(Comp, '3', -xc, -yi, 1.9, 1.9, 1.0, False);   { GND  }
    AddPadPT(Comp, '4', -xc, -yo, 1.9, 1.9, 1.0, False);   { GND  }
    AddPadPT(Comp, '5',  xc,  yo, 1.9, 1.9, 1.0, False);   { Vo+  }
    AddPadPT(Comp, '6',  xc,  yi, 1.9, 1.9, 1.0, False);   { Vo+  }
    AddPadPT(Comp, '7',  xc, -yi, 1.9, 1.9, 1.0, False);   { GND  }
    AddPadPT(Comp, '8',  xc, -yo, 1.9, 1.9, 1.0, False);   { GND  }

    AddSilkBoxPT(Comp, -11.25, -8.5, 11.25, 8.5);          { 22.5 x 17.0 }
    AddSilkPT(Comp, -11.25, 6.5, -9.25, 8.5);              { cut corner = Vin+ end }
End;


{ ---------- runnable: the safe one ---------------------------------------- }

Procedure CheckEnvironmentPT;
Var
    Lib : IPCB_Library;
    Msg : String;
Begin
    Msg := 'MakeFootprintsPT environment check' + Chr(13) + Chr(13);

    If PCBServer = Nil Then
        Msg := Msg + 'PCBServer: NOT LOADED'
    Else
    Begin
        Msg := Msg + 'PCBServer: loaded' + Chr(13);
        Lib := PCBServer.GetCurrentPCBLibrary;
        If Lib = Nil Then
            Msg := Msg + 'GetCurrentPCBLibrary: nil  <-- this is the problem.' +
                         Chr(13) + 'Click the STM32WL_PT.PcbLib tab, then run again.'
        Else
        Begin
            Msg := Msg + 'Current PCB library: ' + Lib.Board.FileName + Chr(13);
            Msg := Msg + Chr(13) +
                   'CHECK THE NAME ABOVE. If it says STM32WL_FE.PcbLib, stop:' + Chr(13) +
                   'these pads are sized for hand etching and would corrupt' + Chr(13) +
                   'the fab board''s library. Ready otherwise.';
        End;
    End;

    ShowMessage(Msg);
End;


{ ---------- runnable: the real one ---------------------------------------- }

Procedure MakeFootprintsPT;
Var
    Lib : IPCB_Library;
Begin
    Lib := PCBServer.GetCurrentPCBLibrary;

    If Lib = Nil Then
    Begin
        ShowMessage('No PCB library is active, so nothing was created.' +
                    Chr(13) + Chr(13) +
                    'Create STM32WL_PT.PcbLib and click its tab so it is the ' +
                    'document in front, then run again.');
        Exit;
    End;

    PCBServer.PreProcess;

    Make_AxialR(Lib);
    Make_DO41(Lib);
    Make_DO15(Lib);
    Make_CeramicC(Lib);
    Make_RadialCan(Lib, 8.0, 3.5, 'RADIAL-D8-P35',
        'Radial electrolytic, 8 mm can, 3.5 mm pitch. C11, C19. PAD 1 = POSITIVE');
    Make_RadialCan(Lib, 5.0, 2.0, 'RADIAL-D5-P20',
        'Radial electrolytic, 5 mm can, 2.0 mm pitch. C21. PAD 1 = POSITIVE');
    Make_TO220(Lib);
    Make_TO92(Lib);
    Make_Header1xN(Lib, 8, 'HDR1X8-P254-PT',
        'Pin header 1x8, 2.54 mm. CN6 power tap - clip CN6-1 and plug hole 1');
    Make_Header1xN(Lib, 3, 'HDR1X3-P254-PT',
        'Pin header 1x3, 2.54 mm. J13 debug UART');
    Make_BoxHeader2x20(Lib);
    Make_PhoenixHdr(Lib, 2, 'PHX-MC15-2-G-35-PT');
    Make_PhoenixHdr(Lib, 3, 'PHX-MC15-3-G-35-PT');
    Make_PhoenixHdr(Lib, 4, 'PHX-MC15-4-G-35-PT');
    Make_FuseHolder(Lib);
    Make_SOT23(Lib);
    Make_ModTCA9548A(Lib);
    Make_ModDFR0570(Lib);

    PCBServer.PostProcess;
    Lib.Board.ViewManager_FullUpdate;

    ShowMessage('Created 18 footprints in ' + Lib.Board.FileName + Chr(13) +
                'Look in the PCB Library panel, not the canvas.' + Chr(13) +
                'Re-run only into an empty library, or you get duplicates.' +
                Chr(13) + Chr(13) +
                'AXIAL-R-P1016     R1-R25, R38-R42  (30 parts)' + Chr(13) +
                'DO41-P762         D10, D11, D12' + Chr(13) +
                'DO15-P1270        D9  (P6KE33A)' + Chr(13) +
                'CERAMIC-P508      C2, C8, C10, C22' + Chr(13) +
                'RADIAL-D8-P35     C11, C19' + Chr(13) +
                'RADIAL-D5-P20     C21' + Chr(13) +
                'TO220-VERT-STAG   Q2, Q3   TAB IS LIVE' + Chr(13) +
                'TO92-INLINE-P254  Q4' + Chr(13) +
                'HDR1X8-P254-PT    CN6' + Chr(13) +
                'HDR1X3-P254-PT    J13' + Chr(13) +
                'HDR2X20-BOX-PT    J7' + Chr(13) +
                'PHX-MC15-2/3/4    J14 / J1-J6 / J9-J12' + Chr(13) +
                'FUSEHOLDER-5X20   F1' + Chr(13) +
                'SOT23-3-M         Q1' + Chr(13) +
                'MOD-TCA9548A      U3  pads named by CHIP pin, not row order' + Chr(13) +
                'MOD-DFR0570       U7  1,2=Vin+  5,6=Vo+  3,4,7,8=GND' + Chr(13) + Chr(13) +
                'STILL TO DRAW BY HAND:' + Chr(13) +
                '  TO-220 tab hole, once Q3''s heatsink is chosen' + Chr(13) +
                '  DO-15 body outline, if you want one' + Chr(13) + Chr(13) +
                'Print the library at 1:1 and lay the real parts on the paper.' +
                Chr(13) + 'That is the check that catches errors.');
End;
