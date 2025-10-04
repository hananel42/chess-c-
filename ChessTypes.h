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
