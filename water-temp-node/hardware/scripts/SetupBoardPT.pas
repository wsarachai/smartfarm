{------------------------------------------------------------------------------
  SetupBoardPT.pas  --  water-temp-node  STM32WL_PT

  Places the things on STM32WL_PT.PcbDoc whose VALUE IS THEIR PRECISION: the
  board outline, the drawing origin, the three registration fiducials, the
  scale bar and the two layer-identification markers.  All of it is specified
  in docs/pcb-home-etch.md Stage 1.

  Run this on the PCB DOCUMENT, not the library.  MakeFootprintsPT.pas is the
  one that wants a library in front; this one wants STM32WL_PT.PcbDoc.

  THIS FILE HAS EXACTLY TWO THINGS YOU CAN RUN
      CheckBoardPT    safe.  Says which document is in front and what is
                      already on it, so you do not run the real one twice.
      DrawArtworkPT   the real one.

  WHY THE DESIGN RULES ARE NOT IN HERE
    They are ten values you can see and check in one dialog, and a wrong rule
    announces itself in the DRC.  A wrong scripted rule can half-apply and look
    applied.  The rules are a typing job in docs/pcb-home-etch.md Stage 1;
    this script does the part that is tedious and unverifiable by eye -- three
    fiducials at 0.01 mm and mirrored copper text -- and leaves the part that
    is quick and self-checking to you.

  THE COORDINATE SCHEME, WHICH IS THE ONE THING TO UNDERSTAND
    Altium's absolute space starts at (0,0) and dislikes negative coordinates,
    but the fiducials in pcb-home-etch.md sit in the MARGIN, outside the board,
    at negative coordinates relative to the board corner.  So the board is
    placed at an offset and the drawing origin is moved to its bottom-left
    corner:

        absolute (20,20)  =  relative (0,0)  =  board bottom-left

    After this runs, the coordinate readout matches the numbers in the process
    document.  Everything below is written in RELATIVE mm and converted once,
    in ToAbs, so there is one place to change if the offset ever moves.

  WHAT GETS DRAWN, AND ON WHICH LAYER
    Board outline .......... Mechanical 1, then Design > Board Shape >
                             Define from selected objects  (you do this; the
                             script draws the rectangle, see the note at the
                             end of DrawArtworkPT)
    Fiducials .............. multilayer pads, 1.0 mm hole -- they must be
                             drilled through the blank BEFORE transfer, which
                             is why they are pads and not just circles
    Scale bar .............. both copper layers
    TOP / BOT markers ...... their own copper layer each, BOT mirrored so it
                             reads correctly from the bottom face

  THE MARKERS ARE THE MIRROR CHECK
    pcb-home-etch.md Stage 2 says: after transfer, BOT must read correctly when
    you look at the bottom face and TOP when you look at the top.  That check
    only works if the artwork is right here, so this is the one thing in this
    file worth looking at twice.  BOT is placed on the bottom layer with the
    mirror flag SET, which is how Altium expresses "reads correctly from below".
------------------------------------------------------------------------------}


Const
    OFFSET_X = 20.0;      { absolute mm of the board's bottom-left corner }
    OFFSET_Y = 20.0;
    BOARD_W  = 160.0;
    BOARD_H  = 120.0;


{ ---------- helpers -- all take a parameter, so none is runnable ---------- }

Function ToAbsX(Rel : Real) : TCoord;
Begin
    Result := MMsToCoord(Rel + OFFSET_X);
End;

Function ToAbsY(Rel : Real) : TCoord;
Begin
    Result := MMsToCoord(Rel + OFFSET_Y);
End;


Procedure PutTrack(Board : IPCB_Board; Layer : TLayer;
                   X1, Y1, X2, Y2, WidthMM : Real);
Var
    T : IPCB_Track;
Begin
    T := PCBServer.PCBObjectFactory(eTrackObject, eNoDimension, eCreate_Default);
    T.X1    := ToAbsX(X1);
    T.Y1    := ToAbsY(Y1);
    T.X2    := ToAbsX(X2);
    T.Y2    := ToAbsY(Y2);
    T.Width := MMsToCoord(WidthMM);
    T.Layer := Layer;
    Board.AddPCBObject(T);
    PCBServer.SendMessageToRobots(Board.I_ObjectAddress, c_Broadcast,
                                  PCBM_BoardRegisteration, T.I_ObjectAddress);
End;


{ A fiducial is a real drilled hole, so it is a pad.  1.0 mm hole matches the
  pins used to register the two transfer sheets (pcb-home-etch.md Stage 3). }
Procedure PutFiducial(Board : IPCB_Board; X, Y : Real; AName : String);
Var
    P : IPCB_Pad;
Begin
    P := PCBServer.PCBObjectFactory(ePadObject, eNoDimension, eCreate_Default);
    P.X        := ToAbsX(X);
    P.Y        := ToAbsY(Y);
    P.Mode     := ePadMode_Simple;
    P.TopShape := eRounded;
    P.TopXSize := MMsToCoord(2.4);
    P.TopYSize := MMsToCoord(2.4);
    P.HoleSize := MMsToCoord(1.0);
    P.Layer    := eMultiLayer;
    P.Plated   := False;
    P.Name     := AName;
    Board.AddPCBObject(P);
    PCBServer.SendMessageToRobots(Board.I_ObjectAddress, c_Broadcast,
                                  PCBM_BoardRegisteration, P.I_ObjectAddress);
End;


Procedure PutText(Board : IPCB_Board; Layer : TLayer;
                  X, Y, HeightMM : Real; S : String; Mirrored : Boolean);
Var
    T : IPCB_Text;
Begin
    T := PCBServer.PCBObjectFactory(eTextObject, eNoDimension, eCreate_Default);
    T.XLocation  := ToAbsX(X);
    T.YLocation  := ToAbsY(Y);
    T.Layer      := Layer;
    T.Text       := S;
    T.Size       := MMsToCoord(HeightMM);
    T.Width      := MMsToCoord(0.4);        { thick enough to survive etching }
    T.MirrorFlag := Mirrored;
    Board.AddPCBObject(T);
    PCBServer.SendMessageToRobots(Board.I_ObjectAddress, c_Broadcast,
                                  PCBM_BoardRegisteration, T.I_ObjectAddress);
End;


{ 100.0 mm CENTRE TO CENTRE of the two end ticks.  Measure it on a
  plain-paper proof with calipers before transferring anything: a printer set
  to shrink-to-fit produces a board where every 2.54 mm header refuses to seat
  by the fourth pin (pcb-home-etch.md Stage 2, GATE 2).

  MEASURE CENTRE TO CENTRE, NOT OUTSIDE TO OUTSIDE.  The ticks are drawn AT
  x and x+100, so their outer edges are half a line width beyond that on each
  side: outside-to-outside reads 100.5 and inside-to-inside 99.5.  GATE 2's
  tolerance is +-0.3 mm, so measuring the outside of a perfectly printed bar
  fails the gate and invites rescaling artwork that was already correct.
  Centre to centre is 100.0 whatever the line width is, which is the whole
  reason to measure it that way.

  THE WIDTH IS 0.5 mm BECAUSE THE DESIGN RULE SAYS SO.  These tracks sit on
  copper, so the Width rule (min 0.5 mm) polices them like any other track.
  At the 0.4 mm they were first drawn at, the bar and its three ticks raise
  eight MinWidthStubTrack violations on both layers -- and GATE 1 asks for a
  clean DRC, which a board carrying eight known-harmless violations can never
  honestly pass. }
Procedure PutScaleBar(Board : IPCB_Board; Layer : TLayer; X, Y : Real);
Begin
    PutTrack(Board, Layer, X,         Y, X + 100.0, Y, 0.5);
    PutTrack(Board, Layer, X,         Y - 1.5, X,         Y + 1.5, 0.5);
    PutTrack(Board, Layer, X + 50.0,  Y - 1.0, X + 50.0,  Y + 1.0, 0.5);
    PutTrack(Board, Layer, X + 100.0, Y - 1.5, X + 100.0, Y + 1.5, 0.5);
End;


{ ---------- runnable: the safe one ---------------------------------------- }

Procedure CheckBoardPT;
Var
    Board : IPCB_Board;
    Msg   : String;
Begin
    Board := PCBServer.GetCurrentPCBBoard;

    If Board = Nil Then
    Begin
        ShowMessage('No PCB document is in front.' + Chr(13) + Chr(13) +
                    'This script runs on STM32WL_PT.PcbDoc, not on the ' +
                    'library. Create it with File > New > PCB, save it in ' +
                    'the STM32WL_PT folder, click its tab, and run again.');
        Exit;
    End;

    Msg := 'Document in front: ' + Board.FileName + Chr(13) + Chr(13) +
           'CHECK THAT NAME. If it says STM32WL_FE.PcbDoc, stop --' + Chr(13) +
           'that board is finished and this would draw on top of it.' +
           Chr(13) + Chr(13) +
           'Origin is at ' +
           FloatToStr(CoordToMMs(Board.XOrigin)) + ', ' +
           FloatToStr(CoordToMMs(Board.YOrigin)) + ' mm' + Chr(13) +
           'After DrawArtworkPT it should read 20, 20.' + Chr(13) + Chr(13) +
           'Run DrawArtworkPT ONCE. There is no duplicate check --' + Chr(13) +
           'running it twice stacks a second set of fiducials on the first.';

    ShowMessage(Msg);
End;


{ ---------- runnable: the real one ---------------------------------------- }

Procedure DrawArtworkPT;
Var
    Board : IPCB_Board;
Begin
    Board := PCBServer.GetCurrentPCBBoard;

    If Board = Nil Then
    Begin
        ShowMessage('No PCB document is in front. Run CheckBoardPT first.');
        Exit;
    End;

    PCBServer.PreProcess;

    { origin at the board's bottom-left corner, so the readout matches the
      numbers in docs/pcb-home-etch.md }
    Board.XOrigin := MMsToCoord(OFFSET_X);
    Board.YOrigin := MMsToCoord(OFFSET_Y);

    { board outline, 160 x 120, on Mechanical 1 }
    PutTrack(Board, eMechanical1, 0,       0,       BOARD_W, 0,       0.2);
    PutTrack(Board, eMechanical1, BOARD_W, 0,       BOARD_W, BOARD_H, 0.2);
    PutTrack(Board, eMechanical1, BOARD_W, BOARD_H, 0,       BOARD_H, 0.2);
    PutTrack(Board, eMechanical1, 0,       BOARD_H, 0,       0,       0.2);

    { three fiducials, an asymmetric L in the margin -- three corners of a
      rectangle and never four, so a sheet flipped or rotated cannot look
      right (pcb-home-etch.md Stage 1) }
    PutFiducial(Board, -7.0,   -7.0,  'FID1');
    PutFiducial(Board, 167.0,  -7.0,  'FID2');
    PutFiducial(Board, -7.0,  127.0,  'FID3');

    { scale bar in the bottom margin, on both copper layers }
    PutScaleBar(Board, eTopLayer,    30.0, -4.0);
    PutScaleBar(Board, eBottomLayer, 30.0, -4.0);
    PutText(Board, eTopLayer,    62.0, -9.0, 3.0, '100.0 mm', False);
    PutText(Board, eBottomLayer, 62.0, -9.0, 3.0, '100.0 mm', True);

    { layer markers in copper -- the mirror check of Stage 2/GATE 4.
      BOT is mirrored so it reads correctly when you look at the BOTTOM face. }
    PutText(Board, eTopLayer,    8.0, 110.0, 6.0, 'TOP', False);
    PutText(Board, eBottomLayer, 8.0, 110.0, 6.0, 'BOT', True);

    PCBServer.PostProcess;
    Board.ViewManager_FullUpdate;

    ShowMessage('Drawn on ' + Board.FileName + Chr(13) + Chr(13) +
                'Origin moved to 20, 20 absolute = board corner 0, 0.' + Chr(13) +
                'Outline 160 x 120 on Mechanical 1.' + Chr(13) +
                'FID1/2/3 at (-7,-7) (167,-7) (-7,127) relative.' + Chr(13) +
                'Scale bar and TOP/BOT markers on both copper layers.' +
                Chr(13) + Chr(13) +
                'TWO THINGS LEFT, BOTH BY HAND:' + Chr(13) + Chr(13) +
                '1. Select the four Mechanical 1 tracks, then' + Chr(13) +
                '   Design > Board Shape > Define from selected objects.' + Chr(13) +
                '   The script draws the rectangle; only you can promote it' + Chr(13) +
                '   to the board shape, and doing that from a script is the' + Chr(13) +
                '   kind of call that fails differently between builds.' +
                Chr(13) + Chr(13) +
                '2. Type the design rules -- ten values, one dialog,' + Chr(13) +
                '   docs/pcb-home-etch.md Stage 1. Press Q first so the' + Chr(13) +
                '   PCB is in mm, or you will be entering mil.' +
                Chr(13) + Chr(13) +
                'Then GATE 1: DRC clean, via count known, fiducials and' + Chr(13) +
                'markers on both layers.');
End;
