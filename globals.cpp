#include "chess.h"

// Definitions of globals (previously static in single-file)
uint64_t zobristTable[8][8][12];
Piece boardG[8][8];
Color humanSide = C_WHITE;
Color sideToMoveG = C_WHITE;
Move lastMoveG;
bool gameOverG = false;
int halfmoveClock = 0;
bool showLegalG = true;
bool aiOnG = false;
int aiDepthG = 3;
bool flipBoardG = false;
int clientW = 1000, clientH = 1000;
int squareSize = 80;
int boardLeft = 30, boardTop = 100;
std::vector<UndoEntry> undoStack;
HWND g_hwnd = NULL;
std::mt19937 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());
HFONT glyphFont = NULL;
HFONT uiFont = NULL;