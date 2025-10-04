#include "chess.h"

// Promotion selection dialog simple modal
std::optional<PieceType> DoPromotionDialog(HWND hwndParent) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1, L"Queen");
    AppendMenuW(hMenu, MF_STRING, 2, L"Rook");
    AppendMenuW(hMenu, MF_STRING, 3, L"Bishop");
    AppendMenuW(hMenu, MF_STRING, 4, L"Knight");

    POINT pt;
    GetCursorPos(&pt);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_TOPALIGN | TPM_LEFTALIGN,
                             pt.x, pt.y, 0, hwndParent, NULL);
    DestroyMenu(hMenu);

    switch(cmd) {
        case 1: return PT_QUEEN;
        case 2: return PT_ROOK;
        case 3: return PT_BISHOP;
        case 4: return PT_KNIGHT;
        default: return std::nullopt;
    }
}

// Promotion helper for UI: modal selection (uses Windows simple message box-like modal window)
PieceType ChoosePromotion(HWND parent, Color color){
    auto r = DoPromotionDialog(parent);
    if(r) return *r;
    return PT_QUEEN; // default
}

// Rendering + UI
wchar_t Glyph(const Piece &p){
    if(p.type==PT_NONE) return L' ';
    if(p.color==C_WHITE){
        switch(p.type){ case PT_KING: return 0x2654; case PT_QUEEN: return 0x2655; case PT_ROOK: return 0x2656;
            case PT_BISHOP: return 0x2657; case PT_KNIGHT: return 0x2658; case PT_PAWN: return 0x2659; default: return L'?'; }
    } else {
        switch(p.type){ case PT_KING: return 0x265A; case PT_QUEEN: return 0x265B; case PT_ROOK: return 0x265C;
            case PT_BISHOP: return 0x265D; case PT_KNIGHT: return 0x265E; case PT_PAWN: return 0x265F; default: return L'?'; }
    }
}

void CreateFonts(){
    if(glyphFont) { DeleteObject(glyphFont); glyphFont=NULL; }
    if(uiFont) { DeleteObject(uiFont); uiFont=NULL; }
    glyphFont = CreateFontW(- (int)(squareSize*0.7), 0,0,0,FW_BOLD,FALSE,FALSE,FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI Symbol");
    uiFont = CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH,L"Segoe UI");
}

void DrawBoardAndUI(HDC hdcScreen){
    RECT rc; 
    GetClientRect(g_hwnd, &rc);

    // create memory DC for double buffering
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdcScreen, rc.right, rc.bottom);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hbmMem);

    // fill background
    FillRect(hdcMem, &rc, (HBRUSH)GetStockObject(GRAY_BRUSH));

    // compute board placement
    int boardPixelW = squareSize * 8;
    boardLeft = (rc.right - boardPixelW) / 2;
    boardTop = 120;

    // title + controls at top
    HFONT oldf = (HFONT)SelectObject(hdcMem, uiFont);
    SetBkMode(hdcMem, TRANSPARENT);
    std::wstring title = L"Chess — Full rules, AI & local play";
    TextOutW(hdcMem, 10, 10, title.c_str(), (int)title.size());

    // buttons: New, Undo, AI toggle, Flip, ShowLegal
    RECT btnNew = {10,40,110,80}, btnUndo={120,40,220,80}, btnAIToggle={230,40,330,80},
         btnFlip={340,40,440,80}, btnShow={450,40,600,80};

    HBRUSH bbg = CreateSolidBrush(RGB(200,200,200));
    FillRect(hdcMem, &btnNew, bbg); FillRect(hdcMem, &btnUndo, bbg);
    FillRect(hdcMem, &btnAIToggle, bbg); FillRect(hdcMem, &btnFlip, bbg); FillRect(hdcMem, &btnShow, bbg);
    DeleteObject(bbg);

    DrawTextW(hdcMem, L"New Game", -1, &btnNew, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    DrawTextW(hdcMem, L"Undo", -1, &btnUndo, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    DrawTextW(hdcMem, aiOnG ? L"AI: ON" : L"AI: OFF", -1, &btnAIToggle, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    DrawTextW(hdcMem, flipBoardG ? L"Flip: ON" : L"Flip: OFF", -1, &btnFlip, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    DrawTextW(hdcMem, showLegalG ? L"ShowMoves: ON" : L"ShowMoves: OFF", -1, &btnShow, DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    wchar_t depthBuf[64]; 
    wsprintfW(depthBuf, L"AI depth: %d (use +/-)", aiDepthG);
    TextOutW(hdcMem, 620, 50, depthBuf, lstrlenW(depthBuf));
    SelectObject(hdcMem, oldf);

    // draw board squares and pieces
    for(int by=0; by<8; ++by){
        for(int bx=0; bx<8; ++bx){
            int x = bx, y = by;
            if(flipBoardG){ x = 7-bx; y = 7-by; }

            RECT r = { boardLeft + bx*squareSize, boardTop + by*squareSize,
                       boardLeft + (bx+1)*squareSize, boardTop + (by+1)*squareSize };
            bool light = ((x + y) & 1) == 0;
            HBRUSH br = CreateSolidBrush(light ? RGB(240,217,181) : RGB(181,136,99));
            FillRect(hdcMem, &r, br);
            DeleteObject(br);

            Piece p = boardG[y][x];
            if(p.type != PT_NONE){
                HFONT old = (HFONT)SelectObject(hdcMem, glyphFont);
                SetBkMode(hdcMem, TRANSPARENT);
                SetTextColor(hdcMem, p.color==C_WHITE?RGB(255,255,255):RGB(10,10,10));
                wchar_t g[2] = { Glyph(p), 0 };
                DrawTextW(hdcMem, g, 1, &r, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                SelectObject(hdcMem, old);
            }
        }
    }

    // highlight legal moves if requested
    if(showLegalG){
        LPARAM prop = (LPARAM)GetPropW(g_hwnd, L"SELECT");
        if(prop){
            int sel = (int)prop;
            int selBX = (sel>>16)&0xFFFF, selBY = sel&0xFFFF;
            Piece tmp[8][8]; CopyBoard(boardG,tmp);
            auto all = GenerateLegalMoves(tmp, sideToMoveG, lastMoveG);
            for(auto &m : all){
                if(m.fx==selBX && m.fy==selBY){
                    int tx=m.tx, ty=m.ty;
                    if(flipBoardG){ tx = 7-tx; ty = 7-ty; }
                    RECT r={ boardLeft + tx*squareSize, boardTop + ty*squareSize,
                             boardLeft + (tx+1)*squareSize, boardTop + (ty+1)*squareSize };
                    HBRUSH br = CreateSolidBrush(RGB(0,255,0));
                    FrameRect(hdcMem, &r, br);
                    DeleteObject(br);
                }
            }
        }
    }

    // game status
    HFONT sf = (HFONT)SelectObject(hdcMem, uiFont);
    std::wstring status = gameOverG ? L"Game Over" : (sideToMoveG==C_WHITE?L"White to move":L"Black to move");
    RECT rStatus = {10, clientH-60, 300, clientH-20};
    DrawTextW(hdcMem, status.c_str(), -1, &rStatus, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    SelectObject(hdcMem, sf);

    // blit memory DC to screen (swap)
    BitBlt(hdcScreen, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);

    // cleanup
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

// map mouse -> board coordinates considering flip
bool ScreenToBoard(int sx,int sy,int &bx,int &by){
    if(sx < boardLeft || sy < boardTop) return false;
    int dx = sx - boardLeft, dy = sy - boardTop;
    if(dx >= squareSize*8 || dy >= squareSize*8) return false;
    int col = dx / squareSize, row = dy / squareSize;
    if(flipBoardG){ bx = 7 - col; by = 7 - row; } else { bx = col; by = row; }
    return true;
}

// basic UI actions
void newGame(){
	InitStartingBoard();
	undoStack.clear();
	InvalidateRect(g_hwnd, NULL, TRUE);
}

void flipBoard(){
	flipBoardG = !flipBoardG; 
	InvalidateRect(g_hwnd, NULL, TRUE);
}
void flipSide(){
	flipBoardG = !flipBoardG; 
	humanSide = humanSide==C_WHITE ? C_BLACK : C_WHITE;
	InvalidateRect(g_hwnd, NULL, TRUE);
}

void toggleAi(){
	aiOnG = !aiOnG;
	InvalidateRect(g_hwnd, NULL, TRUE);
}

void toggleShowLegal(){
	showLegalG=!showLegalG;
	InvalidateRect(g_hwnd, NULL, TRUE);
}

// main window proc
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
    static int selX=-1, selY=-1;
    switch(msg){
        case WM_CREATE:
            return 0;
        case WM_LBUTTONDOWN:{
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            // top buttons area (quick hit test)
            RECT rNew = {10,40,110,80}, rUndo={120,40,220,80}, rAI={230,40,330,80}, rFlip={340,40,440,80}, rShow={450,40,600,80};
            POINT pt = {mx,my};
            if(PtInRect(&rNew, pt)){newGame();return 0;}
			else if(PtInRect(&rUndo, pt)){DoUndo();return 0;}
            else if(PtInRect(&rAI, pt)){toggleAi();return 0;}
            else if(PtInRect(&rFlip, pt)){flipBoard();return 0;}
            else if(PtInRect(&rShow, pt)){toggleShowLegal();return 0;}

            // board click:
            int bx, by;
            if(!ScreenToBoard(mx,my,bx,by)) return 0;

            // if nothing selected, select piece if it belongs to user to move
            if(selX==-1){
                if(boardG[by][bx].type!=PT_NONE && boardG[by][bx].color==sideToMoveG){
                    selX = bx; selY = by;
                    // store selection in window prop so drawing can highlight legal moves
                    SetPropW(g_hwnd, L"SELECT", reinterpret_cast<HANDLE>(static_cast<intptr_t>((selX<<16) | (selY & 0xFFFF))));
                    InvalidateRect(hWnd,NULL,TRUE);
                }
            } else {
                // attempt move from selX,selY -> bx,by
                // generate legal moves for sideToMoveG
                Piece cur[8][8]; CopyBoard(boardG, cur);
                auto legal = GenerateLegalMoves(cur, sideToMoveG, lastMoveG);
                bool found=false;
                Move candidate;
                for(auto &m: legal){
                    if(m.fx==selX && m.fy==selY && m.tx==bx && m.ty==by){
                        found=true; candidate=m; break;
                    }
                }
                if(found){
                    // promotion dialog if needed
                    if(boardG[candidate.fy][candidate.fx].type==PT_PAWN && (candidate.ty==0 || candidate.ty==7)){
                        PieceType chosen = ChoosePromotion(hWnd, boardG[candidate.fy][candidate.fx].color);
                        candidate.promoteTo = chosen;
                    }
                    PushUndo();
                    ApplyMoveGlobal(candidate);
                    selX = selY = -1;
                    RemovePropW(g_hwnd, L"SELECT");
                    InvalidateRect(hWnd,NULL,TRUE);
                } else {
                    // select new square if piece belongs to side
                    if(boardG[by][bx].type!=PT_NONE && boardG[by][bx].color==sideToMoveG){
                        selX = bx; selY = by;
                        SetPropW(g_hwnd, L"SELECT", reinterpret_cast<HANDLE>(static_cast<intptr_t>((selX<<16) | (selY & 0xFFFF))));

                        InvalidateRect(hWnd,NULL,TRUE);
                    } else {
                        selX = selY = -1;
                        RemovePropW(g_hwnd, L"SELECT");
                        InvalidateRect(hWnd,NULL,TRUE);
                    }
                }
            }
            return 0;
        }
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case ID_NEW_GAME: newGame(); break;
				case ID_UNDO: DoUndo(); break;
				case ID_FLIP_BOARD: flipBoard(); break;
				case ID_FLIP_SIDE: flipSide(); break;
				case ID_TOGGLE_AI: toggleAi(); break;
				case ID_SHOW_LEGAL: toggleShowLegal(); break;
			}
			break;

        case WM_KEYDOWN:
            if(wParam==VK_ESCAPE) PostQuitMessage(0);
            else if(wParam==VK_OEM_PLUS || wParam==VK_ADD) { aiDepthG = std::min(6, aiDepthG+1); InvalidateRect(hWnd,NULL,TRUE); }
            else if(wParam==VK_OEM_MINUS || wParam==VK_SUBTRACT) { aiDepthG = std::max(1, aiDepthG-1); InvalidateRect(hWnd,NULL,TRUE); }
            else if(wParam=='U'){ DoUndo(); }
			else if(wParam=='C'){flipSide();}
			else if(wParam=='F'){flipBoard();}
			else if(wParam=='A'){toggleAi();}
			else if(wParam=='R'){newGame();}
            return 0;
        case WM_PAINT:{
			PAINTSTRUCT ps; 
			HDC hdc = BeginPaint(hWnd,&ps);
			DrawBoardAndUI(hdc);
			EndPaint(hWnd,&ps);
			return 0;
		}
		case WM_SIZE:
			InvalidateRect(hWnd,NULL,TRUE);
			return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd,msg,wParam,lParam);
}

void CreateMainMenu(HWND hwnd) {
    HMENU hMenu = CreateMenu();
    HMENU hGame = CreatePopupMenu();
    HMENU hOptions = CreatePopupMenu();

    // Game Menu Items
    AppendMenuW(hGame, MF_STRING, ID_NEW_GAME, L"New Game");
    AppendMenuW(hGame, MF_STRING, ID_UNDO, L"Undo");
    AppendMenuW(hGame, MF_STRING, ID_EXIT, L"Exit");

    // Options Menu Items
    AppendMenuW(hOptions, MF_STRING, ID_FLIP_BOARD, L"Flip Board");
	AppendMenuW(hOptions, MF_STRING, ID_FLIP_SIDE, L"Flip Side");
    AppendMenuW(hOptions, MF_STRING, ID_TOGGLE_AI, L"Toggle AI");
    AppendMenuW(hOptions, MF_STRING, ID_SHOW_LEGAL, L"Show Legal Moves");
    
    // Main Menu
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hGame, L"Game");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hOptions, L"Options");

    SetMenu(hwnd, hMenu);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int){
    SetDPIAwareness();
    InitStartingBoard();
    CreateFonts();

    WNDCLASSW wc = {}; wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = L"ChessFullClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); RegisterClassW(&wc);

    RECT r = {0,0, clientW, clientH};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND wnd = CreateWindowW(
		wc.lpszClassName,
		L"Chess (Full) - local & AI", 
		WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 
		CW_USEDEFAULT, 
		r.right-r.left, 
		r.bottom-r.top, 
		NULL, 
		NULL,
		hInst,
		NULL
	);
    if(!wnd) return 0;
    g_hwnd = wnd;
	 CreateMainMenu(wnd);
    ShowWindow(wnd, SW_SHOW);
    UpdateWindow(wnd);

    // main loop
    MSG msg;
    bool running = true;
    while(running){
		if (!gameOverG){
			Piece cur[8][8]; CopyBoard(boardG, cur);
			auto legal = GenerateLegalMoves(cur, sideToMoveG, lastMoveG);
			if(legal.empty() || halfmoveClock==100 || IsThreefoldRepetition(undoStack,boardG,sideToMoveG)){
				int kx, ky;
				if(!FindKing(cur, sideToMoveG, kx, ky)) { MessageBoxW(g_hwnd, L" king not found", L"Error", MB_OK);}
				else if(IsSquareAttacked(cur, kx, ky, Opp(sideToMoveG))){ MessageBoxW(g_hwnd, sideToMoveG==C_BLACK ? L"Checkmate: White wins":L"Checkmate: Black wins", L"Game Over", MB_OK);}
				else { MessageBoxW(g_hwnd, L"Stalemate", L"Draw", MB_OK); }
				gameOverG=true;
				InvalidateRect(g_hwnd, NULL, TRUE);
				
			}
			else if(aiOnG && sideToMoveG!=humanSide){
				
				Move best = ChooseBestFromLegal(cur, Opp(humanSide), lastMoveG, aiDepthG);
				if(best.fx!=-1){
					PushUndo();
				
					if(cur[best.fy][best.fx].type==PT_PAWN && (best.ty==0 || best.ty==7)) best.promoteTo = PT_QUEEN;
					ApplyMoveGlobal(best);
					InvalidateRect(g_hwnd, NULL, TRUE);
				}
				
			}
		}
        if(PeekMessage(&msg, NULL, 0,0, PM_REMOVE)){
            if(msg.message==WM_QUIT){ running=false; break;}
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    }

    return 0;
}