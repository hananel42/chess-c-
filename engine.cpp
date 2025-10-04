#include "chess.h"

// Set DPI awareness helper
void SetDPIAwareness(){
    HMODULE h = LoadLibraryW(L"user32.dll");
    if(h){
        typedef BOOL(WINAPI *PF)();
        PF p = (PF)GetProcAddress(h, "SetProcessDPIAware");
        if(p) p();
        FreeLibrary(h);
    }
}

void InitStartingBoard(){
    for(int y=0;y<8;y++) for(int x=0;x<8;x++) boardG[y][x] = Piece();
    // black back rank
    boardG[0][0] = {PT_ROOK,C_BLACK,false}; boardG[0][1] = {PT_KNIGHT,C_BLACK,false};
    boardG[0][2] = {PT_BISHOP,C_BLACK,false}; boardG[0][3] = {PT_QUEEN,C_BLACK,false};
    boardG[0][4] = {PT_KING,C_BLACK,false}; boardG[0][5] = {PT_BISHOP,C_BLACK,false};
    boardG[0][6] = {PT_KNIGHT,C_BLACK,false}; boardG[0][7] = {PT_ROOK,C_BLACK,false};
    for(int x=0;x<8;x++) boardG[1][x] = {PT_PAWN,C_BLACK,false};
    // white back rank
    boardG[7][0] = {PT_ROOK,C_WHITE,false}; boardG[7][1] = {PT_KNIGHT,C_WHITE,false};
    boardG[7][2] = {PT_BISHOP,C_WHITE,false}; boardG[7][3] = {PT_QUEEN,C_WHITE,false};
    boardG[7][4] = {PT_KING,C_WHITE,false}; boardG[7][5] = {PT_BISHOP,C_WHITE,false};
    boardG[7][6] = {PT_KNIGHT,C_WHITE,false}; boardG[7][7] = {PT_ROOK,C_WHITE,false};
    for(int x=0;x<8;x++) boardG[6][x] = {PT_PAWN,C_WHITE,false};

    sideToMoveG = C_WHITE;
    lastMoveG = Move();
    gameOverG = false;
	halfmoveClock=0;
}

// helper: copy
void CopyBoard(const Piece src[8][8], Piece dst[8][8]){ std::memcpy(dst, src, sizeof(Piece)*64); }

// find king
bool FindKing(const Piece b[8][8], Color side, int &outX, int &outY){
    for(int y=0;y<8;y++) for(int x=0;x<8;x++){
        if(b[y][x].type==PT_KING && b[y][x].color==side){ outX = x; outY = y; return true; }
    }
    return false;
}

// is square attacked by color 'by'
bool IsSquareAttacked(const Piece b[8][8], int sx, int sy, Color by){
    if(!OnBoard(sx,sy) || by==C_NONE) return false;

    // pawns attack
	if(by==C_WHITE){
		int py = sy + 1; // לבן תוקף מטה
		if(py<8){
			if(sx-1>=0 && b[py][sx-1].type==PT_PAWN && b[py][sx-1].color==C_WHITE) return true;
			if(sx+1<8  && b[py][sx+1].type==PT_PAWN && b[py][sx+1].color==C_WHITE) return true;
		}
	} else if(by==C_BLACK){
		int py = sy - 1; // שחור תוקף מעלה
		if(py>=0){
			if(sx-1>=0 && b[py][sx-1].type==PT_PAWN && b[py][sx-1].color==C_BLACK) return true;
			if(sx+1<8  && b[py][sx+1].type==PT_PAWN && b[py][sx+1].color==C_BLACK) return true;
		}
	}

    // knights
    const int kdx[8] = {1,2,2,1,-1,-2,-2,-1};
    const int kdy[8] = {-2,-1,1,2,2,1,-1,-2};
    for(int i=0;i<8;i++){
        int nx=sx+kdx[i], ny=sy+kdy[i];
        if(OnBoard(nx,ny) && b[ny][nx].type==PT_KNIGHT && b[ny][nx].color==by) return true;
    }

    // king adjacency
    for(int dx=-1;dx<=1;dx++) for(int dy=-1;dy<=1;dy++){
        if(dx==0 && dy==0) continue;
        int nx=sx+dx, ny=sy+dy;
        if(OnBoard(nx,ny) && b[ny][nx].type==PT_KING && b[ny][nx].color==by) return true;
    }

    // rook/queen straight
    const int rdx[4] = {1,-1,0,0}, rdy[4] = {0,0,1,-1};
    for(int d=0; d<4; d++){
        for(int s=1;;s++){
            int nx = sx + rdx[d]*s, ny = sy + rdy[d]*s;
            if(!OnBoard(nx,ny)) break;
            if(b[ny][nx].type!=PT_NONE){
                if(b[ny][nx].color==by && (b[ny][nx].type==PT_ROOK || b[ny][nx].type==PT_QUEEN)) return true;
                break;
            }
        }
    }

    // bishop/queen diagonal
    const int bdx[4] = {1,1,-1,-1}, bdy[4] = {1,-1,1,-1};
    for(int d=0; d<4; d++){
        for(int s=1;;s++){
            int nx = sx + bdx[d]*s, ny = sy + bdy[d]*s;
            if(!OnBoard(nx,ny)) break;
            if(b[ny][nx].type!=PT_NONE){
                if(b[ny][nx].color==by && (b[ny][nx].type==PT_BISHOP || b[ny][nx].type==PT_QUEEN)) return true;
                break;
            }
        }
    }

    return false;
}

// Pseudo-legal moves (use lastMove to determine en-passant candidates).
std::vector<Move> GeneratePseudoLegal(const Piece b[8][8], Color side, const Move &lastMove){
    std::vector<Move> out;
    for(int y=0;y<8;y++){
        for(int x=0;x<8;x++){
            Piece p = b[y][x];
            if(p.type==PT_NONE || p.color!=side) continue;
            if(p.type==PT_PAWN){
                int dir = (p.color==C_WHITE ? -1 : 1);
                int ny = y + dir;
                // forward one
                if(OnBoard(x,ny) && b[ny][x].type==PT_NONE) out.push_back(Move(x,y,x,ny));
                // forward two
                int startRow = (p.color==C_WHITE ? 6 : 1);
                if(y==startRow){
                    int ny1 = y+dir, ny2 = y+2*dir;
                    if(OnBoard(x,ny2) && b[ny1][x].type==PT_NONE && b[ny2][x].type==PT_NONE){
                        out.push_back(Move(x,y,x,ny2));
                    }
                }
                // diagonals (captures or en-passant)
                for(int dx=-1; dx<=1; dx+=2){
                    int nx=x+dx;
                    if(!OnBoard(nx, ny)) continue;
                    // normal capture
                    if(b[ny][nx].type!=PT_NONE && b[ny][nx].color==Opp(side)){
                        out.push_back(Move(x,y,nx,ny));
                    } else {
                        // en-passant possibility: lastMove must be pawn double-step landed at (nx, y) and the piece still there
                        if(lastMove.fx!=-1){
                            if(abs(lastMove.ty - lastMove.fy)==2 && lastMove.ty==y && lastMove.tx==nx){
                                if(OnBoard(lastMove.tx, lastMove.ty) && b[lastMove.ty][lastMove.tx].type==PT_PAWN && b[lastMove.ty][lastMove.tx].color==Opp(side)){
                                    Move m(x,y,nx,ny); m.isEnPassant = true; out.push_back(m);
                                }
                            }
                        }
                    }
                }
            }
            else if(p.type==PT_KNIGHT){
                const int kx[8] = {1,2,2,1,-1,-2,-2,-1};
                const int ky[8] = {-2,-1,1,2,2,1,-1,-2};
                for(int i=0;i<8;i++){
                    int nx=x+kx[i], ny=y+ky[i];
                    if(OnBoard(nx,ny) && b[ny][nx].color!=side) out.push_back(Move(x,y,nx,ny));
                }
            }
            else if(p.type==PT_BISHOP || p.type==PT_ROOK || p.type==PT_QUEEN){
                std::vector<std::pair<int,int>> dirs;
                if(p.type==PT_BISHOP || p.type==PT_QUEEN){
                    dirs.push_back({1,1}); dirs.push_back({1,-1}); dirs.push_back({-1,1}); dirs.push_back({-1,-1});
                }
                if(p.type==PT_ROOK || p.type==PT_QUEEN){
                    dirs.push_back({1,0}); dirs.push_back({-1,0}); dirs.push_back({0,1}); dirs.push_back({0,-1});
                }
                for(auto d:dirs){
                    int nx=x+d.first, ny=y+d.second;
                    while(OnBoard(nx,ny)){
                        if(b[ny][nx].type==PT_NONE) out.push_back(Move(x,y,nx,ny));
                        else { if(b[ny][nx].color==Opp(side)) out.push_back(Move(x,y,nx,ny)); break; }
                        nx += d.first; ny += d.second;
                    }
                }
            }
            else if(p.type==PT_KING){
                for(int dx=-1;dx<=1;dx++) for(int dy=-1;dy<=1;dy++){
                    if(dx==0 && dy==0) continue;
                    int nx=x+dx, ny=y+dy;
                    if(OnBoard(nx,ny) && b[ny][nx].color!=side) out.push_back(Move(x,y,nx,ny));
                }
                // castling pseudo
                if(!p.moved){
                    // kingside
                    if(OnBoard(7,y) && b[y][7].type==PT_ROOK && b[y][7].color==side && !b[y][7].moved){
                        if(b[y][5].type==PT_NONE && b[y][6].type==PT_NONE){
                            Move m(x,y,x+2,y); m.isCastle = true; out.push_back(m);
                        }
                    }
                    // queenside
                    if(OnBoard(0,y) && b[y][0].type==PT_ROOK && b[y][0].color==side && !b[y][0].moved){
                        if(b[y][1].type==PT_NONE && b[y][2].type==PT_NONE && b[y][3].type==PT_NONE){
                            Move m(x,y,x-2,y); m.isCastle = true; out.push_back(m);
                        }
                    }
                }
            }
        }
    }
    return out;
}

bool IsThreefoldRepetition(const std::vector<UndoEntry> &undoStack, const Piece currentBoard[8][8], Color sideToMove) {
    int repetitions = 0;

    for (auto it = undoStack.rbegin(); it != undoStack.rend(); ++it) {
        const UndoEntry &entry = *it;

        // בדיקה שהצד בתור זהה
        if (entry.side != sideToMove) continue;

        bool identical = true;

        // השוואת הלוח
        for (int y = 0; y < 8 && identical; ++y) {
            for (int x = 0; x < 8 && identical; ++x) {
                const Piece &p1 = currentBoard[y][x];
                const Piece &p2 = entry.board[y][x];

                if (p1.type != p2.type || p1.color != p2.color || p1.moved != p2.moved) {
                    identical = false;
                }
            }
        }

        // בדיקת זכויות en passant: אם ההזדמנות נעלמה, לא זהה
        if (identical && entry.lastMove.isEnPassant) {
            int epRow = (sideToMove == C_WHITE) ? 3 : 4; // שורה אפשרית ל-en passant
            bool epStillPossible = false;
            for (int x = 0; x < 8; ++x) {
                const Piece &p = currentBoard[epRow][x];
                if (p.type == PT_PAWN && p.color != sideToMove) {
                    epStillPossible = true;
                    break;
                }
            }
            if (!epStillPossible) identical = false;
        }

        if (identical) {
            repetitions++;
            if (repetitions >= 2) return true; // כולל המצב הנוכחי = שלוש פעמים
        }
    }

    return false;
}


// legal moves: simulate and filter out those leaving own king in check; also validate castling path
std::vector<Move> GenerateLegalMoves(const Piece b[8][8], Color side, const Move &lastMove){
    std::vector<Move> legal;
	
    auto pseudo = GeneratePseudoLegal(b, side, lastMove);

    for(auto m : pseudo){
        Piece copyB[8][8]; CopyBoard(b, copyB);

        // if en-passant, captured pawn sits at (m.tx, m.fy)
        if(m.isEnPassant){
            int capX = m.tx, capY = m.fy;
            if(OnBoard(capX,capY) && copyB[capY][capX].type==PT_PAWN && copyB[capY][capX].color==Opp(side)){
                copyB[capY][capX] = Piece();
            } else {
                continue; // invalid en-passant candidate
            }
        }

        // move piece
        copyB[m.ty][m.tx] = copyB[m.fy][m.fx];
        copyB[m.ty][m.tx].moved = true;
        copyB[m.fy][m.fx] = Piece();

        // castle rook movement
        if(m.isCastle){
            if(m.tx == m.fx + 2){
                // kingside
                if(copyB[m.ty][7].type==PT_ROOK){
                    copyB[m.ty][m.tx-1] = copyB[m.ty][7];
                    copyB[m.ty][7] = Piece();
                    copyB[m.ty][m.tx-1].moved = true;
                } else continue;
            } else if(m.tx == m.fx - 2){
                if(copyB[m.ty][0].type==PT_ROOK){
                    copyB[m.ty][m.tx+1] = copyB[m.ty][0];
                    copyB[m.ty][0] = Piece();
                    copyB[m.ty][m.tx+1].moved = true;
                } else continue;
            }
        }

        // promotion (assume queen for legality check; final selection handled in UI)
        if(copyB[m.ty][m.tx].type==PT_PAWN && (m.ty==0 || m.ty==7)){
            copyB[m.ty][m.tx].type = m.promoteTo;
        }

        // additional castling validations: king not in check now, and path squares not attacked
        if(m.isCastle){
            int kx=-1, ky=-1;
            if(!FindKing(b, side, kx, ky)) continue;
            if(IsSquareAttacked(b, kx, ky, Opp(side))) continue; // currently in check
            int dir = (m.tx > m.fx) ? 1 : -1;
            for(int step=1; step<=abs(m.tx - m.fx); ++step){
                int cx = m.fx + dir*step, cy = m.fy;
                Piece tB[8][8]; CopyBoard(b,tB);
                tB[cy][cx] = tB[m.fy][m.fx];
                tB[m.fy][m.fx] = Piece();
                if(IsSquareAttacked(tB, cx, cy, Opp(side))) goto skip_move;
            }
        }

        // ensure own king not left in check
        {
            int kx=-1, ky=-1;
            if(!FindKing(copyB, side, kx, ky)) continue;
            if(IsSquareAttacked(copyB, kx, ky, Opp(side))) { continue; }
        }

        legal.push_back(m);
        skip_move: ;
    }
    return legal;
}


// apply move to global board, update lastMove and side
void ApplyMoveGlobal(const Move &m){
    if(m.fx==-1) return;
    // en-passant capture removal
    if(m.isEnPassant){
        int capX = m.tx, capY = m.fy;
        if(OnBoard(capX,capY)) boardG[capY][capX] = Piece();
    }
    boardG[m.ty][m.tx] = boardG[m.fy][m.fx];
    boardG[m.ty][m.tx].moved = true;
    boardG[m.fy][m.fx] = Piece();

    if(m.isCastle){
        if(m.tx == m.fx + 2){
            boardG[m.ty][m.tx-1] = boardG[m.ty][7];
            boardG[m.ty][7] = Piece();
            boardG[m.ty][m.tx-1].moved = true;
        } else if(m.tx == m.fx - 2){
            boardG[m.ty][m.tx+1] = boardG[m.ty][0];
            boardG[m.ty][0] = Piece();
            boardG[m.ty][m.tx+1].moved = true;
        }
    }

    if(boardG[m.ty][m.tx].type==PT_PAWN && (m.ty==0 || m.ty==7)){
        boardG[m.ty][m.tx].type = m.promoteTo;
    }
	if(boardG[m.ty][m.tx].type==PT_PAWN)halfmoveClock=0;
	else halfmoveClock ++;
    lastMoveG = m;
    sideToMoveG = Opp(sideToMoveG);
}

// make move on copy (for search)
void MakeMoveOnCopy(Piece b[8][8], const Move &m){
    if(m.isEnPassant){
        int capX = m.tx, capY = m.fy;
        if(OnBoard(capX,capY)) b[capY][capX] = Piece();
    }
    b[m.ty][m.tx] = b[m.fy][m.fx];
    b[m.ty][m.tx].moved = true;
    b[m.fy][m.fx] = Piece();

    if(m.isCastle){
        if(m.tx == m.fx + 2){
            b[m.ty][m.tx-1] = b[m.ty][7];
            b[m.ty][7] = Piece();
            b[m.ty][m.tx-1].moved = true;
        } else if(m.tx == m.fx - 2){
            b[m.ty][m.tx+1] = b[m.ty][0];
            b[m.ty][0] = Piece();
            b[m.ty][m.tx+1].moved = true;
        }
    }

    if(b[m.ty][m.tx].type==PT_PAWN && (m.ty==0 || m.ty==7)){
        b[m.ty][m.tx].type = m.promoteTo;
    }
}

// Undo stack
void PushUndo(){
    UndoEntry e;
    CopyBoard(boardG, e.board);
    e.side = sideToMoveG;
	e.halfmoveClock = halfmoveClock;
    e.lastMove = lastMoveG;
    undoStack.push_back(e);
}
bool CanUndo(){ return !undoStack.empty(); }
void DoUndo(){
    if(!CanUndo()) return;
    UndoEntry e = undoStack.back(); undoStack.pop_back();
    CopyBoard(e.board, boardG);
    sideToMoveG = e.side;
    lastMoveG = e.lastMove;
	halfmoveClock = e.halfmoveClock;
    gameOverG = false;
    InvalidateRect(g_hwnd, NULL, TRUE);
}