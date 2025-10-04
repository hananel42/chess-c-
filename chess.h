#ifndef CHESS_H
#define CHESS_H

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#include <windows.h>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstring>
#include <optional>
#include <future>
#include <atomic>

// types/enums
enum PieceType { PT_NONE=0, PT_PAWN, PT_KNIGHT, PT_BISHOP, PT_ROOK, PT_QUEEN, PT_KING };
enum Color { C_NONE=0, C_WHITE=1, C_BLACK=2 };
enum MenuIDs {ID_NEW_GAME = 1,ID_UNDO,ID_TOGGLE_AI,ID_FLIP_BOARD,ID_FLIP_SIDE,ID_SHOW_LEGAL,ID_EXIT};

struct Piece {
    PieceType type = PT_NONE;
    Color color = C_NONE;
    bool moved = false;
};

struct Move {
    int fx=-1, fy=-1, tx=-1, ty=-1;
    bool isEnPassant=false;
    bool isCastle=false;
    PieceType promoteTo = PT_QUEEN;
    Move() {}
    Move(int a,int b,int c,int d):fx(a),fy(b),tx(c),ty(d){}
};

// Undo: store full board snapshot and side and lastMove
struct UndoEntry {
    Piece board[8][8];
	int halfmoveClock;
    Color side;
    Move lastMove;
};

// helpers
inline Color Opp(Color c){ return c==C_WHITE?C_BLACK:(c==C_BLACK?C_WHITE:C_NONE); }
inline bool OnBoard(int x,int y){ return x>=0 && x<8 && y>=0 && y<8; }

// Globals (defined in globals.cpp)
extern uint64_t zobristTable[8][8][12];
extern Piece boardG[8][8];
extern Color humanSide;
extern Color sideToMoveG;
extern Move lastMoveG;
extern bool gameOverG;
extern int halfmoveClock;
extern bool showLegalG;
extern bool aiOnG;
extern int aiDepthG;
extern bool flipBoardG;
extern int clientW;
extern int clientH;
extern int squareSize;
extern int boardLeft;
extern int boardTop;
extern std::vector<UndoEntry> undoStack;
extern HWND g_hwnd;
extern std::mt19937 rng;
extern HFONT glyphFont;
extern HFONT uiFont;

// Function prototypes - engine
void SetDPIAwareness();
void InitStartingBoard();
void CopyBoard(const Piece src[8][8], Piece dst[8][8]);
bool FindKing(const Piece b[8][8], Color side, int &outX, int &outY);
bool IsSquareAttacked(const Piece b[8][8], int sx, int sy, Color by);
std::vector<Move> GeneratePseudoLegal(const Piece b[8][8], Color side, const Move &lastMove);
std::vector<Move> GenerateLegalMoves(const Piece b[8][8], Color side, const Move &lastMove);
bool IsThreefoldRepetition(const std::vector<UndoEntry> &undoStack, const Piece currentBoard[8][8], Color sideToMove);
void ApplyMoveGlobal(const Move &m);
void MakeMoveOnCopy(Piece b[8][8], const Move &m);

// Undo
void PushUndo();
bool CanUndo();
void DoUndo();

// AI
int pieceValue(PieceType t);
int EvaluateBoard(const Piece b[8][8]);
int EvalForSide(const Piece b[8][8], Color side);
int Negamax(Piece b[8][8], Color side, const Move &lastMv, int depth, int alpha, int beta);
Move ChooseBestFromLegal(const Piece cur[8][8], Color side, const Move &lastMv, int depth);

// UI / Win32
wchar_t Glyph(const Piece &p);
void CreateFonts();
void DrawBoardAndUI(HDC hdcScreen);
bool ScreenToBoard(int sx,int sy,int &bx,int &by);
std::optional<PieceType> DoPromotionDialog(HWND hwndParent);
PieceType ChoosePromotion(HWND parent, Color color);

void newGame();
void flipBoard();
void flipSide();
void toggleAi();
void toggleShowLegal();
void CreateMainMenu(HWND hwnd);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // CHESS_H