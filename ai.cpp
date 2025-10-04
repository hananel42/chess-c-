#include "chess.h"
#include <unordered_map>
#include <mutex>
#include <chrono>

// Improved AI: quiescence search, transposition table (Zobrist), better move ordering (MVV-LVA + TT move),
// and safer multi-threading for top-level parallelization.
// The public API (EvaluateBoard, Negamax, ChooseBestFromLegal, etc.) is preserved.

struct TTEntry {
    int value;
    int depth;
    uint8_t flag; // 0 = exact, 1 = lowerbound, 2 = upperbound
    Move bestMove;
};

static std::unordered_map<uint64_t, TTEntry> tt;
static std::mutex ttMutex;
static bool zobristInitialized = false;

// Piece-square tables (from white's perspective). Mirror for black in evaluation.
static const int PST_PAWN[8][8] = {
    {  0,  0,  0,  0,  0,  0,  0,  0},
    { 50, 50, 50, 50, 50, 50, 50, 50},
    { 10, 10, 20, 30, 30, 20, 10, 10},
    {  5,  5, 10, 25, 25, 10,  5,  5},
    {  0,  0,  0, 20, 20,  0,  0,  0},
    {  5, -5,-10,  0,  0,-10, -5,  5},
    {  5, 10, 10,-20,-20, 10, 10,  5},
    {  0,  0,  0,  0,  0,  0,  0,  0}
};
static const int PST_KNIGHT[8][8] = {
    {-50,-40,-30,-30,-30,-30,-40,-50},
    {-40,-20,  0,  0,  0,  0,-20,-40},
    {-30,  0, 10, 15, 15, 10,  0,-30},
    {-30,  5, 15, 20, 20, 15,  5,-30},
    {-30,  0, 15, 20, 20, 15,  0,-30},
    {-30,  5, 10, 15, 15, 10,  5,-30},
    {-40,-20,  0,  5,  5,  0,-20,-40},
    {-50,-40,-30,-30,-30,-30,-40,-50}
};
static const int PST_BISHOP[8][8] = {
    {-20,-10,-10,-10,-10,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5, 10, 10,  5,  0,-10},
    {-10,  5,  5, 10, 10,  5,  5,-10},
    {-10,  0, 10, 10, 10, 10,  0,-10},
    {-10, 10, 10, 10, 10, 10, 10,-10},
    {-10,  5,  0,  0,  0,  0,  5,-10},
    {-20,-10,-10,-10,-10,-10,-10,-20}
};
static const int PST_ROOK[8][8] = {
    {  0,  0,  0,  5,  5,  0,  0,  0},
    {-5,  0,  0,  0,  0,  0,  0, -5},
    {-5,  0,  0,  0,  0,  0,  0, -5},
    {-5,  0,  0,  0,  0,  0,  0, -5},
    {-5,  0,  0,  0,  0,  0,  0, -5},
    {-5,  0,  0,  0,  0,  0,  0, -5},
    {  5, 10, 10, 10, 10, 10, 10,  5},
    {  0,  0,  0,  0,  0,  0,  0,  0}
};
static const int PST_QUEEN[8][8] = {
    {-20,-10,-10, -5, -5,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5,  5,  5,  5,  0,-10},
    { -5,  0,  5,  5,  5,  5,  0, -5},
    {  0,  0,  5,  5,  5,  5,  0, -5},
    {-10,  5,  5,  5,  5,  5,  0,-10},
    {-10,  0,  5,  0,  0,  0,  0,-10},
    {-20,-10,-10, -5, -5,-10,-10,-20}
};
static const int PST_KING[8][8] = {
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-20,-30,-30,-40,-40,-30,-30,-20},
    {-10,-20,-20,-20,-20,-20,-20,-10},
    { 20, 20,  0,  0,  0,  0, 20, 20},
    { 20, 30, 10,  0,  0, 10, 30, 20}
};

// Return PST value signed from perspective of side: positive for white advantage when white piece on a good square,
// negative for black advantage (we'll add/subtract based on piece color in EvaluateBoard).
inline int PSTValue(PieceType pt, int x, int y, Color col){
    if(pt==PT_NONE) return 0;
    int base = 0;
    if(col==C_WHITE){
        switch(pt){
            case PT_PAWN:   base = PST_PAWN[y][x]; break;
            case PT_KNIGHT: base = PST_KNIGHT[y][x]; break;
            case PT_BISHOP: base = PST_BISHOP[y][x]; break;
            case PT_ROOK:   base = PST_ROOK[y][x]; break;
            case PT_QUEEN:  base = PST_QUEEN[y][x]; break;
            case PT_KING:   base = PST_KING[y][x]; break;
            default: break;
        }
        return base;
    } else {
        int my = 7 - y;
        switch(pt){
            case PT_PAWN:   base = PST_PAWN[my][x]; break;
            case PT_KNIGHT: base = PST_KNIGHT[my][x]; break;
            case PT_BISHOP: base = PST_BISHOP[my][x]; break;
            case PT_ROOK:   base = PST_ROOK[my][x]; break;
            case PT_QUEEN:  base = PST_QUEEN[my][x]; break;
            case PT_KING:   base = PST_KING[my][x]; break;
            default: break;
        }
        // For black pieces, a positive PST base is good for black — but EvaluateBoard handles sign by subtracting for black.
        return base;
    }
}

// Piece base values (centipawns)
int pieceValue(PieceType t){
    switch(t){ case PT_PAWN: return 100; case PT_KNIGHT: return 320; case PT_BISHOP: return 330;
        case PT_ROOK: return 500; case PT_QUEEN: return 900; case PT_KING: return 20000; default: return 0; }
}

// Improved EvaluateBoard: clear, consistent sign handling and small improvements
int EvaluateBoard(const Piece b[8][8]){
    int score = 0;
    int mobilityWhite = 0, mobilityBlack = 0;
    for(int y=0;y<8;y++){
        for(int x=0;x<8;x++){
            Piece p = b[y][x];
            if(p.type==PT_NONE) continue;
            int material = pieceValue(p.type);
            int pst = PSTValue(p.type, x, y, p.color);
            if(p.color==C_WHITE){
                score += material;
                score += pst;
            } else {
                score -= material;
                score -= pst;
            }

            // mobility approximation (cheap)
            int mob = 0;
            if(p.type==PT_KNIGHT){
                const int kx[8] = {1,2,2,1,-1,-2,-2,-1};
                const int ky[8] = {-2,-1,1,2,2,1,-1,-2};
                for(int i=0;i<8;i++){ int nx=x+kx[i], ny=y+ky[i]; if(nx>=0&&nx<8&&ny>=0&&ny<8) if(b[ny][nx].color!=p.color) mob++; }
            } else if(p.type==PT_BISHOP || p.type==PT_ROOK || p.type==PT_QUEEN){
                const std::vector<std::pair<int,int>> dirs = (p.type==PT_BISHOP? std::vector<std::pair<int,int>>{{1,1},{1,-1},{-1,1},{-1,-1}} :
                    (p.type==PT_ROOK? std::vector<std::pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1}} :
                    std::vector<std::pair<int,int>>{{1,1},{1,-1},{-1,1},{-1,-1},{1,0},{-1,0},{0,1},{0,-1}}));
                for(auto d: dirs){
                    int nx=x+d.first, ny=y+d.second;
                    while(nx>=0&&nx<8&&ny>=0&&ny<8){
                        if(b[ny][nx].type==PT_NONE) mob++;
                        else { if(b[ny][nx].color!=p.color) mob++; break; }
                        nx+=d.first; ny+=d.second;
                    }
                }
            } else if(p.type==PT_PAWN){
                int dir = (p.color==C_WHITE? -1: 1);
                int ny = y + dir;
                if(ny>=0 && ny<8){
                    if(x-1>=0 && b[ny][x-1].color!=p.color) mob++;
                    if(x+1<8  && b[ny][x+1].color!=p.color) mob++;
                    if(b[ny][x].type==PT_NONE) mob++;
                }
            } else if(p.type==PT_KING){
                for(int dx=-1;dx<=1;dx++) for(int dy=-1;dy<=1;dy++){
                    if(dx==0 && dy==0) continue;
                    int nx=x+dx, ny=y+dy;
                    if(nx>=0&&nx<8&&ny>=0&&ny<8) if(b[ny][nx].color!=p.color) mob++;
                }
            }
            if(p.color==C_WHITE) mobilityWhite += mob; else mobilityBlack += mob;
        }
    }
    score += (mobilityWhite - mobilityBlack) * 4;
    return score;
}

int EvalForSide(const Piece b[8][8], Color side){ int v = EvaluateBoard(b); return (side==C_WHITE)?v:-v; }

// ---------------- Zobrist hashing ----------------

// Initialize zobrist table once
static void InitZobristIfNeeded() {
    if (zobristInitialized) return;
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    for(int y=0;y<8;y++){
        for(int x=0;x<8;x++){
            for(int k=0;k<12;k++){
                zobristTable[y][x][k] = dist(rng);
            }
        }
    }
    zobristInitialized = true;
}

// Map piece (type+color) to index 0..11: white(PAWN..KING)=0..5, black = 6..11
static inline int PieceIndex(const Piece &p){
    if(p.type==PT_NONE) return -1;
    int t = (int)p.type - 1; // 0..5
    if(p.color==C_WHITE) return t;
    return 6 + t;
}

uint64_t ComputeZobrist(const Piece b[8][8], Color sideToMove){
    InitZobristIfNeeded();
    uint64_t h = 1469598103934665603ULL; // FNV offset basis (not required but non-zero)
    for(int y=0;y<8;y++){
        for(int x=0;x<8;x++){
            int idx = PieceIndex(b[y][x]);
            if(idx>=0) h ^= zobristTable[y][x][idx];
        }
    }
    // mix side
    if(sideToMove==C_BLACK) h ^= 0xF0F0F0F0F0F0F0F0ULL;
    return h;
}

// ---------------- Move ordering ----------------

// MVV-LVA-ish priority for captures: victimValue * 100 - attackerValue
static int MoveHeuristicScore(const Piece cur[8][8], const Move &m, const Move *ttMove = nullptr){
    int score = 0;
    // TT move gets big bonus
    if(ttMove && ttMove->fx==m.fx && ttMove->fy==m.fy && ttMove->tx==m.tx && ttMove->ty==m.ty) score += 2000000;
    // captures
    if(m.isEnPassant){
        score += 120000; // fairly good capture
    } else {
        Piece victim = cur[m.ty][m.tx];
        Piece attacker = cur[m.fy][m.fx];
        if(victim.type != PT_NONE){
            int vVal = pieceValue(victim.type);
            int aVal = pieceValue(attacker.type);
            score += 100000 + vVal*100 - aVal; // prefer capturing valuable victims with light attackers
        }
        // promotion
        if(attacker.type == PT_PAWN && (m.ty==0 || m.ty==7)){
            score += 80000;
        }
        if(m.isCastle) score += 5000;
    }
    return score;
}

// ---------------- Quiescence search ----------------

int Quiescence(Piece b[8][8], Color side, const Move &lastMv, int alpha, int beta){
    int stand = EvalForSide(b, side);
    if(stand >= beta) return beta;
    if(alpha < stand) alpha = stand;

    // generate capture-like moves only (captures, promotions, en-passant)
    auto pseudo = GeneratePseudoLegal(b, side, lastMv);
    // filter to "noisy" moves
    std::vector<Move> noisy;
    for(auto &m : pseudo){
        // consider only captures, en-passant, promotions
        if(m.isEnPassant) noisy.push_back(m);
        else if(b[m.ty][m.tx].type != PT_NONE) noisy.push_back(m);
        else if(b[m.fy][m.fx].type == PT_PAWN && (m.ty==0 || m.ty==7)) noisy.push_back(m);
    }
    if(noisy.empty()) return stand;

    // order noisy moves by MVV-LVA heuristic
    std::sort(noisy.begin(), noisy.end(), [&](const Move &a, const Move &c){
        return MoveHeuristicScore(b, a) > MoveHeuristicScore(b, c);
    });

    for(auto &m : noisy){
        Piece nb[8][8]; CopyBoard(b, nb);
        MakeMoveOnCopy(nb, m);
        int score = -Quiescence(nb, Opp(side), m, -beta, -alpha);
        if(score >= beta) return beta;
        if(score > alpha) alpha = score;
    }
    return alpha;
}

// ---------------- Negamax with TT and quiescence ----------------

int Negamax(Piece b[8][8], Color side, const Move &lastMv, int depth, int alpha, int beta){
    // terminal / draw detection responsibilities are left to caller (as before)
    uint64_t key = ComputeZobrist(b, side);

    // Probe transposition table
    {
        std::lock_guard<std::mutex> lk(ttMutex);
        auto it = tt.find(key);
        if(it != tt.end()){
            const TTEntry &e = it->second;
            if(e.depth >= depth){
                if(e.flag == 0) return e.value; // exact
                if(e.flag == 1) alpha = std::max(alpha, e.value); // lowerbound
                else if(e.flag == 2) beta = std::min(beta, e.value); // upperbound
                if(alpha >= beta) return e.value;
            }
        }
    }

    if(depth == 0){
        // use quiescence at leaf
        int q = Quiescence(b, side, lastMv, alpha, beta);
        return q;
    }

    auto legal = GenerateLegalMoves(b, side, lastMv);
    if(legal.empty()){
        int kx, ky;
        if(!FindKing(b, side, kx, ky)) return -1000000;
        if(IsSquareAttacked(b,kx,ky,Opp(side))) return -1000000; // mate
        return 0; // stalemate
    }

    // Move ordering: try TT best move first (if present)
    Move ttMove;
    {
        std::lock_guard<std::mutex> lk(ttMutex);
        auto it = tt.find(key);
        if(it != tt.end()){
            ttMove = it->second.bestMove;
        } else {
            ttMove = Move();
        }
    }

    std::sort(legal.begin(), legal.end(), [&](const Move &a, const Move &c){
        return MoveHeuristicScore(b, a, &ttMove) > MoveHeuristicScore(b, c, &ttMove);
    });

    int bestVal = -10000000;
    Move bestMoveLocal;

    for(auto &m : legal){
        Piece copyB[8][8]; CopyBoard(b, copyB);
        MakeMoveOnCopy(copyB, m);
        int val = -Negamax(copyB, Opp(side), m, depth-1, -beta, -alpha);
        if(val > bestVal){
            bestVal = val;
            bestMoveLocal = m;
        }
        if(val > alpha) alpha = val;
        if(alpha >= beta) break;
    }

    // store in TT
    {
        std::lock_guard<std::mutex> lk(ttMutex);
        TTEntry entry;
        entry.value = bestVal;
        entry.depth = depth;
        entry.bestMove = bestMoveLocal;
        if(bestVal <= alpha) entry.flag = 2; // upperbound
        else if(bestVal >= beta) entry.flag = 1; // lowerbound
        else entry.flag = 0; // exact
        tt[key] = entry;
    }

    return bestVal;
}

// ---------------- Top-level chooser ----------------

// For simplicity and stability we use parallel first-ply evaluation as before,
// but we leverage the transposition table and better ordering. Each async worker
// will reuse the shared TT (protected by mutex).
Move ChooseBestFromLegal(const Piece cur[8][8], Color side, const Move &lastMv, int depth){
    auto legal = GenerateLegalMoves(cur, side, lastMv);
    if(legal.empty()) return Move();

    // Quick shallow evaluation if depth <= 1
    if(depth <= 1){
        Move best = legal[0];
        int bestVal = -100000000;
        for(auto &m: legal){
            Piece copyB[8][8]; CopyBoard(cur, copyB);
            MakeMoveOnCopy(copyB, m);
            int val = EvalForSide(copyB, side);
            if(val > bestVal){ bestVal = val; best = m; }
        }
        return best;
    }

    // Order moves by heuristic before launching threads (ttMove may help)
    uint64_t key = ComputeZobrist(cur, side);
    Move ttMove;
    {
        std::lock_guard<std::mutex> lk(ttMutex);
        auto it = tt.find(key);
        if(it != tt.end()) ttMove = it->second.bestMove;
    }
    std::sort(legal.begin(), legal.end(), [&](const Move &a, const Move &c){
        return MoveHeuristicScore(cur, a, &ttMove) > MoveHeuristicScore(cur, c, &ttMove);
    });

    // Launch async tasks for each child move, but cap the number of parallel tasks to hardware concurrency
    unsigned hw = std::thread::hardware_concurrency();
    if(hw < 1) hw = 2;
    // We'll still create futures for each move but rely on system scheduling; keep the previous async usage.
    std::vector<std::future<std::pair<int,Move>>> futures;
    futures.reserve(legal.size());

    for(auto &m : legal){
        futures.push_back(std::async(std::launch::async, [cur, side, lastMv, m, depth]()->std::pair<int,Move>{
            Piece copyB[8][8]; CopyBoard(cur, copyB);
            MakeMoveOnCopy(copyB, m);
            int val = -Negamax(copyB, Opp(side), m, depth-1, -100000000, 100000000);
            return {val, m};
        }));
    }

    Move best = legal[0];
    int bestVal = -100000000;
    for(auto &f : futures){
        auto pr = f.get();
        int val = pr.first;
        Move m = pr.second;
        if(val > bestVal){
            bestVal = val;
            best = m;
        }
    }

    // write best move for root position to TT (cheap)
    {
        std::lock_guard<std::mutex> lk(ttMutex);
        TTEntry e; e.value = bestVal; e.depth = depth; e.flag = 0; e.bestMove = best;
        tt[ComputeZobrist(cur, side)] = e;
    }

    return best;
}