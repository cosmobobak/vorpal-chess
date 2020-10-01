#include <iostream>
#include <algorithm>
#include <vector>
#include <string>

#define INF 10000000

#define U64 unsigned long long
#define U32 unsigned __int32
#define U16 unsigned __int16
#define U8 unsigned __int8
#define S64 signed __int64
#define S32 signed __int32
#define S16 signed __int16
#define S8 signed __int8

enum e_piece {
  KING,
  QUEEN,
  ROOK,
  BISHOP,
  KNIGHT,
  PAWN,
  PIECE_EMPTY
};

enum e_color {
  WHITE,
  BLACK,
  COLOR_EMPTY
};

enum e_square {
  A1, B1, C1, D1, E1, F1, G1, H1,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A8, B8, C8, D8, E8, F8, G8, H8
};

class Move
{
public:
    int from_square;
    int to_square;
    int piece;
    bool color;
    int cPiece;
    int cColor;
    bool iscapture;

    Move(int f = 0, int t = 0, int p = 1, bool c = 0, int cp = 13)
    {
        from_square = f;
        to_square = t;
        piece = p;
        color = c;
        cPiece = cp;
        cColor = !c;
        if (cp == 13)
        {
            iscapture = false;
        }
        else
        {
            iscapture = true;
        }
    }
};

struct s_searchTracker
{
    bool myside;
    U8 depth;
    int history[128][128];
    Move killers[1024][2];
    U64 nodes;
    S32 movetime;
    U64 q_nodes;
    unsigned long starttime;
};

std::ostream &operator<<(std::ostream &os, const Move &obj)
{
    os << "Move from " << obj.from_square << " to " << obj.to_square;
    return os;
}

auto square_from_an(std::string an_square) -> int
{
    int a = an_square[0] - 97;
    int b = an_square[1] - '0' - 1;
    return 63 - (a + 8 * b);
}

class Board
{
public:
    char pieces[13] = {'p', 'n', 'b', 'r', 'q', 'k', 'P', 'N', 'B', 'R', 'Q', 'K', '.'};
    U64 BB_PIECES[6] = {
        0b0000000011111111000000000000000000000000000000001111111100000000,
        0b0100001000000000000000000000000000000000000000000000000001000010,
        0b0010010000000000000000000000000000000000000000000000000000100100,
        0b1000000100000000000000000000000000000000000000000000000010000001,
        0b0000100000000000000000000000000000000000000000000000000000001000,
        0b0001000000000000000000000000000000000000000000000000000000010000,
    };
    U64 BB_OCCUPIED =    0b1111111111111111000000000000000000000000000000001111111111111111;
    U64 BB_EMPTY =       0b0000000000000000111111111111111111111111111111110000000000000000;
    
    U64 BB_COLORS[2] = {
        0b1111111111111111000000000000000000000000000000000000000000000000,
        0b0000000000000000000000000000000000000000000000001111111111111111,
    };
    U64 MASK[64] = {};
    std::vector<Move> stack;

    bool turn = 0;

    Board()
    {
        for (int i = 0; i < 64; i++)
        {
            MASK[i] = 1LL << i;
        }
    }

    void show()
    {
        for (int i = 0; i < 64; i++)
        {
            std::cout << pieces[colored_piece_type_at(i)];
            std::cout << " ";
            if (i % 8 == 7)
            {
                std::cout << "\n";
            }
        }
    }

    auto get_square(U64 bb, int squareNum) -> bool
    {
        return (bb & (1LL << squareNum));
    }

    auto color_at(int i) -> bool
    {
        if(get_square(BB_COLORS[0], i))
        {
            return 0;
        }else{
            return 1;
        }
    }

    auto move_from_uci(std::string notation) -> Move
    {
        int f, t, p, cp;
        bool c;
        f = square_from_an(notation.substr(0, 2));
        t = square_from_an(notation.substr(2, 4));
        p = colored_piece_type_at(f);
        c = color_at(f);
        cp = colored_piece_type_at(t);
        return Move(f, t, p, c, cp);
    }

    void make(Move *move) //adapted from chessprogrammingwiki
    {
        U64 fromBB = 1LL << move->from_square;
        U64 toBB = 1LL << move->to_square;
        U64 fromToBB = fromBB ^ toBB; // |+
        BB_PIECES[move->piece] ^= fromToBB;          // update piece bitboard
        BB_PIECES[move->color] ^= fromToBB;          // update white or black color bitboard
        if (move->iscapture)
        {
            BB_PIECES[move->cPiece] ^= toBB; // reset the captured piece
            BB_PIECES[move->cColor] ^= toBB; // update color bitboard by captured piece
        }
        BB_OCCUPIED ^= fromToBB; // update occupied ...
        BB_EMPTY ^= fromToBB;    // ... and empty bitboard
    }

    void flip_piece(int piece_type, int squareNum) //piece_type should be in range(0, 6)
    {
        BB_PIECES[piece_type] = BB_PIECES[piece_type] ^ 1LL << squareNum;
        BB_OCCUPIED = BB_OCCUPIED ^ 1LL << squareNum;
        if(get_square(BB_COLORS[0], squareNum))
        {
            BB_COLORS[0] = BB_COLORS[0] ^ 1LL << squareNum;
        }else{
            BB_COLORS[1] = BB_COLORS[1] ^ 1LL << squareNum;
        }
    }

    auto piece_type_at(int squareNum) -> int
    {
        //Gets the piece type at the given square.
        U64 mask = MASK[squareNum];

        if (!(BB_OCCUPIED & mask))
        {
            return 12; // Early return
        }
        else if (BB_PIECES[0] & mask)
        {
            return 0;
        }
        else if (BB_PIECES[1] & mask)
        {
            return 1;
        }
        else if (BB_PIECES[2] & mask)
        {
            return 2;
        }
        else if (BB_PIECES[3] & mask)
        {
            return 3;
        }
        else if (BB_PIECES[4] & mask)
        {
            return 4;
        }
        else
        {
            return 5;
        }
    }

    auto colored_piece_type_at(int squareNum) -> int
    {
        //Gets the piece type at the given square.
        U64 mask = MASK[squareNum];
        int mod;
        if (BB_COLORS[1] & mask)
        {
            mod = 0;
        }
        else
        {
            mod = 6;
        }

        if (!(BB_OCCUPIED & mask))
        {
            return 12; // Early return
        }
        else if (BB_PIECES[0] & mask)
        {
            return 0 + mod;
        }
        else if (BB_PIECES[1] & mask)
        {
            return 1 + mod;
        }
        else if (BB_PIECES[2] & mask)
        {
            return 2 + mod;
        }
        else if (BB_PIECES[3] & mask)
        {
            return 3 + mod;
        }
        else if (BB_PIECES[4] & mask)
        {
            return 4 + mod;
        }
        else
        {
            return 5 + mod;
        }
    }

    auto is_checkmate() -> bool
    {
        return false;
    }

    auto can_claim_fifty_moves() -> bool
    {
        return false;
    }

    void push(Move edge)
    {
        auto layer = colored_piece_type_at(edge.from_square);

        flip_piece(layer % 6, edge.from_square);
        for (int i = 0; i < 6; i++)
        {
            BB_PIECES[i] &= ~(1LL << edge.to_square);
        }
        flip_piece(layer % 6, edge.to_square);
    }

    void play(Move edge)
    {
        push(edge);
        stack.push_back(edge);
    }
};

class Vorpal
{
public:
    int nodes = 0;
    Board node = Board();
    //int tableSize = SOME PRIME NUMBER;
    //Entry hashtable[tableSize];
    int timeLimit = 1;
    //int startTime = time();
    //include the advanced time control later
    bool human = false;
    bool useBook = false;
    bool inBook = true;
    bool variedBook = false;
    int contempt = 3000;
    bool oddeven = true;
    //Move best = Move();
    int pieceValue[5] = {
        1000,
        3200,
        3330,
        5100,
        8800,
    };
    int pieceSquareTable[12][64] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 50, 50, 50, 50, 50, 50, 50, 50, 10, 10, 20, 30, 30, 20, 10, 10, 5, 5, 10, 25, 25, 10, 5, 5, 0, 0, 0, 20, 20, 0, 0, 0, 5, -5, -10, 0, 0, -10, -5, 5, 5, 10, 10, -20, -20, 10, 10, 5, 0, 0, 0, 0, 0, 0, 0, 0},
        {-50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0, 0, 0, 0, -20, -40, -30, 0, 10, 15, 15, 10, 0, -30, -30, 5, 15, 20, 20, 15, 5, -30, -30, 0, 15, 20, 20, 15, 0, -30, -30, 5, 10, 15, 15, 10, 5, -30, -40, -20, 0, 5, 5, 0, -20, -40, -50, -40, -30, -30, -30, -30, -40, -50},
        {-20, -10, -10, -10, -10, -10, -10, -20, -10, 0, 0, 0, 0, 0, 0, -10, -10, 0, 5, 10, 10, 5, 0, -10, -10, 5, 5, 10, 10, 5, 5, -10, -10, 0, 10, 10, 10, 10, 0, -10, -10, 10, 10, 10, 10, 10, 10, -10, -10, 5, 0, 0, 0, 0, 5, -10, -20, -10, -10, -10, -10, -10, -10, -20},
        {0, 0, 0, 0, 0, 0, 0, 0, 5, 10, 10, 10, 10, 10, 10, 5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 5, 5, 0, 0, 0},
        {-20, -10, -10, -5, -5, -10, -10, -20, -10, 0, 0, 0, 0, 0, 0, -10, -10, 0, 5, 5, 5, 5, 0, -10, -5, 0, 5, 5, 5, 5, 0, -5, 0, 0, 5, 5, 5, 5, 0, -5, -10, 5, 5, 5, 5, 5, 0, -10, -10, 0, 5, 0, 0, 0, 0, -10, -20, -10, -10, -5, -5, -10, -10, -20},
        {-30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -20, -20, -20, -20, -10, 20, 20, 0, 0, 0, 0, 20, 20, 20, 30, 10, 0, 0, 10, 30, 20},
        {0, 0, 0, 0, 0, 0, 0, 0, 5, 10, 10, -20, -20, 10, 10, 5, 5, -5, -10, 0, 0, -10, -5, 5, 0, 0, 0, 20, 20, 0, 0, 0, 5, 5, 10, 25, 25, 10, 5, 5, 10, 10, 20, 30, 30, 20, 10, 10, 50, 50, 50, 50, 50, 50, 50, 50, 0, 0, 0, 0, 0, 0, 0, 0},
        {-50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0, 5, 5, 0, -20, -40, -30, 5, 10, 15, 15, 10, 5, -30, -30, 0, 15, 20, 20, 15, 0, -30, -30, 5, 15, 20, 20, 15, 5, -30, -30, 0, 10, 15, 15, 10, 0, -30, -40, -20, 0, 0, 0, 0, -20, -40, -50, -40, -30, -30, -30, -30, -40, -50},
        {-20, -10, -10, -10, -10, -10, -10, -20, -10, 5, 0, 0, 0, 0, 5, -10, -10, 10, 10, 10, 10, 10, 10, -10, -10, 0, 10, 10, 10, 10, 0, -10, -10, 5, 5, 10, 10, 5, 5, -10, -10, 0, 5, 10, 10, 5, 0, -10, -10, 0, 0, 0, 0, 0, 0, -10, -20, -10, -10, -10, -10, -10, -10, -20},
        {0, 0, 0, 5, 5, 0, 0, 0, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, 5, 10, 10, 10, 10, 10, 10, 5, 0, 0, 0, 0, 0, 0, 0, 0},
        {-20, -10, -10, -5, -5, -10, -10, -20, -10, 0, 0, 0, 0, 5, 0, -10, -10, 0, 5, 5, 5, 5, 5, -10, -5, 0, 5, 5, 5, 5, 0, 0, -5, 0, 5, 5, 5, 5, 0, -5, -10, 0, 5, 5, 5, 5, 0, -10, -10, 0, 0, 0, 0, 0, 0, -10, -20, -10, -10, -5, -5, -10, -10, -20},
        {20, 30, 10, 0, 0, 10, 30, 20, 20, 20, 0, 0, 0, 0, 20, 20, -10, -20, -20, -20, -20, -20, -20, -10, -20, -30, -30, -40, -40, -30, -30, -20, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30},
    };

    Vorpal()
    {
        int testVar = 0;
    }

    auto evaluate(int depth) -> int
    {
        nodes++;
        int mod;
        if (node.turn)
        {
            mod = 1;
        }
        else
        {
            mod = -1;
        }
        int rating = 0;
        if (node.is_checkmate())
        {
            return 1000000000 * (depth + 1) * mod;
        }
        if (node.can_claim_fifty_moves())
        {
            rating = -contempt * mod;
        }

        return rating;
    }

    auto principal_variation_search(int depth, int color, int a = -200000, int b = 200000) -> int
    {
    }
};

auto main() -> int
{
    Vorpal engine;
    Board board;
    Move moves[] = {board.move_from_uci("e2e4"), board.move_from_uci("e7e5")};
    board.show();
    std::cout << std::endl;
    board.make(moves);
    board.show();
    std::cout << std::endl;
    board.make(moves + 1);
    board.show();
    std::cout << board.move_from_uci("e2e4");
    return 0;
}

//TODO: make moves move both correct piece type and color
//fix board flip coordinate stuff
//MOVE GENERATOR
//RULES??