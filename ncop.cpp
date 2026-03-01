/* Solver for n-cop Shannon Switching game on complete graphs.

Cop and robber alternate choosing edges to add to their respective graphs.
In the generalized n-cop version, the cop can add up to n edges per turn.
The robber wins if it can create a path between two designated vertices (0 and 1)
using only edges in its graph. The cop can block edges by adding them to its own graph.
If the cop can prevent the robber from connecting vertices 0 and 1, the cop wins.
*/
#include <iostream>
#include <cstdint>
#include <cassert>
#include <vector>
#define TOP_LEFT 0x8000000000000000ULL

#include <immintrin.h>

using namespace std;

typedef __m256i Bitboard;

Bitboard get_cop_starting_bitboard_for_size_k_graph(int k) {
    uint16_t data[16] = {0};
    for (int i = 0; i < k; i++) {
        data[i] = (1 << k) - 1;
    }
    return _mm256_xor_si256(_mm256_loadu_si256((__m256i const *) data), _mm256_set1_epi32(-1));
}

Bitboard add_edge(Bitboard graph, int u, int v) {
    uint16_t data[16] = {0};
    data[u] |= 1 << v;
    data[v] |= 1 << u;
    return _mm256_or_si256(_mm256_loadu_si256((__m256i const *) data), graph);
}

Bitboard remove_edge(Bitboard graph, int u, int v) {
    uint16_t data[16] = {0};
    data[u] |= 1 << v;
    data[v] |= 1 << u;
    return _mm256_andnot_si256(_mm256_loadu_si256((__m256i const *) data), graph);
}

void print_graph(Bitboard graph) {
    uint16_t data[16];
    _mm256_storeu_si256((__m256i *) data, graph);
    for (int i = 15; i >= 0; i--) {
        for (int j = 0; j < 16; j++) {
            if (data[i] & (1ULL << j)) {
                cout << "1 ";
            } else {
                cout << "0 ";
            }
        }
        cout << endl;
    }
    cout << endl;
}

bool has_edge(Bitboard graph, int u, int v) {
    uint16_t data[16];
    _mm256_storeu_si256((__m256i *) data, graph);
    return data[u] & (1ULL << v);
}

bool is_0_1_connected(Bitboard graph) {
    uint16_t frontier = _mm256_extract_epi16(graph, 0);
    uint16_t last = 0;

    // fill out columns to frontier
    __m256i matches = _mm256_set1_epi16(frontier);
    while (last != frontier && !(frontier & 0x2)) {
        // find rows that intersect with frontier
        matches = _mm256_and_si256(matches, graph);
        matches = _mm256_cmpeq_epi16(matches, _mm256_setzero_si256());
        matches = _mm256_xor_si256(matches, _mm256_set1_epi32(-1));

        // get their contents
        matches = _mm256_and_si256(matches, graph);
        if (_mm256_extract_epi16(matches, 1)) return true; // did we see the one row?

        // merge into new columns
        matches = _mm256_or_si256(_mm256_alignr_epi8(matches, matches, 2), matches);
        matches = _mm256_or_si256(_mm256_alignr_epi8(matches, matches, 4), matches);
        matches = _mm256_or_si256(_mm256_alignr_epi8(matches, matches, 8), matches);
        matches = _mm256_or_si256(_mm256_alignr_epi8(matches, matches, 16), matches);

        // forward propagate...
        last = frontier;
        frontier = _mm256_extract_epi16(matches, 0);
    }
    return frontier & 0x2;
}

struct GameState {
    int graph_size;
    int num_cops;
    Bitboard cop;
    Bitboard robber;

    GameState(const int gs) {
        graph_size = gs;
        cop = get_cop_starting_bitboard_for_size_k_graph(graph_size);
        robber = _mm256_setzero_si256();
    }
};

bool did_robber_win(const GameState& graph) {
    return is_0_1_connected(graph.robber);
}

bool did_cop_win(const GameState& graph) {
    return !is_0_1_connected(~graph.cop);
}

bool is_move_legal(GameState graph, int u, int v) {
    return !has_edge(graph.cop, u, v) && !has_edge(graph.robber, u, v) && u != v;
}

// Forward declare
int robbers_turn_evaluate(const GameState& graph, const int num_cops, const int depth);

int cops_turn_evaluate(const GameState& graph, const int num_cops, const int depth = 0) {
    if(did_cop_win(graph)) return 1;
    if(did_robber_win(graph)) return -1;

    bool no_move_found = true;

    if(no_move_found && num_cops >= 3) {
        for (int u = 0; u < graph.graph_size; u++) {
            for (int v = u + 1; v < graph.graph_size; v++) {
                if (is_move_legal(graph, u, v)) {
                    for (int m = u; m < graph.graph_size; m++) {
                        int n_start = ((m == u) ? v : m) + 1;
                        for (int n = n_start; n < graph.graph_size; n++) {
                            if (is_move_legal(graph, m, n)) {
                    for (int a = m; a < graph.graph_size; a++) {
                        int b_start = ((a == m) ? n : a) + 1;
                        for (int b = b_start; b < graph.graph_size; b++) {
                            if (is_move_legal(graph, a, b)) {
                                no_move_found = false;
                                GameState new_graph = graph;
                                new_graph.cop = add_edge(new_graph.cop, u, v);
                                new_graph.cop = add_edge(new_graph.cop, m, n);
                                new_graph.cop = add_edge(new_graph.cop, a, b);
                                if (depth == 0) {
                                    cout << "Cop removes edges (" << u << ", " << v << "), (" << m << ", " << n << ") and (" << a << ", " << b << ")";
                                }
                                int eval = robbers_turn_evaluate(new_graph, num_cops, depth + 1);
                                if (depth == 0 && eval == 1) cout << ", and wins!" << endl;
                                if (eval == 1) return 1;
                            }
                        }
                    }
                            }
                        }
                    }
                }
            }
        }
    }

    if(no_move_found && num_cops >= 2) {
        for (int u = 0; u < graph.graph_size; u++) {
            for (int v = u + 1; v < graph.graph_size; v++) {
                if (is_move_legal(graph, u, v)) {
                    for (int m = u; m < graph.graph_size; m++) {
                        int n_start = ((m == u) ? v : m) + 1;
                        for (int n = n_start; n < graph.graph_size; n++) {
                            if (is_move_legal(graph, m, n)) {
                                no_move_found = false;
                                GameState new_graph = graph;
                                new_graph.cop = add_edge(new_graph.cop, u, v);
                                new_graph.cop = add_edge(new_graph.cop, m, n);
                                if (depth == 0) {
                                    cout << "Cop removes edges (" << u << ", " << v << ") and (" << m << ", " << n << ")";
                                }
                                int eval = robbers_turn_evaluate(new_graph, num_cops, depth + 1);
                                if (depth == 0 && eval == 1) cout << ", and wins!" << endl;
                                if (eval == 1) return 1;
                            }
                        }
                    }
                }
            }
        }
    }

    if(no_move_found) {
        for (int u = 0; u < graph.graph_size; u++) {
            for (int v = u + 1; v < graph.graph_size; v++) {
                if (is_move_legal(graph, u, v)) {
                    GameState new_graph = graph;
                    new_graph.cop = add_edge(new_graph.cop, u, v);
                    if (depth == 0) {
                        cout << "Cop removes edge (" << u << ", " << v << ")";
                    }
                    int eval = robbers_turn_evaluate(new_graph, num_cops, depth + 1);
                    if (depth == 0 && eval == 1) cout << ", and wins!" << endl;
                    if (eval == 1) return 1;
                }
            }
        }
    }
    return -1;
}

int robbers_turn_evaluate(const GameState& graph, const int num_cops, const int depth) {
    if(did_cop_win(graph)) return 1;
    if(did_robber_win(graph)) return -1;
    for (int u = 0; u < graph.graph_size; u++) {
        for (int v = u + 1; v < graph.graph_size; v++) {
            if (is_move_legal(graph, u, v)) {
                GameState new_graph = graph;
                new_graph.robber = add_edge(new_graph.robber, u, v);
                if(cops_turn_evaluate(new_graph, num_cops, depth + 1) == -1) {
                    if (depth == 1) {
                        cout << ", but Robber can win by adding edge (" << u << ", " << v << ")" << endl;
                    }
                    return -1;
                }
            }
        }
    }
    return 1;
}

void unit_tests() {
    // Silence output during unit tests
    streambuf* orig_buf = cout.rdbuf();
    cout.rdbuf(nullptr);

    {
        Bitboard cop_graph = get_cop_starting_bitboard_for_size_k_graph(4);
        Bitboard robber_graph = _mm256_setzero_si256();

        // Test adding edges
        cop_graph = add_edge(cop_graph, 0, 2);
        robber_graph = add_edge(robber_graph, 1, 3);

        // Test edge existence
        assert(has_edge(cop_graph, 0, 2));
        assert(!has_edge(cop_graph, 1, 3));
        assert(has_edge(robber_graph, 1, 3));
        assert(!has_edge(robber_graph, 0, 2));
        assert(!has_edge(cop_graph, 0, 1));
        assert(!has_edge(robber_graph, 0, 1));

        // Test removing edges
        cop_graph = remove_edge(cop_graph, 0, 2);
        assert(!has_edge(cop_graph, 0, 2));

        // Test connectivity
        assert(!is_0_1_connected(robber_graph));
        robber_graph = add_edge(robber_graph, 2, 3);
        assert(!is_0_1_connected(robber_graph));
        robber_graph = add_edge(robber_graph, 2, 0);
        assert(is_0_1_connected(robber_graph));
    }

    {
        GameState state(6);
        state.cop = add_edge(state.cop, 0, 1);
        state.cop = add_edge(state.cop, 0, 2);
        state.robber = add_edge(state.robber, 4, 5);

        state.cop = add_edge(state.cop, 0, 3);
        state.cop = add_edge(state.cop, 0, 4);
        state.robber = add_edge(state.robber, 0, 5);

        state.cop = add_edge(state.cop, 1, 4);
        state.cop = add_edge(state.cop, 1, 5);
        state.robber = add_edge(state.robber, 1, 2);

        assert(cops_turn_evaluate(state, 2, 0) == 1);
    }

    {
        // Test that the cop wins in a 4-vertex clique with 1 cop
        GameState state(4);
        assert(cops_turn_evaluate(state, 1, 0) == -1);

        state.cop = add_edge(state.cop, 0, 1);
        state.robber = add_edge(state.robber, 0, 2);
        assert(cops_turn_evaluate(state, 1, 0) == -1);
    }

    {
        Bitboard robber = add_edge(_mm256_setzero_si256(), 0, 5);
        assert(!is_0_1_connected(robber));
        robber = add_edge(robber, 1, 4);
        assert(!is_0_1_connected(robber));
        robber = add_edge(robber, 2, 3);
        assert(!is_0_1_connected(robber));
        robber = add_edge(robber, 5, 2);
        assert(!is_0_1_connected(robber));
        robber = add_edge(robber, 5, 3);
        assert(!is_0_1_connected(robber));
        robber = add_edge(robber, 4, 2);
        assert(is_0_1_connected(robber));
    }

    {
        Bitboard robber = add_edge(_mm256_setzero_si256(), 0, 7);
        assert(!is_0_1_connected(robber));
        robber = add_edge(robber, 1, 6);
        assert(!is_0_1_connected(robber));
        robber = add_edge(robber, 2, 5);
        assert(!is_0_1_connected(robber));
        robber = add_edge(robber, 3, 4);
        assert(!is_0_1_connected(robber));
        robber = add_edge(robber, 7, 3);
        assert(!is_0_1_connected(robber));
        robber = add_edge(robber, 3, 5);
        assert(!is_0_1_connected(robber));
        robber = add_edge(robber, 5, 1);
        assert(is_0_1_connected(robber));
    }

    cout.rdbuf(orig_buf);

    cout << "All unit tests passed!" << endl << endl;
}

int run_game_test(const int graph_size, const int num_cops) {
    GameState initial_state(graph_size);
    int result = cops_turn_evaluate(initial_state, num_cops);
    if (result == 1) {
        cout << "Cop wins on graph size " << graph_size << endl;
    } else {
        cout << "Robber wins on graph size " << graph_size << endl;
    }
    return result;
}

int play_as_cop_against_computer(const int graph_size, const int num_cops) {
    GameState state(graph_size);
    while (true) {
        // Cop's turn
        for(int c = 0; c < num_cops; c++) {
            int u, v;
            cout << "You have " << num_cops - c << " deletions left this turn. Enter an edge to remove (u v): ";
            cin >> u >> v;
            if (!is_move_legal(state, u, v)) {
                cout << "Illegal move. Try again." << endl;
                c--;
                continue;
            }
            state.cop = add_edge(state.cop, u, v);
            if(did_cop_win(state)) {
                cout << "Congratulations, you win!" << endl;
                return 1;
            }
        }

        // Robber's turn
        bool move_made = false;
        vector<pair<int, int>> legal_moves;
        for (int u = 0; u < state.graph_size; u++) {
            for (int v = u + 1; v < state.graph_size; v++) {
                if (is_move_legal(state, u, v)) {
                    legal_moves.emplace_back(u, v);
                    GameState new_state = state;
                    new_state.robber = add_edge(new_state.robber, u, v);
                    if(cops_turn_evaluate(new_state, num_cops, 1) == -1) {
                        cout << "Robber adds edge (" << u << ", " << v << ")" << endl;
                        state.robber = add_edge(state.robber, u, v);
                        move_made = true;
                        goto after_robber_move;
                    }
                }
            }
        }
after_robber_move:
        if(!move_made) {
            // Pick a random legal move if no winning move found
            assert(!legal_moves.empty()); // if so, cop should have already won
            pair<int, int> move = legal_moves[rand() % legal_moves.size()];
            cout << "Robber adds edge (" << move.first << ", " << move.second << ")" << endl;
            state.robber = add_edge(state.robber, move.first, move.second);
        }
        if(did_robber_win(state)) {
            cout << "Sorry, the robber wins!" << endl;
            return -1;
        }
    }
}

int main_play_mode(int argc, char* argv[]) {
    int num_cops;
    if (argv[1][0] == '1') {
        num_cops = 1;
    } else if (argv[1][0] == '2') {
        num_cops = 2;
    } else if (argv[1][0] == '3') {
        num_cops = 3;
    } else {
        cout << "Invalid number of cops. Must be 1, 2, or 3." << endl;
        return 1;
    }

    int graph_size = atoi(argv[2]);
    if (graph_size < 2 || graph_size > 16) {
        cout << "Invalid graph size. Must be between 2 and 16 inclusive." << endl;
        return 1;
    }

    string role = argv[3];
    if (role != "cop" && role != "robber") {
        cout << "Invalid role. Must be 'cop' or 'robber'." << endl;
        return 1;
    }

    if (role == "cop") {
        play_as_cop_against_computer(graph_size, num_cops);
    } else {
        cout << "Playing as robber is not implemented yet." << endl;
    }
    return 0;
}

int main_eval_mode(int argc, char* argv[]) {
    int num_cops;
    if (argv[1][0] == '1') {
        num_cops = 1;
    } else if (argv[1][0] == '2') {
        num_cops = 2;
    } else if (argv[1][0] == '3') {
        num_cops = 3;
    } else {
        cout << "Invalid number of cops. Must be 1, 2, or 3." << endl;
        return 1;
    }

    unit_tests();
    int i = 1;
    while (true) {
        int result = run_game_test(i, num_cops);
        if (result == -1) break;
        if(i == 8) {
            break;
        }
        i++;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 4) {
        cout << "To evaluate for n cops   : " << argv[0] << " <num_cops>" << endl;
        cout << "To play as cop or robber : " << argv[0] << " <num_cops> <graph_size> <cop/robber>" << endl;
        return 1;
    } else if (argc == 4) {
        main_play_mode(argc, argv);
    } else {
        main_eval_mode(argc, argv);
    }
}
