{------------------------------------------------------------------------------
  PlacePartsPT.pas  --  water-temp-node  STM32WL_PT

  Drops all 69 components onto their floorplan positions, in the arrangement
  section 6 of docs/hardware-interface-proto.md fixes.  Run it on
  STM32WL_PT.PcbDoc, after SetupBoardPT.pas has drawn the artwork and after
  Design > Import Changes has brought the components across.

  THIS FILE HAS THREE THINGS YOU CAN RUN
      CheckPlacementPT   safe.  Says which document is in front, how many of
                         the 69 it can find, and which are missing or already
                         locked.  Run it first, every time.
      PlaceFloorplanPT   the real one.  Moves and locks the 14 connectors and
                         the two modules; moves everything else unlocked.
      DrawKeepoutsPT     the real one, part two.  Three keep-out rectangles:
                         under U3, under U7, and under Q3's heatsink.

  WHAT IS LOCKED AND WHAT IS NOT, WHICH IS THE POINT OF THE SCRIPT
    LOCKED: J1-J7, J9-J14, CN6, U3, U7.  These are the positions section 6
    calls not negotiable -- the edge assignment, and J1..J6 running P0->P5 in
    that order along the bottom.  Swapping two probe connectors silently
    relabels which end of the greenhouse a reading came from: it is
    DS_PROBE_BUSES and the dashboard metric names, and nothing downstream
    would report an error.  Locking them is the cheap way to make that
    impossible to do by accident with a stray drag.

    NOT LOCKED: the other 53.  Their positions here are a STARTING floorplan,
    not a specification.  They are grouped with the thing they belong to and
    spaced to clear the 0.5 mm clearance rule, and they are meant to be
    dragged while routing.  Two groups do carry real rules and are commented
    where they are placed: the 24 V chain's physical order, and C19's distance
    to U7's input pins.

  WHAT THIS SCRIPT DOES NOT DECIDE
    ROTATION IS PLACEMENT ONLY, NOT POLARITY.  Every part here lands at 0 or
    90 degrees because that is what makes the ratsnest readable, not because
    the band ends up on the correct side.  D9, D10, D11, D12, C11, C19 and
    both electrolytics are polarised and docs/build-sheet-proto.md section 2
    is what says which way round they go on the finished board.  Expect to
    flip several of these while routing.

    IT ALSO DOES NOT ROUTE, and it must not be run as though it did -- see the
    Q1 warning below.

  BUG 2 IS CLOSED -- 2026-09-10, and this note is kept as history
    Q1 spent a day carrying the MOSFET-P symbol from Miscellaneous Devices
    (D=1 G=2 S=3) against the SOT23-3-M land, whose pads carry the AOS
    drawing's numbers (G=1 S=2 D=3), so its three nets sat one pad round from
    the part's own pins.  Restoring the AO3401A symbol fixed the numbering and
    dragged the vendor's own nominal land in with it, which then had to be set
    back to SOT23-3-M by hand.  Both halves are verified out of the saved
    board.  Routing is unblocked.  docs/pcb-altium-pt.md, Bug 2, has the whole
    account -- worth reading before touching any component's model.

  THE COORDINATE SCHEME
    Same as SetupBoardPT.pas and for the same reason: absolute (20,20) is the
    board's bottom-left corner, everything below is written in RELATIVE mm,
    and ToAbs does the conversion once.  Board is 160 x 120.

  EVERY ROUTINE IN THIS FILE IS FLAT, AND HAS TO STAY THAT WAY
    Altium's DelphiScript will not let a nested routine read the enclosing
    one's parameters or locals; it fails at run time with "Can't access top
    level variable", which reads like a complaint about globals and is not one.
    Pass what you need as a parameter.  See PutKeepoutSeg.

  THE FLOORPLAN, IN ONE PICTURE   (relative mm, y up)

    120 +--------------------------------------------------------------+
        | J13                                                          |
        |        [ U3  TCA9548A ]                        R39 R40 -[J12]|
        | J7     R15 R16 R23 C8                          R21 R22 -[J11]|
        |        [pre-reg]   [C19][ U7 ]  Q1 R1 R2       R19 R20 -[J10]|
        |        R41 D11 Q3  C22 D12 R42 Q4   C1 C2 C21  R17 R18 -[J09]|
        | CN6                     C11  D10 R38  R24 R25 C10           |
        |         probe front ends        D9   Q2      F1              |
      0 +--[J1]--[J2]--[J3]--[J4]--[J5]--[J6]-----------------[J14]----+
        0                                                            160
------------------------------------------------------------------------------}


Const
    OFFSET_X = 20.0;      { absolute mm of the board's bottom-left corner }
    OFFSET_Y = 20.0;

    LOCKED   = True;
    FREE     = False;


Var
    GMissing : String;    { designators PlaceOne could not find }
    GPlaced  : Integer;


{ ---------- helpers -- all take a parameter, so none is runnable ---------- }

Function ToAbsX(Rel : Real) : TCoord;
Begin
    Result := MMsToCoord(Rel + OFFSET_X);
End;

Function ToAbsY(Rel : Real) : TCoord;
Begin
    Result := MMsToCoord(Rel + OFFSET_Y);
End;


Function FindComp(Board : IPCB_Board; Des : String) : IPCB_Component;
Var
    Iter : IPCB_BoardIterator;
    Comp : IPCB_Component;
Begin
    Result := Nil;
    Iter := Board.BoardIterator_Create;
    Iter.AddFilter_ObjectSet(MkSet(eComponentObject));
    Iter.AddFilter_LayerSet(AllLayers);
    Iter.AddFilter_Method(eProcessAll);

    Comp := Iter.FirstPCBObject;
    While Comp <> Nil Do
    Begin
        If Comp.Name.Text = Des Then
        Begin
            Result := Comp;
            Break;
        End;
        Comp := Iter.NextPCBObject;
    End;

    Board.BoardIterator_Destroy(Iter);
End;


{ Move one component.  A designator that is not on the board is collected in
  GMissing rather than raised -- one missing part should not abandon the other
  sixty-eight halfway through. }
Procedure PlaceOne(Board : IPCB_Board; Des : String;
                   X, Y, Rot : Real; Lock : Boolean);
Var
    Comp : IPCB_Component;
Begin
    Comp := FindComp(Board, Des);

    If Comp = Nil Then
    Begin
        GMissing := GMissing + Des + ' ';
        Exit;
    End;

    PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast,
                                  PCBM_BeginModify, c_NoEventData);

    Comp.Moveable := True;              { in case a previous run locked it }
    Comp.Rotation := Rot;
    Comp.X        := ToAbsX(X);
    Comp.Y        := ToAbsY(Y);
    Comp.Moveable := Not Lock;

    PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast,
                                  PCBM_EndModify, c_NoEventData);

    GPlaced := GPlaced + 1;
End;


{ One edge of a keep-out rectangle.

  THIS IS FLAT ON PURPOSE, AND THAT IS THE WHOLE NOTE.
  It began as a nested procedure inside PutKeepoutBox, which is natural Pascal
  and which Altium's DelphiScript cannot run: a nested routine may not read the
  enclosing routine's parameters or locals, and reaching for `Board` from inside
  it fails at run time with

      Can't access top level variable

  -- an error whose wording points at globals and whose cause is the nesting.
  SetupBoardPT.pas never hit it because every routine in that file is flat.
  Keep them flat here too: pass what you need as a parameter. }
Procedure PutKeepoutSeg(Board : IPCB_Board; Ax, Ay, Bx, By : Real);
Var
    T : IPCB_Track;
Begin
    T := PCBServer.PCBObjectFactory(eTrackObject, eNoDimension,
                                    eCreate_Default);
    T.X1        := ToAbsX(Ax);
    T.Y1        := ToAbsY(Ay);
    T.X2        := ToAbsX(Bx);
    T.Y2        := ToAbsY(By);
    T.Width     := MMsToCoord(0.25);
    T.Layer     := eKeepOutLayer;
    T.IsKeepout := True;
    Board.AddPCBObject(T);
    PCBServer.SendMessageToRobots(Board.I_ObjectAddress, c_Broadcast,
                                  PCBM_BoardRegisteration,
                                  T.I_ObjectAddress);
End;


{ A keep-out rectangle, drawn as four tracks on the keep-out layer.  The DRC
  reads these as "no copper here", which is what section 6's "no traces under
  the module footprints, and none under Q3's heatsink" asks for. }
Procedure PutKeepoutBox(Board : IPCB_Board; X1, Y1, X2, Y2 : Real);
Begin
    PutKeepoutSeg(Board, X1, Y1, X2, Y1);
    PutKeepoutSeg(Board, X2, Y1, X2, Y2);
    PutKeepoutSeg(Board, X2, Y2, X1, Y2);
    PutKeepoutSeg(Board, X1, Y2, X1, Y1);
End;


{ ---------- runnable: the safe one ---------------------------------------- }

Procedure CheckPlacementPT;
Var
    Board  : IPCB_Board;
    Iter   : IPCB_BoardIterator;
    Comp   : IPCB_Component;
    N, L   : Integer;
    Msg    : String;
Begin
    Board := PCBServer.GetCurrentPCBBoard;

    If Board = Nil Then
    Begin
        ShowMessage('No PCB document is in front.' + Chr(13) + Chr(13) +
                    'This script runs on STM32WL_PT.PcbDoc. Click its tab ' +
                    'and run again.');
        Exit;
    End;

    N := 0;
    L := 0;
    Iter := Board.BoardIterator_Create;
    Iter.AddFilter_ObjectSet(MkSet(eComponentObject));
    Iter.AddFilter_LayerSet(AllLayers);
    Iter.AddFilter_Method(eProcessAll);
    Comp := Iter.FirstPCBObject;
    While Comp <> Nil Do
    Begin
        N := N + 1;
        If Not Comp.Moveable Then L := L + 1;
        Comp := Iter.NextPCBObject;
    End;
    Board.BoardIterator_Destroy(Iter);

    Msg := 'Document in front: ' + Board.FileName + Chr(13) + Chr(13) +
           'CHECK THAT NAME. If it says STM32WL_FE.PcbDoc, stop.' +
           Chr(13) + Chr(13) +
           'Components on the board: ' + IntToStr(N) + '   (expect 69)' +
           Chr(13) +
           'Already locked:          ' + IntToStr(L) +
           '   (expect 0 before, 16 after)' + Chr(13) + Chr(13) +
           'Origin reads ' +
           FloatToStr(CoordToMMs(Board.XOrigin)) + ', ' +
           FloatToStr(CoordToMMs(Board.YOrigin)) + ' mm' + Chr(13) +
           'It must read 20, 20 -- if it does not, DrawArtworkPT has not run' +
           Chr(13) + 'and every coordinate below would land 20 mm out.' +
           Chr(13) + Chr(13) +
           'PlaceFloorplanPT is safe to run more than once: it sets absolute' +
           Chr(13) + 'positions rather than nudging, so a second run puts' +
           Chr(13) + 'everything back where the first one did. DrawKeepoutsPT' +
           Chr(13) + 'is NOT -- it adds tracks, and twice gives you two sets.';

    ShowMessage(Msg);
End;


{ ---------- runnable: the real one ---------------------------------------- }

Procedure PlaceFloorplanPT;
Var
    Board : IPCB_Board;
Begin
    Board := PCBServer.GetCurrentPCBBoard;

    If Board = Nil Then
    Begin
        ShowMessage('No PCB document is in front. Run CheckPlacementPT first.');
        Exit;
    End;

    GMissing := '';
    GPlaced  := 0;

    PCBServer.PreProcess;

    { ===== the fourteen connectors -- locked ============================== }

    { Bottom edge, pad row at y = 6.0.  J1..J6 ARE P0..P5 AND THE ORDER IS
      LOAD-BEARING: it is DS_PROBE_BUSES and the dashboard metric names.
      14 mm centres on a 10.5 mm body leaves 3.5 mm between shrouds. }
    PlaceOne(Board, 'J1',  20.0,   6.0,   0.0, LOCKED);
    PlaceOne(Board, 'J2',  34.0,   6.0,   0.0, LOCKED);
    PlaceOne(Board, 'J3',  48.0,   6.0,   0.0, LOCKED);
    PlaceOne(Board, 'J4',  62.0,   6.0,   0.0, LOCKED);
    PlaceOne(Board, 'J5',  76.0,   6.0,   0.0, LOCKED);
    PlaceOne(Board, 'J6',  90.0,   6.0,   0.0, LOCKED);

    { 24 V in, same edge, far corner -- the door the whole input chain
      hangs off. }
    PlaceOne(Board, 'J14', 148.0,  6.0,   0.0, LOCKED);

    { Right edge, pad column at x = 152.0, rotated 90 so the plug faces out.
      Four identical I2C branches: CH0 CH1 CH2 CH3(CO2), bottom to top. }
    PlaceOne(Board, 'J9',  152.0,  44.0, 90.0, LOCKED);
    PlaceOne(Board, 'J10', 152.0,  60.0, 90.0, LOCKED);
    PlaceOne(Board, 'J11', 152.0,  76.0, 90.0, LOCKED);
    PlaceOne(Board, 'J12', 152.0,  92.0, 90.0, LOCKED);

    { Left edge.  J7 is the 2x20 ribbon to CN10 -- 52.5 mm of shroud, which is
      why it takes most of one edge on its own.  J13's DBG_TX/DBG_RX come off
      J7 pins 35/37, which sit at the TOP of J7 once it is rotated, so J13
      goes above it and not below. }
    PlaceOne(Board, 'J7',   12.0,  62.0, 90.0, LOCKED);
    PlaceOne(Board, 'CN6',   8.0,  24.0, 90.0, LOCKED);
    PlaceOne(Board, 'J13',   8.0,  96.0, 90.0, LOCKED);

    { ===== the two modules -- locked ====================================== }
    { No traces under either: DrawKeepoutsPT draws the rectangles. }
    PlaceOne(Board, 'U3',  112.0,  82.0,  0.0, LOCKED);
    PlaceOne(Board, 'U7',  108.0,  52.0,  0.0, LOCKED);

    { ===== probe front ends -- three parts per channel ==================== }
    { Per channel the DQ node is a star: pull-up, series resistor and TVS all
      meet on it, and it goes to the connector. The TVS sits nearest the
      connector because that is the end the 5-10 m of wet outdoor cable
      arrives on. Series resistor inboard-left, pull-up inboard-right, both
      vertical so a 10.16 mm axial part fits inside a 14 mm channel pitch. }

    PlaceOne(Board, 'D1',   20.0,  13.5,  0.0, FREE);   { P0 }
    PlaceOne(Board, 'R9',   17.0,  22.0, 90.0, FREE);
    PlaceOne(Board, 'R3',   23.0,  22.0, 90.0, FREE);

    PlaceOne(Board, 'D2',   34.0,  13.5,  0.0, FREE);   { P1 }
    PlaceOne(Board, 'R10',  31.0,  22.0, 90.0, FREE);
    PlaceOne(Board, 'R4',   37.0,  22.0, 90.0, FREE);

    PlaceOne(Board, 'D3',   48.0,  13.5,  0.0, FREE);   { P2 }
    PlaceOne(Board, 'R11',  45.0,  22.0, 90.0, FREE);
    PlaceOne(Board, 'R5',   51.0,  22.0, 90.0, FREE);

    PlaceOne(Board, 'D4',   62.0,  13.5,  0.0, FREE);   { P3 }
    PlaceOne(Board, 'R12',  59.0,  22.0, 90.0, FREE);
    PlaceOne(Board, 'R6',   65.0,  22.0, 90.0, FREE);

    PlaceOne(Board, 'D5',   76.0,  13.5,  0.0, FREE);   { P4 }
    PlaceOne(Board, 'R13',  73.0,  22.0, 90.0, FREE);
    PlaceOne(Board, 'R7',   79.0,  22.0, 90.0, FREE);

    PlaceOne(Board, 'D6',   90.0,  13.5,  0.0, FREE);   { P5 }
    PlaceOne(Board, 'R14',  87.0,  22.0, 90.0, FREE);
    PlaceOne(Board, 'R8',   93.0,  22.0, 90.0, FREE);

    { ===== 24 V input chain =============================================== }
    { THE ORDER ALONG THIS ROW IS THE SPECIFICATION, not a preference:
      J14 -> F1 -> Q2 -> D9 -> C11, right to left. D9 sits nearest the door so
      an 11.3 A surge is shunted before it tours the board, C11 immediately
      inside it. Give D9's anode and C11's minus their own wide copper back to
      J14's ground pin -- section 6, and it is the one node on this board where
      "ground is just ground" is wrong. }
    PlaceOne(Board, 'F1',  138.0,  18.0,  0.0, FREE);
    PlaceOne(Board, 'Q2',  118.0,  18.0,  0.0, FREE);   { tab = 24V_PROT, LIVE }
    PlaceOne(Board, 'D9',  104.0,  18.0,  0.0, FREE);
    PlaceOne(Board, 'C11', 104.0,  30.0,  0.0, FREE);

    { Q2's gate clamp. R38 and D10 are a pair and both belong AT the FET --
      without D10 V_GS reaches -32 V against a +-20 V limit.

      R38 SITS HIGHER THAN IT LOOKS IT SHOULD, and so does the gap to D9.
      TO220-VERT-STAG's body box runs from +1.60 to +6.30 ABOVE the pad row,
      so a TO-220 placed at y=18 reaches y=24.4 and is 10.4 mm wide, not the
      7.5 mm the pads suggest. Both were first placed against a hand-guessed
      extent and Altium's ComponentClearance caught them at 10 mil.
      check_floorplan.py now derives every extent from MakeFootprintsPT.pas
      instead; run it after moving anything here. }
    PlaceOne(Board, 'R38', 112.0,  33.0, 90.0, FREE);
    PlaceOne(Board, 'D10', 118.0,  30.0, 90.0, FREE);

    { VBAT_SENSE divider. Section 6 wants it LAST along the 24V_PROT node and
      away from both modules: it is a 27 kOhm analog source and has no business
      near switching copper. This is the group most worth a second look once
      the ground pour is in. }
    PlaceOne(Board, 'R24', 128.0,  34.0,  0.0, FREE);
    PlaceOne(Board, 'R25', 128.0,  28.0,  0.0, FREE);
    PlaceOne(Board, 'C10', 140.0,  28.0,  0.0, FREE);

    { ===== linear pre-regulator, 24V_PROT -> 24V_PRE ====================== }
    { Q3 is a source follower; D11 pins its gate so U7's input can never climb
      past ~23 V whatever the bank does. Q3 needs a clip-on heatsink -- 3.3 W
      into a bare TO-220 on a sustained short -- and its tab is at 24V_PRE, so
      the heatsink is live unless it is isolated. Keep-out drawn separately. }
    PlaceOne(Board, 'Q3',   70.0,  48.0,  0.0, FREE);
    PlaceOne(Board, 'R41',  52.0,  54.0, 90.0, FREE);
    PlaceOne(Board, 'D11',  52.0,  41.0, 90.0, FREE);
    PlaceOne(Board, 'C22',  58.0,  42.0, 90.0, FREE);
    PlaceOne(Board, 'D12',  58.0,  52.0, 90.0, FREE);
    PlaceOne(Board, 'R42',  84.0,  48.0, 90.0, FREE);
    PlaceOne(Board, 'Q4',   82.0,  64.0,  0.0, FREE);

    { C19 is U7's input bulk and wants to be within 5 mm of its input pins.
      This is as close as the geometry goes: an 8 mm can beside a module whose
      silk outline reaches 2.36 mm past its own pad column leaves 7.6 mm pad
      to pad. Closer means the can overhangs the module. Sanity-check it
      against the real module before committing. }
    PlaceOne(Board, 'C19',  91.5,  57.08, 90.0, FREE);

    { 3.3 V rail output bulk }
    PlaceOne(Board, 'C21', 124.0,  46.0,  0.0, FREE);

    { ===== rail gate -- Q1 switches VSENS ================================= }
    { SEE THE Q1 WARNING IN THE HEADER. Place, do not route. }
    PlaceOne(Board, 'Q1',  128.0,  60.0,  0.0, FREE);
    PlaceOne(Board, 'R1',  128.0,  70.0, 90.0, FREE);   { 100k gate->source }
    PlaceOne(Board, 'R2',  134.0,  70.0, 90.0, FREE);   { gate series }
    PlaceOne(Board, 'C1',  132.0,  52.0,  0.0, FREE);   { VSENS bulk }
    PlaceOne(Board, 'C2',  132.0,  46.0,  0.0, FREE);   { VSENS decoupling }

    { ===== U3 mux and its own parts ======================================= }
    { R23 is the RESET pull-up and it is not optional: a held RESET is a total
      I2C blackout, CO2 included, because everything on this node is behind
      the mux. It pulls up to VSENS, not to permanent 3V3. }
    PlaceOne(Board, 'R23',  95.0,  88.0, 90.0, FREE);
    PlaceOne(Board, 'C8',   89.0,  88.0, 90.0, FREE);
    PlaceOne(Board, 'R15',  95.0,  74.0, 90.0, FREE);   { upstream SDA 4k7 }
    PlaceOne(Board, 'R16',  89.0,  74.0, 90.0, FREE);   { upstream SCL 4k7 }

    { ===== downstream pull-up pairs, one per branch ======================= }
    { 2.2 kOhm, not 4.7 -- each pair drives 5 m of cable. One pair beside its
      own connector so the pairing survives a later edit. }
    PlaceOne(Board, 'R17', 141.0,  44.0, 90.0, FREE);   { J9  CH0 }
    PlaceOne(Board, 'R18', 145.0,  44.0, 90.0, FREE);
    PlaceOne(Board, 'R19', 141.0,  60.0, 90.0, FREE);   { J10 CH1 }
    PlaceOne(Board, 'R20', 145.0,  60.0, 90.0, FREE);
    PlaceOne(Board, 'R21', 141.0,  76.0, 90.0, FREE);   { J11 CH2 }
    PlaceOne(Board, 'R22', 145.0,  76.0, 90.0, FREE);
    PlaceOne(Board, 'R39', 141.0,  92.0, 90.0, FREE);   { J12 CH3, the SCD41 }
    PlaceOne(Board, 'R40', 145.0,  92.0, 90.0, FREE);

    PCBServer.PostProcess;
    Board.ViewManager_FullUpdate;

    If GMissing = '' Then
        GMissing := '(none -- all 69 found)';

    ShowMessage('Placed ' + IntToStr(GPlaced) + ' of 69 on ' +
                Board.FileName + Chr(13) + Chr(13) +
                'Not found: ' + GMissing + Chr(13) + Chr(13) +
                'LOCKED (16): J1-J7, J9-J14, CN6, U3, U7.' + Chr(13) +
                'Everything else is a starting position -- drag it.' +
                Chr(13) + Chr(13) +
                'NEXT, IN THIS ORDER:' + Chr(13) +
                '1. DrawKeepoutsPT, once, for U3 / U7 / Q3''s heatsink.' +
                Chr(13) +
                '2. Fix Bug 2 -- Q1''s symbol back to AO3401A -- and' + Chr(13) +
                '   re-import. DO NOT ROUTE BEFORE THIS.' + Chr(13) +
                '3. Eyeball the three groups the comments flag: the 24 V' +
                Chr(13) +
                '   chain order, C19 to U7, and the VBAT_SENSE divider.' +
                Chr(13) +
                '4. pcb-home-etch.md Stage 1 routing -- bottom layer is the' +
                Chr(13) +
                '   real board, nothing between 2.54 mm pins, vias counted.');
End;


{ ---------- runnable: the real one, part two ------------------------------ }

Procedure DrawKeepoutsPT;
Var
    Board : IPCB_Board;
Begin
    Board := PCBServer.GetCurrentPCBBoard;

    If Board = Nil Then
    Begin
        ShowMessage('No PCB document is in front. Run CheckPlacementPT first.');
        Exit;
    End;

    PCBServer.PreProcess;

    { U3, 22 x 31 at (112, 82) }
    PutKeepoutBox(Board, 101.0, 66.5, 123.0, 97.5);

    { U7, 22.5 x 17 at (108, 52) }
    PutKeepoutBox(Board,  96.75, 43.5, 119.25, 60.5);

    { Q3's heatsink at (70, 48). 16 x 16 is a clip-on for a TO-220 plus a
      little; measure the real one and stretch this if it is bigger. The tab
      is at 24V_PRE, so this rectangle is a live part as well as a hot one. }
    PutKeepoutBox(Board,  62.0, 40.0,  78.0, 56.0);

    PCBServer.PostProcess;
    Board.ViewManager_FullUpdate;

    ShowMessage('Three keep-out rectangles drawn on ' + Board.FileName +
                Chr(13) + Chr(13) +
                'U3   101.0, 66.5  ->  123.0, 97.5' + Chr(13) +
                'U7    96.8, 43.5  ->  119.3, 60.5' + Chr(13) +
                'Q3hs  62.0, 40.0  ->   78.0, 56.0' + Chr(13) + Chr(13) +
                'RUN THIS ONCE. There is no duplicate check -- a second run' +
                Chr(13) + 'stacks a second set on the first, and two' +
                Chr(13) + 'coincident keep-outs look exactly like one.' +
                Chr(13) + Chr(13) +
                'If you moved U3, U7 or Q3 first, delete these and edit the' +
                Chr(13) + 'coordinates: the rectangles do not follow the part.');
End;
