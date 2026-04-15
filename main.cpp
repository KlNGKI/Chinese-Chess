#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <cmath>
#include <map>
#include <cassert>
#include <algorithm>
#include "jsoncpp/json.h"
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <unordered_map>
#include <sstream>


using std::string;
using std::vector;
using std::map;
using std::cin;
using std::cout;
using std::endl;

// ---------------------------------------------------------------------------
// 棋盘基础定义与符号映射
// ---------------------------------------------------------------------------
#define BOARDWIDTH 9
#define BOARDHEIGHT 10
enum stoneType {None = 0, King=1, Bishop=2, Knight=3, Rook=4, Pawn=5, Cannon=6, Assistant=7};
enum colorType {BLACK=0, RED=1, EMPTY = 2};
int baseLine[2] = {9, 0};
char stoneSym[] = "*kbnrpca";
map<char,stoneType> charSym = {{stoneSym[0], None}, {stoneSym[1], King},
                               {stoneSym[2], Bishop}, {stoneSym[3], Knight},
                               {stoneSym[4], Rook}, {stoneSym[5], Pawn},
                               {stoneSym[6], Cannon}, {stoneSym[7], Assistant}};

// ---------------------------------------------------------------------------
// 方向数组与棋子移动辅助常量
// ---------------------------------------------------------------------------
int dx[] = {-1,-1,-1,0,0,1,1,1};
int dy[] = {-1,0,1,-1,1,-1,0,1};
int dx_ob[] = {-1,1,-1,1};
int dy_ob[] = {-1,-1,1,1};
int dx_strai[] = {-1,0,0,1};
int dy_strai[] = {0,-1,1,0};
int dx_lr[] = {-1,1};
int dx_knight[] = {-2,-2,-1,-1,1,1,2,2};
int dy_knight[] = {-1,1,-2,2,-2,2,-1,1};
int dx_knight_foot[] = {-1,-1,0,0,0,0,1,1};
int dy_knight_foot[] = {0,0,-1,1,-1,1,0,0};
int dx_bishop[] = {-2,-2,2,2};
int dy_bishop[] = {-2,2,-2,2};
int dx_bishop_eye[] = {-1,-1,1,1};
int dy_bishop_eye[] = {-1,1,-1,1};


// ---------------------------------------------------------------------------
// 坐标转换工具：棋盘坐标与字符串表示之间的互换。
// ---------------------------------------------------------------------------
int pgnchar2int(char c) {
    return (int)c - (int)'a';
}

char pgnint2char(int i) {
    return (char)((int)'a' + i);
}

int char2int(char c) {
    return (int)c - (int)'0';
}

char int2char(int i) {
    return (char)((int)'0' + i);
}

// ---------------------------------------------------------------------------
// Move / Grid: 简单数据结构，表示一次走法和棋盘格子信息。
// ---------------------------------------------------------------------------
struct Move {
    int source_x, source_y, target_x, target_y;
    Move() {source_x = -1; source_y = -1; target_x = -1; target_y = -1;}
    Move(int msx, int msy, int mtx, int mty): source_x(msx), source_y(msy),
                                                                target_x(mtx), target_y(mty) {;}
    Move(string msource, string mtarget) {
        source_x = pgnchar2int(msource[0]);
        source_y = char2int(msource[1]);
        target_x = pgnchar2int(mtarget[0]);
        target_y = char2int(mtarget[1]);
    }
    bool operator ==(const Move& other) const {
        return source_x == other.source_x && source_y == other.source_y && target_x == other.target_x
               && target_y == other.target_y;
    }
};

struct Grid;

struct Grid {
    stoneType type;
    colorType color;
    Grid () : type(None), color(EMPTY) {;}
    Grid (stoneType mtype, colorType mcolor):
            type(mtype), color(mcolor) {;}
    Grid& operator = (const Grid& other) {
        if (this != &other) {
            type = other.type;
            color = other.color;
        }
        return *this;
    }
    bool operator == (const Grid& other) const {
        return type == other.type && color == other.color;
    }
};

struct MoveState {
    int sourcePos;
    int targetPos;
    Grid sourceGrid;
    Grid targetGrid;
    uint64_t previousHash;
};

class Chess;

class PieceValueEvaluator {
public:
    virtual ~PieceValueEvaluator() = default;

    virtual int baseValue(stoneType type) const {
        switch (type) {
            case King: return 100000;
            case Rook: return 650;
            case Cannon: return 520;
            case Knight: return 360;
            case Bishop: return 250;
            case Assistant: return 240;
            case Pawn: return 120;
            default: return 0;
        }
    }

    virtual int positionValue(const Chess& /*chess*/, int x, int y, stoneType type, colorType color) const {
        int bonus = 0;
        bool isRed = color == RED;

        switch (type) {
            case King:
                if ((isRed && x == 4 && y == 1) || (!isRed && x == 4 && y == 8)) {
                    bonus += 20;
                }
                break;
            case Rook:
                if (x == 4) bonus += 12;
                if (y == 4 || y == 5) bonus += 10;
                break;
            case Cannon:
                if (x >= 3 && x <= 5) bonus += 10;
                if (y == 4 || y == 5) bonus += 6;
                break;
            case Knight:
                if (x >= 3 && x <= 5) bonus += 8;
                if (y >= 2 && y <= 7) bonus += 5;
                break;
            case Bishop:
                if ((isRed && y <= 2) || (!isRed && y >= 7)) bonus += 5;
                break;
            case Assistant: {
                bool inPalace = false;
                if (isRed) {
                    inPalace = (x >= 3 && x <= 5 && y >= 0 && y <= 2);
                } else {
                    inPalace = (x >= 3 && x <= 5 && y >= 7 && y <= 9);
                }
                if (inPalace) bonus += 10;
                if (abs(x - 4) <= 1) bonus += 6;
                break;
            }
            case Pawn:
                if ((isRed && y > 4) || (!isRed && y < 5)) bonus += 25;
                if (x == 4) bonus += 8;
                bonus += (isRed ? y : 9 - y);
                break;
            default:
                break;
        }
        return bonus;
    }

    int evaluatePiece(const Chess& chess, int x, int y, stoneType type, colorType color) const {
        if (type == None) {
            return 0;
        }
        return baseValue(type) + positionValue(chess, x, y, type, color);
    }

    int evaluateBoard(const Chess& chess) const;
};

// ---------------------------------------------------------------------------
// Chess: 象棋棋盘状态与走法生成的核心类。
//       负责保存局面、生成合法走法、判断行棋是否合法、执行等操作。
// ---------------------------------------------------------------------------
class Chess {
private:
    std::array<Grid, BOARDWIDTH * BOARDHEIGHT> state;
    colorType currColor;
    vector<int> continuePeaceTurn;
    vector<int> lastMoveEaten;
    uint64_t zobristTable[BOARDWIDTH][BOARDHEIGHT][8][3];
    uint64_t currentHash;
    vector<uint64_t> hashHistory;
public:
    Chess();
    void resetBoard();
    void generateMoves(vector<Move> &legalMoves, bool mustDefend=true);
    void generateMovesWithForbidden(vector<Move> &legalMoves, bool mustDefend=true);
    bool repeatAfterMove(const Move& move);
    static bool inBoard(int mx, int my);
    static bool inKingArea(int mx, int my, colorType mcolor);
    static bool inColorArea(int mx, int my, colorType mcolor);
    colorType oppColor() const;
    colorType oppColor(colorType mcolor) const;
    bool isMoveValid(const Move& move, bool mustDefend=true);
    bool isLegalMove(const Move& move, bool mustDefend=true);
    bool isLegalMoveWithForbidden(const Move& move, bool mustDefend=true);
    bool isMoveValidWithForbidden(const Move& move, bool mustDefend=true);
    bool isCurrKingAttacked() const;
    bool makeMove(const Move& move, MoveState& moveState);
    void unmakeMove(const MoveState& moveState);
    bool makeMoveAssumeLegal(const Move& move);
    bool attacked(colorType color, int mx, int my) const;
    bool betweenIsEmpty(int sx, int sy, int ex, int ey);
    int betweenNotEmptyNum(int sx, int sy, int ex, int ey);
    static bool inSameLine(int sx, int sy, int ex, int ey);
    static bool inSameStraightLine(int sx, int sy, int ex, int ey);
    static bool inSameObiqueLine(int sx, int sy, int ex, int ey);
    bool isMyKingAttackedAfterMove(const Move& move);
    bool isOppKingAttackedAfterMove(const Move& move);
    bool currKingAttacked(const Move& move);
    bool winAfterMove(const Move& move);
    bool exceedMaxPeaceState();
    static int xy2pos(int mx, int my);
    static int pos2x(int mpos);
    static int pos2y(int mpos);
    Grid getGrid(int x, int y) const { return state[xy2pos(x, y)]; }
    colorType getCurrColor() const { return currColor; }
    void printBoard();
};

class Book {
public:
    std::unordered_map<std::string, std::vector<std::string>> nodes;
    Book() { }
    explicit Book(const std::string& path) { load(path); }
    // load book content from a single string (each line like in slim_book.txt)
    void loadFromString(const std::string& content) {
        nodes.clear();
        std::istringstream in(content);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            // trim possible carriage return
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto pos = line.find('|');
            if (pos == std::string::npos) continue;
            std::string left = line.substr(0, pos);
            std::string right = line.substr(pos + 1);
            // left possibly empty
            // right may contain a single move (like c3c4)
            // normalize: remove spaces
            auto normalize = [](std::string s) {
                std::string out;
                for (char c : s) if (c != ' ' && c != '\t') out.push_back(c);
                return out;
            };
            left = normalize(left);
            right = normalize(right);
            nodes[left].push_back(right);
        }
    }
    void load(const std::string& path) {
        nodes.clear();
        std::ifstream in(path);
        if (!in.is_open()) return;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            // trim possible carriage return
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto pos = line.find('|');
            if (pos == std::string::npos) continue;
            std::string left = line.substr(0, pos);
            std::string right = line.substr(pos + 1);
            // left possibly empty
            // right may contain a single move (like c3c4)
            // normalize: remove spaces
            auto normalize = [](std::string s) {
                std::string out;
                for (char c : s) if (c != ' ' && c != '\t') out.push_back(c);
                return out;
            };
            left = normalize(left);
            right = normalize(right);
            nodes[left].push_back(right);
        }
    }
    std::vector<std::string> getNext(const std::vector<std::string>& history) const {
        std::string key;
        for (size_t i = 0; i < history.size(); ++i) {
            if (i) key.push_back(',');
            key += history[i];
        }
        auto it = nodes.find(key);
        if (it == nodes.end()) return {};
        return it->second;
    }
};


class MoveSelector {
public:
    explicit MoveSelector(const Book& book, PieceValueEvaluator evaluator = PieceValueEvaluator())
        : book_(book), evaluator_(evaluator) {}

    Move selectMove(Chess& chess, const std::vector<std::string>& history) const {
        std::vector<Move> legalMoves;
        chess.generateMovesWithForbidden(legalMoves);
        if (legalMoves.empty()) {
            return Move();
        }

        std::vector<std::string> legalMoveStrs = encodeMoves(legalMoves);
        std::vector<std::string> bookCandidates = book_.getNext(history);
        std::vector<int> candidateIndexes = matchBookMoves(legalMoveStrs, bookCandidates);

        if (!candidateIndexes.empty()) {
            return chooseBookMove(legalMoves, candidateIndexes);
        }
        return selectBestMove(chess, legalMoves, evaluator_);
    }

private:
    static std::vector<std::string> encodeMoves(const std::vector<Move>& moves) {
        std::vector<std::string> result;
        result.reserve(moves.size());
        for (const auto &m : moves) {
            std::string s;
            s.push_back(pgnint2char(m.source_x));
            s.push_back(int2char(m.source_y));
            s.push_back(pgnint2char(m.target_x));
            s.push_back(int2char(m.target_y));
            result.push_back(s);
        }
        return result;
    }

    static std::vector<int> matchBookMoves(const std::vector<std::string>& legalMoveStrs,
                                          const std::vector<std::string>& bookCandidates) {
        std::vector<int> indexes;
        for (size_t i = 0; i < legalMoveStrs.size(); ++i) {
            for (const auto &candidate : bookCandidates) {
                if (legalMoveStrs[i] == candidate) {
                    indexes.push_back((int)i);
                    break;
                }
            }
        }
        return indexes;
    }

    static Move chooseBookMove(const std::vector<Move>& legalMoves, const std::vector<int>& candidateIndexes) {
        int chosenIndex = candidateIndexes[rand() % candidateIndexes.size()];
        return legalMoves[chosenIndex];
    }

    static int moveOrderScore(Chess& chess, const Move& move, const PieceValueEvaluator& evaluator) {
        Grid targetGrid = chess.getGrid(move.target_x, move.target_y);
        int score = 0;
        if (targetGrid.color == chess.oppColor()) {
            score += evaluator.evaluatePiece(chess, move.target_x, move.target_y,
                                            targetGrid.type, targetGrid.color);
        }
        if (chess.isOppKingAttackedAfterMove(move)) {
            score += 120;
        }
        return score;
    }

    static int evaluatePosition(Chess& chess, const PieceValueEvaluator& evaluator) {
        int score = evaluator.evaluateBoard(chess);
        return chess.getCurrColor() == RED ? score : -score;
    }

    static int negamax(Chess& chess, int depth, int alpha, int beta, const PieceValueEvaluator& evaluator) {
        if (depth <= 0) {
            return evaluatePosition(chess, evaluator);
        }

        std::vector<Move> legalMoves;
        chess.generateMovesWithForbidden(legalMoves);
        if (legalMoves.empty()) {
            if (chess.isCurrKingAttacked()) {
                return -1000000;
            }
            return 0;
        }

        int bestScore = -1000000000;
        std::vector<int> order(legalMoves.size());
        for (int i = 0; i < (int)order.size(); ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            return moveOrderScore(chess, legalMoves[a], evaluator) > moveOrderScore(chess, legalMoves[b], evaluator);
        });

        for (int index : order) {
            MoveState moveState;
            chess.makeMove(legalMoves[index], moveState);
            int score = -negamax(chess, depth - 1, -beta, -alpha, evaluator);
            chess.unmakeMove(moveState);
            if (score > bestScore) {
                bestScore = score;
            }
            alpha = std::max(alpha, score);
            if (alpha >= beta) {
                break;
            }
        }
        return bestScore;
    }

    static Move selectBestMove(Chess& chess, const std::vector<Move>& legalMoves,
                               const PieceValueEvaluator& evaluator) {
        int bestScore = -1000000000;
        int bestIndex = 0;
        std::vector<int> order(legalMoves.size());
        for (int i = 0; i < (int)legalMoves.size(); ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            return moveOrderScore(chess, legalMoves[a], evaluator) > moveOrderScore(chess, legalMoves[b], evaluator);
        });

        const int SEARCH_DEPTH = 3;
        for (int index : order) {
            MoveState moveState;
            chess.makeMove(legalMoves[index], moveState);
            int score = -negamax(chess, SEARCH_DEPTH - 1, -1000000000, 1000000000, evaluator);
            chess.unmakeMove(moveState);
            if (score > bestScore) {
                bestScore = score;
                bestIndex = index;
            }
        }
        return legalMoves[bestIndex];
    }

private:
    const Book& book_;
    PieceValueEvaluator evaluator_;
};

// ======================== LZW 压缩的开局库数据 ========================
//测试时先不用开局库，内容在kaijukuzancun.txt里，复制进去即可
const std::vector<uint16_t> COMPRESSED_BOOK = {
};
// ============================================================================

// ======================== LZW 解压函数 ========================
// 负责将 embedded book 的压缩数据解压为可加载的字符串格式。
std::string decompressLZW(const std::vector<uint16_t>& compressed) {
    if (compressed.empty()) return "";
    
    // 初始化字典：0-255 对应单字节字符
    std::unordered_map<uint16_t, std::string> dict;
    for (int i = 0; i < 256; ++i) {
        dict[i] = std::string(1, (char)i);
    }
    
    std::string result;
    uint16_t w_code = compressed[0];
    std::string w = dict[w_code];
    result += w;
    
    uint16_t dict_size = 256;
    
    for (size_t i = 1; i < compressed.size(); ++i) {
        uint16_t code = compressed[i];
        std::string entry;
        
        if (dict.find(code) != dict.end()) {
            entry = dict[code];
        } else if (code == dict_size) {
            // 特殊情况：code 还没被添加到字典
            entry = w + w[0];
        } else {
            // 不应该发生
            continue;
        }
        
        result += entry;
        
        // 添加新的字典条目（如果字典还没满）
        if (dict_size < 65536) {
            dict[dict_size] = w + entry[0];
            dict_size++;
        }
        
        w = entry;
    }
    
    return result;
}
// ================================================================

// ---------------------------------------------------------------------------
// BotZone I/O 接口声明
// getInputBotzone 负责从 BotZone JSON 输入恢复当前棋局状态。
// giveOutputBotzone 负责选择走法并输出最终 BotZone JSON 响应。
// ---------------------------------------------------------------------------
bool getInputBotzone(Chess& chess, std::vector<std::string>& history);
void giveOutputBotzone(Chess& chess, const std::vector<std::string>& history, Book& book);

// ---------------------------------------------------------------------------
// main: 程序入口，负责初始化、恢复局面并调用 BotZone 输出接口。
// ---------------------------------------------------------------------------
int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    // 初始化棋盘状态与开局库
    Chess chess;
    Book book;
    std::string slim_book_text = decompressLZW(COMPRESSED_BOOK);
    book.loadFromString(slim_book_text);

    // 从 BotZone 输入解析当前棋局历史并恢复状态
    std::vector<std::string> history;
    getInputBotzone(chess, history);

    // 选择落子并输出 JSON 格式结果
    giveOutputBotzone(chess, history, book);
    
    return 0;
}


// ---------------------------------------------------------------------------
// giveOutputBotzone: 通过 MoveSelector 选择最优落子并构造 BotZone 响应。
// ---------------------------------------------------------------------------
void giveOutputBotzone(Chess& chess, const std::vector<std::string>& history, Book& book) {
    Json::Value ret;
    MoveSelector selector(book);
    Move selMove = selector.selectMove(chess, history);

    if (selMove.source_x < 0 || selMove.target_x < 0) {
        ret["response"]["source"] = string("-1");
        ret["response"]["target"] = string("-1");
    } else {
        ret["response"]["source"] = string(1, pgnint2char(selMove.source_x)) + string(1, int2char(selMove.source_y));
        ret["response"]["target"] = string(1, pgnint2char(selMove.target_x)) + string(1, int2char(selMove.target_y));
    }

    Json::FastWriter writer;
    cout << writer.write(ret) << endl;
}

// ---------------------------------------------------------------------------
// getInputBotzone: 解析 BotZone 请求 JSON，将历史走法应用到棋盘上恢复当前局面。
// ---------------------------------------------------------------------------
bool getInputBotzone(Chess& chess, std::vector<std::string>& history) {
    string str;
    if (!std::getline(cin, str)) {
        std::cerr << "BotZone input error: failed to read line" << std::endl;
        return false;
    }

    Json::Reader reader;
    Json::Value input;
    if (!reader.parse(str, input)) {
        std::cerr << "BotZone input error: invalid JSON" << std::endl;
        return false;
    }

    if (!input.isMember("requests") || !input.isMember("responses") || !input["requests"].isArray() || !input["responses"].isArray()) {
        std::cerr << "BotZone input error: missing requests or responses" << std::endl;
        return false;
    }

    int turnID = static_cast<int>(input["responses"].size());
    if (static_cast<int>(input["requests"].size()) <= turnID) {
        std::cerr << "BotZone input error: requests size is too small" << std::endl;
        return false;
    }

    try {
        string curSource, curTarget;
        for (int i = 0; i < turnID; i++) {
            curSource = input["requests"][i]["source"].asString();
            curTarget = input["requests"][i]["target"].asString();
            if (curSource != "-1") {
                if (curSource.size() != 2 || curTarget.size() != 2) {
                    std::cerr << "BotZone input warning: invalid move format" << std::endl;
                    return false;
                }
                Move curMove(curSource, curTarget);
                if (!chess.isMoveValidWithForbidden(curMove)) {
                    std::cerr << "BotZone input warning: invalid request move" << std::endl;
                    return false;
                }
                chess.makeMoveAssumeLegal(curMove);
                history.push_back(curSource + curTarget);
            }

            curSource = input["responses"][i]["source"].asString();
            curTarget = input["responses"][i]["target"].asString();
            if (curSource.size() != 2 || curTarget.size() != 2) {
                std::cerr << "BotZone input warning: invalid response move format" << std::endl;
                return false;
            }
            Move curMove(curSource, curTarget);
            if (!chess.isMoveValidWithForbidden(curMove)) {
                std::cerr << "BotZone input warning: invalid response move" << std::endl;
                return false;
            }
            chess.makeMoveAssumeLegal(curMove);
            history.push_back(curSource + curTarget);
        }

        curSource = input["requests"][turnID]["source"].asString();
        curTarget = input["requests"][turnID]["target"].asString();
        if (curSource != "-1") {
            if (curSource.size() != 2 || curTarget.size() != 2) {
                std::cerr << "BotZone input warning: invalid move format" << std::endl;
                return false;
            }
            Move curMove(curSource, curTarget);
            if (!chess.isMoveValidWithForbidden(curMove)) {
                std::cerr << "BotZone input warning: invalid request move" << std::endl;
                return false;
            }
            chess.makeMoveAssumeLegal(curMove);
            history.push_back(curSource + curTarget);
        }
    } catch (const std::exception& ex) {
        std::cerr << "BotZone input exception: " << ex.what() << std::endl;
        return false;
    }

    return true;
}

void Chess::resetBoard() {
    Grid boardInfo[BOARDWIDTH][BOARDHEIGHT];
    for (int x = 0; x < BOARDWIDTH; x++) {
        for (int y = 0; y < BOARDHEIGHT; y++) {
            boardInfo[x][y] = Grid(None, EMPTY);
        }
    }

    boardInfo[0][0] = Grid(Rook, RED);
    boardInfo[1][0] = Grid(Knight, RED);
    boardInfo[2][0] = Grid(Bishop, RED);
    boardInfo[3][0] = Grid(Assistant, RED);
    boardInfo[4][0] = Grid(King, RED);
    boardInfo[5][0] = Grid(Assistant, RED);
    boardInfo[6][0] = Grid(Bishop, RED);
    boardInfo[7][0] = Grid(Knight, RED);
    boardInfo[8][0] = Grid(Rook, RED);
    boardInfo[1][2] = Grid(Cannon, RED);
    boardInfo[7][2] = Grid(Cannon, RED);
    boardInfo[0][3] = Grid(Pawn, RED);
    boardInfo[2][3] = Grid(Pawn, RED);
    boardInfo[4][3] = Grid(Pawn, RED);
    boardInfo[6][3] = Grid(Pawn, RED);
    boardInfo[8][3] = Grid(Pawn, RED);

    boardInfo[0][9] = Grid(Rook, BLACK);
    boardInfo[1][9] = Grid(Knight, BLACK);
    boardInfo[2][9] = Grid(Bishop, BLACK);
    boardInfo[3][9] = Grid(Assistant, BLACK);
    boardInfo[4][9] = Grid(King, BLACK);
    boardInfo[5][9] = Grid(Assistant, BLACK);
    boardInfo[6][9] = Grid(Bishop, BLACK);
    boardInfo[7][9] = Grid(Knight, BLACK);
    boardInfo[8][9] = Grid(Rook, BLACK);
    boardInfo[1][7] = Grid(Cannon, BLACK);
    boardInfo[7][7] = Grid(Cannon, BLACK);
    boardInfo[0][6] = Grid(Pawn, BLACK);
    boardInfo[2][6] = Grid(Pawn, BLACK);
    boardInfo[4][6] = Grid(Pawn, BLACK);
    boardInfo[6][6] = Grid(Pawn, BLACK);
    boardInfo[8][6] = Grid(Pawn, BLACK);

    for (int mpos = 0; mpos < BOARDWIDTH * BOARDHEIGHT; mpos++) {
        state[mpos] = boardInfo[pos2x(mpos)][pos2y(mpos)];
    }
    currentHash = 0;
    for (int x = 0; x < BOARDWIDTH; x++) {
        for (int y = 0; y < BOARDHEIGHT; y++) {
            Grid g = boardInfo[x][y];
            if (g.type != None) {
                currentHash ^= zobristTable[x][y][g.type][g.color];
            }
        }
    }

    continuePeaceTurn.clear();
    lastMoveEaten.clear();
    hashHistory.clear();
    continuePeaceTurn.push_back(0);
    lastMoveEaten.push_back(false);
    hashHistory.push_back(currentHash);
    currColor = RED;
}

Chess::Chess() {
    srand(time(NULL));
    for (int x = 0; x < BOARDWIDTH; x++) {
        for (int y = 0; y < BOARDHEIGHT; y++) {
            for (int t = 0; t < 8; t++) {
                for (int c = 0; c < 3; c++) {
                    zobristTable[x][y][t][c] = ((uint64_t)rand() << 32) | rand();
                }
            }
        }
    }
    resetBoard();
}

void Chess::generateMoves(vector<Move> &legalMoves, bool mustDefend) {
    legalMoves.clear();
    for (int x = 0; x < BOARDWIDTH; x++) {
        for (int y = 0; y < BOARDHEIGHT; y++) {
            auto& curGrid = state[xy2pos(x, y)];
            if (curGrid.color == currColor) {
                switch (curGrid.type) {
                    case King: {
                        for (int dir = 0; dir < 4; dir++) {
                            int tx = x + dx_strai[dir];
                            int ty = y + dy_strai[dir];
                            if (!inKingArea(tx, ty, curGrid.color)) {
                                continue;
                            }
                            auto& tGrid = state[xy2pos(tx, ty)];
                            if (tGrid.color == currColor)
                                continue;
                            if (mustDefend) {
                                if (!isMyKingAttackedAfterMove(Move(x,y,tx,ty))) {
                                    legalMoves.emplace_back(x, y, tx, ty);
                                }
                            } else {
                                legalMoves.emplace_back(x, y, tx, ty);
                            }
                        }
                        break;
                    }
                    case Assistant: {
                        for (int dir = 0; dir < 4; dir++) {
                            int tx = x + dx_ob[dir], ty = y + dy_ob[dir];
                            if (!inKingArea(tx, ty, curGrid.color)) {
                                continue;
                            }
                            auto& tGrid = state[xy2pos(tx, ty)];
                            if (tGrid.color == currColor) {
                                continue;
                            }
                            if (mustDefend) {
                                if (!isMyKingAttackedAfterMove(Move(x,y,tx,ty))) {
                                    legalMoves.emplace_back(x, y, tx, ty);
                                }
                            } else {
                                legalMoves.emplace_back(x, y, tx, ty);
                            }
                        }
                        break;
                    }
                    case Pawn: {
                        int forwardOne = (curGrid.color == RED ? 1 : -1);
                        bool crossMidLine = (curGrid.color == RED ? (y > 4) : (y < 5));
                        int tx = x, ty = y + forwardOne;
                        if (!inBoard(tx, ty)) {
                            continue;
                        }
                        auto& tGrid = state[xy2pos(tx, ty)];
                        if (tGrid.color == currColor) {
                            continue;
                        }
                        if (mustDefend) {
                            if (!isMyKingAttackedAfterMove(Move(x,y,tx,ty))) {
                                legalMoves.emplace_back(x, y, tx, ty);
                            }
                        } else {
                            legalMoves.emplace_back(x, y, tx, ty);
                        }
                        if (crossMidLine) {
                            for (int dir = 0; dir < 2; dir++) {
                                int ox = x + dx_lr[dir], oy = y;
                                if (!inBoard(ox, oy)) {
                                    continue;
                                }
                                auto& oGrid = state[xy2pos(ox, oy)];
                                if (oGrid.color == currColor) {
                                    continue;
                                }
                                if (mustDefend) {
                                    if (!isMyKingAttackedAfterMove(Move(x,y,ox,oy))) {
                                        legalMoves.emplace_back(x, y, ox, oy);
                                    }
                                } else {
                                    legalMoves.emplace_back(x, y, ox, oy);
                                }
                            }
                        }
                        break;
                    }
                    case Knight: {
                        for (int dir = 0; dir < 8; dir++) {
                            int tx = x + dx_knight[dir], ty = y + dy_knight[dir];
                            if (!inBoard(tx, ty)) {
                                continue;
                            }
                            auto& tGrid = state[xy2pos(tx, ty)];
                            int fx = x + dx_knight_foot[dir], fy = y + dy_knight_foot[dir];
                            auto& fGrid = state[xy2pos(fx, fy)];
                            if (tGrid.color == currColor || fGrid.color != EMPTY) {
                                continue;
                            }
                            if (mustDefend) {
                                if (!isMyKingAttackedAfterMove(Move(x,y,tx,ty))) {
                                    legalMoves.emplace_back(x, y, tx, ty);
                                }
                            } else {
                                legalMoves.emplace_back(x, y, tx, ty);
                            }
                        }
                        break;
                    }
                    case Bishop: {
                        for (int dir = 0; dir < 4; dir++) {
                            int tx = x + dx_bishop[dir], ty = y + dy_bishop[dir];
                            if (!inBoard(tx, ty) || !inColorArea(tx, ty, currColor)) {
                                continue;
                            }
                            int ex = x + dx_bishop_eye[dir], ey = y + dy_bishop_eye[dir];
                            auto& tGrid = state[xy2pos(tx, ty)];
                            auto& eGrid = state[xy2pos(ex, ey)];
                            if (tGrid.color == currColor || eGrid.color != EMPTY) {
                                continue;
                            }
                            if (mustDefend) {
                                if (!isMyKingAttackedAfterMove(Move(x,y,tx,ty))) {
                                    legalMoves.emplace_back(x, y, tx, ty);
                                }
                            } else {
                                legalMoves.emplace_back(x, y, tx, ty);
                            }
                        }
                        break;
                    }
                    case Rook: {
                        for (int dir = 0; dir < 4; dir++) {
                            int tx = x + dx_strai[dir];
                            int ty = y + dy_strai[dir];
                            while (inBoard(tx, ty)) {
                                auto& tGrid = state[xy2pos(tx, ty)];
                                if (tGrid.color == currColor) {
                                    break;
                                } else if (tGrid.color == oppColor()) {
                                    if (mustDefend) {
                                        if (!isMyKingAttackedAfterMove(Move(x,y,tx,ty))) {
                                            legalMoves.emplace_back(x, y, tx, ty);
                                        }
                                    } else {
                                        legalMoves.emplace_back(x, y, tx, ty);
                                    }
                                    break;
                                } else {
                                    assert(tGrid.color == EMPTY);
                                    if (mustDefend) {
                                        if (!isMyKingAttackedAfterMove(Move(x,y,tx,ty))) {
                                            legalMoves.emplace_back(x, y, tx, ty);
                                        }
                                    } else {
                                        legalMoves.emplace_back(x, y, tx, ty);
                                    }
                                }
                                tx += dx_strai[dir];
                                ty += dy_strai[dir];
                            }
                        }
                        break;
                    }
                    case Cannon: {
                        for (int dir = 0; dir < 4; dir++) {
                            int tx = x + dx_strai[dir];
                            int ty = y + dy_strai[dir];
                            while (inBoard(tx, ty)) {
                                int betweenNum = betweenNotEmptyNum(x, y, tx, ty);
                                if (betweenNum > 1) {
                                    break;
                                } else if (betweenNum == 1) {
                                    auto& tGrid = state[xy2pos(tx, ty)];
                                    if (tGrid.color == currColor || tGrid.color == EMPTY) {
                                        tx += dx_strai[dir];
                                        ty += dy_strai[dir];
                                        continue;
                                    }
                                    if (mustDefend) {
                                        if (!isMyKingAttackedAfterMove(Move(x,y,tx,ty))) {
                                            legalMoves.emplace_back(x, y, tx, ty);
                                        }
                                    } else {
                                        legalMoves.emplace_back(x, y, tx, ty);
                                    }
                                } else if (betweenNum == 0) {
                                    auto& tGrid = state[xy2pos(tx, ty)];
                                    if (tGrid.color != EMPTY) {
                                        tx += dx_strai[dir];
                                        ty += dy_strai[dir];
                                        continue;
                                    }
                                    if (mustDefend) {
                                        if (!isMyKingAttackedAfterMove(Move(x,y,tx,ty))) {
                                            legalMoves.emplace_back(x, y, tx, ty);
                                        }
                                    } else {
                                        legalMoves.emplace_back(x, y, tx, ty);
                                    }
                                }
                                tx += dx_strai[dir];
                                ty += dy_strai[dir];
                            }
                        }
                        break;
                    }
                    case None:
                        throw std::runtime_error("color is not empty but type is none");
                }
            }
        }
    }
}

bool Chess::inBoard(int mx, int my) {
    return mx >= 0 && mx < BOARDWIDTH && my >= 0 && my < BOARDHEIGHT;
}

colorType Chess::oppColor() const {
    return currColor == BLACK ? RED : BLACK;
}

bool Chess::isLegalMove(const Move &move, bool mustDefend) {
    vector<Move> currLegalMoves;
    generateMoves(currLegalMoves, mustDefend);
    auto fi_iter = std::find(currLegalMoves.begin(), currLegalMoves.end(), move);
    return fi_iter != currLegalMoves.end();
}

bool Chess::makeMove(const Move &move, MoveState &moveState) {
    int sourcePos = xy2pos(move.source_x, move.source_y);
    int targetPos = xy2pos(move.target_x, move.target_y);
    moveState.sourcePos = sourcePos;
    moveState.targetPos = targetPos;
    moveState.sourceGrid = state[sourcePos];
    moveState.targetGrid = state[targetPos];
    moveState.previousHash = currentHash;

    int nextPeaceTrun = continuePeaceTurn.back() + 1;
    currentHash ^= zobristTable[move.source_x][move.source_y][moveState.sourceGrid.type][moveState.sourceGrid.color];

    if (moveState.targetGrid.type != None) {
        currentHash ^= zobristTable[move.target_x][move.target_y][moveState.targetGrid.type][moveState.targetGrid.color];
        nextPeaceTrun = 0;
        lastMoveEaten.push_back(true);
    } else {
        lastMoveEaten.push_back(false);
    }

    state[targetPos] = moveState.sourceGrid;
    currentHash ^= zobristTable[move.target_x][move.target_y][moveState.sourceGrid.type][moveState.sourceGrid.color];
    state[sourcePos] = Grid(None, EMPTY);

    continuePeaceTurn.push_back(nextPeaceTrun);
    currColor = oppColor();
    hashHistory.push_back(currentHash);
    return true;
}

void Chess::unmakeMove(const MoveState &moveState) {
    state[moveState.sourcePos] = moveState.sourceGrid;
    state[moveState.targetPos] = moveState.targetGrid;
    currentHash = moveState.previousHash;
    continuePeaceTurn.pop_back();
    lastMoveEaten.pop_back();
    hashHistory.pop_back();
    currColor = oppColor();
}

bool Chess::makeMoveAssumeLegal(const Move &move) {
    MoveState moveState;
    return makeMove(move, moveState);
}

bool Chess::attacked(colorType color, int mx, int my) const {
    auto gridAt = [&](int x, int y) -> const Grid& {
        return state[xy2pos(x, y)];
    };

    // Pawn attack source positions
    int forwardOne = (color == RED ? 1 : -1);
    int sourceX = mx;
    int sourceY = my - forwardOne;
    if (inBoard(sourceX, sourceY)) {
        const Grid& sourceGrid = gridAt(sourceX, sourceY);
        if (sourceGrid.color == color && sourceGrid.type == Pawn) {
            return true;
        }
    }
    bool crossPossible = (color == RED ? my > 4 : my < 5);
    if (crossPossible) {
        for (int dir = 0; dir < 2; dir++) {
            int sx = mx + dx_lr[dir];
            int sy = my;
            if (!inBoard(sx, sy)) {
                continue;
            }
            const Grid& sourceGrid = gridAt(sx, sy);
            if (sourceGrid.color == color && sourceGrid.type == Pawn) {
                return true;
            }
        }
    }

    // Straight-line attacks: king, rook, cannon
    for (int dir = 0; dir < 4; dir++) {
        int dx = dx_strai[dir];
        int dy = dy_strai[dir];
        int x = mx + dx;
        int y = my + dy;
        int screenCount = 0;
        while (inBoard(x, y)) {
            const Grid& g = gridAt(x, y);
            if (g.color != EMPTY) {
                if (g.color == color) {
                    if (screenCount == 0) {
                        if (g.type == Rook || g.type == King) {
                            return true;
                        }
                    } else if (screenCount == 1 && g.type == Cannon) {
                        return true;
                    }
                }
                screenCount++;
                if (screenCount > 1) {
                    break;
                }
            }
            x += dx;
            y += dy;
        }
    }

    // Knight attacks
    for (int i = 0; i < 8; i++) {
        int sx = mx - dx_knight[i];
        int sy = my - dy_knight[i];
        if (!inBoard(sx, sy)) {
            continue;
        }
        int fx = sx + dx_knight_foot[i];
        int fy = sy + dy_knight_foot[i];
        if (!inBoard(fx, fy)) {
            continue;
        }
        if (gridAt(fx, fy).color != EMPTY) {
            continue;
        }
        const Grid& sourceGrid = gridAt(sx, sy);
        if (sourceGrid.color == color && sourceGrid.type == Knight) {
            return true;
        }
    }

    // Bishop attacks
    for (int i = 0; i < 4; i++) {
        int sx = mx - dx_bishop[i];
        int sy = my - dy_bishop[i];
        if (!inBoard(sx, sy)) {
            continue;
        }
        int ex = sx + dx_bishop_eye[i];
        int ey = sy + dy_bishop_eye[i];
        if (!inBoard(ex, ey)) {
            continue;
        }
        if (gridAt(ex, ey).color != EMPTY) {
            continue;
        }
        const Grid& sourceGrid = gridAt(sx, sy);
        if (sourceGrid.color == color && sourceGrid.type == Bishop) {
            return true;
        }
    }

    // Advisor attacks
    if (inKingArea(mx, my, oppColor(color))) {
        for (int i = 0; i < 4; i++) {
            int sx = mx + dx_ob[i];
            int sy = my + dy_ob[i];
            if (!inBoard(sx, sy)) {
                continue;
            }
            const Grid& sourceGrid = gridAt(sx, sy);
            if (sourceGrid.color == color && sourceGrid.type == Assistant) {
                return true;
            }
        }
    }

    return false;
}

bool Chess::betweenIsEmpty(int sx, int sy, int ex, int ey) {
    assert(inSameStraightLine(sx, sy, ex, ey));
    int dex = ex > sx ? 1 : (ex == sx ? 0 : -1);
    int dey = ey > sy ? 1 : (ey == sy ? 0 : -1);
    int tx = sx + dex, ty = sy + dey;
    while (inBoard(tx, ty) && !(tx == ex && ty == ey)) {
        auto& tGrid = state[xy2pos(tx, ty)];
        if (tGrid.color != EMPTY)
            return false;
        tx += dex; ty += dey;
    }
    return true;
}

bool Chess::inSameLine(int sx, int sy, int ex, int ey) {
    int dex = ex - sx, dey = ey - sy;
    return dex == dey || dex == -dey || dex == 0 || dey == 0;
}

bool Chess::inSameStraightLine(int sx, int sy, int ex, int ey) {
    int dex = ex - sx, dey = ey - sy;
    return dex == 0 || dey == 0;
}

bool Chess::inSameObiqueLine(int sx, int sy, int ex, int ey) {
    int dex = ex - sx, dey = ey - sy;
    return dex == dey || dex == -dey;
}

bool Chess::isMyKingAttackedAfterMove(const Move &move) {
    colorType originColor = currColor;
    MoveState moveState;
    makeMove(move, moveState);

    int kingX = -1, kingY = -1;
    for (int x = 0; x < BOARDWIDTH; x++) {
        for (int y = 0; y < BOARDHEIGHT; y++) {
            auto& curGrid = state[xy2pos(x, y)];
            if (curGrid.color == originColor && curGrid.type == King) {
                kingX = x; kingY = y;
                break;
            }
        }
    }
    bool myKingStillAttacked = attacked(oppColor(originColor), kingX, kingY);
    unmakeMove(moveState);
    return myKingStillAttacked;
}

bool Chess::isMoveValid(const Move& move, bool mustDefend) {
    return isLegalMove(move, mustDefend);
}

bool Chess::winAfterMove(const Move &move) {
    MoveState moveState;
    makeMove(move, moveState);
    vector<Move> oppLegalMoves;
    generateMoves(oppLegalMoves);
    bool win = oppLegalMoves.empty();
    unmakeMove(moveState);
    return win;
}

bool Chess::currKingAttacked(const Move &move) {
    int kingX = -1, kingY = -1;
    for (int x = 0; x < BOARDWIDTH; x++) {
        for (int y = 0; y < BOARDHEIGHT; y++) {
            auto& curGrid = state[xy2pos(x, y)];
            if (curGrid.color == currColor && curGrid.type == King) {
                kingX = x; kingY = y;
            }
        }
    }
    return attacked(oppColor(), kingX, kingY);
}

bool Chess::isOppKingAttackedAfterMove(const Move &move) {
    colorType originColor = currColor;
    MoveState moveState;
    makeMove(move, moveState);

    int kingX = -1, kingY = -1;
    for (int x = 0; x < BOARDWIDTH; x++) {
        for (int y = 0; y < BOARDHEIGHT; y++) {
            auto& curGrid = state[xy2pos(x, y)];
            if (curGrid.color == oppColor(originColor) && curGrid.type == King) {
                kingX = x; kingY = y;
                break;
            }
        }
    }
    bool oppKingBeingAttacked = attacked(originColor, kingX, kingY);
    unmakeMove(moveState);
    return oppKingBeingAttacked;
}

bool Chess::exceedMaxPeaceState() {
    return continuePeaceTurn.back() >= 60 - 1;
}

int Chess::xy2pos(int mx, int my) {
    return my * BOARDWIDTH + mx;
}

int Chess::pos2x(int mpos) {
    return mpos % BOARDWIDTH;
}

int Chess::pos2y(int mpos) {
    return mpos / BOARDWIDTH;
}

bool Chess::inKingArea(int mx, int my, colorType mcolor) {
    assert(mcolor == RED || mcolor == BLACK);
    if (mcolor == RED) {
        return mx >= 3 && mx <= 5 && my >= 0 && my <= 2;
    } else {
        return mx >= 3 && mx <= 5 && my >= 7 && my <= 9;
    }
}

bool Chess::inColorArea(int mx, int my, colorType mcolor) {
    assert(mcolor == RED || mcolor == BLACK);
    if (mcolor == RED) {
        return my <= 4;
    } else {
        return my >= 5;
    }
}

int Chess::betweenNotEmptyNum(int sx, int sy, int ex, int ey) {
    assert(inSameStraightLine(sx, sy, ex, ey));
    int dex = ex > sx ? 1 : (ex == sx ? 0 : -1);
    int dey = ey > sy ? 1 : (ey == sy ? 0 : -1);
    int tx = sx + dex, ty = sy + dey;
    int retNum = 0;
    while (inBoard(tx, ty) && !(tx == ex && ty == ey)) {
        auto& tGrid = state[xy2pos(tx, ty)];
        if (tGrid.color != EMPTY) {
            retNum++;
        }
        tx += dex; ty += dey;
    }
    return retNum;
}

colorType Chess::oppColor(colorType mcolor) const {
    return mcolor == RED ? BLACK : RED;
}

int PieceValueEvaluator::evaluateBoard(const Chess& chess) const {
    int total = 0;
    for (int x = 0; x < BOARDWIDTH; x++) {
        for (int y = 0; y < BOARDHEIGHT; y++) {
            Grid g = chess.getGrid(x, y);
            if (g.type == None) {
                continue;
            }
            int value = evaluatePiece(chess, x, y, g.type, g.color);
            total += (g.color == RED ? value : -value);
        }
    }
    return total;
}

bool Chess::isCurrKingAttacked() const {
    int kingX = -1, kingY = -1;
    for (int x = 0; x < BOARDWIDTH; x++) {
        for (int y = 0; y < BOARDHEIGHT; y++) {
            auto& curGrid = state[xy2pos(x, y)];
            if (curGrid.color == currColor && curGrid.type == King) {
                kingX = x;
                kingY = y;
                break;
            }
        }
        if (kingX >= 0) break;
    }
    return attacked(oppColor(), kingX, kingY);
}


void Chess::generateMovesWithForbidden(vector<Move> &legalMoves, bool mustDefend) {
    vector<Move> firstMoves;
    generateMoves(firstMoves, mustDefend);
    legalMoves.clear();
    for (const auto& move : firstMoves) {
        if (!repeatAfterMove(move)) {
            legalMoves.push_back(move);
        }
    }
}

bool Chess::repeatAfterMove(const Move& move) {
    MoveState moveState;
    makeMove(move, moveState);

    if (lastMoveEaten.back()) {
        unmakeMove(moveState);
        return false;
    }

    int tmp_index = hashHistory.size() - 1;
    while (tmp_index > 1 && !lastMoveEaten[tmp_index - 2]) {
        tmp_index -= 2;
    }

    int repeatTime = 0;
    for (int i = hashHistory.size() - 1; i >= tmp_index; i -= 2) {
        if (hashHistory[i] == currentHash) {
            // To handle hash collision risk, we can optionally compare the actual board state
            // For now, we rely on the low collision probability of uint64_t
            repeatTime++;
        }
    }

    unmakeMove(moveState);
    return repeatTime >= 3;
}

bool Chess::isLegalMoveWithForbidden(const Move& move, bool mustDefend) {
    vector<Move> currLegalMoves;
    generateMovesWithForbidden(currLegalMoves, mustDefend);
    auto fi_iter = std::find(currLegalMoves.begin(), currLegalMoves.end(), move);
    return fi_iter != currLegalMoves.end();
}

void Chess::printBoard() {
    std::cout << std::endl << "Board" << std::endl;
    for (int y = 9; y >= 0; y--) {
        for (int x = 0; x <= 8; x++) {
            std::cout << stoneSym[state[xy2pos(x,y)].type] << " ";
        }
        std::cout << std::endl;
    }
}

bool Chess::isMoveValidWithForbidden(const Move& move, bool mustDefend) {
    return isLegalMoveWithForbidden(move, mustDefend);
}