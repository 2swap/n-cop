/* Solver for n-cop Shannon Switching game on complete graphs.

Cop and robber alternate choosing edges to add to their respective graphs.
In the generalized n-cop version, the cop can add up to n edges per turn.
The robber wins if it can create a path between two designated vertices (0 and 1)
using only edges in its graph. The cop can block edges by adding them to its own graph.
*/
#include <iostream>
#include <cstdint>
#include <cassert>
#define TOP_LEFT 0x8000000000000000ULL

using namespace std;

typedef uint64_t Bitboard;

Bitboard get_cop_starting_bitboard_for_size_k_graph(int k) {
    Bitboard v = 0ULL;
    Bitboard h = 0ULL;
    for(int i = k; i < 8; i++) {
        v |= 1ULL << i;
        h |= 1ULL << (i * 8);
    }
    v |= v << 8;
    v |= v << 16;
    v |= v << 32;
    h |= h << 1;
    h |= h << 2;
    h |= h << 4;
    return v | h;
}

Bitboard add_edge(Bitboard graph, int u, int v) {
    return graph | (1ULL << (u * 8 + v)) | (1ULL << (v * 8 + u));
}

Bitboard remove_edge(Bitboard graph, int u, int v) {
    return graph & ~(1ULL << (u * 8 + v)) & ~(1ULL << (v * 8 + u));
}

void print_graph(Bitboard graph) {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (graph & (1ULL << (i * 8 + j))) {
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
    return graph & (1ULL << (u * 8 + v));
}

bool is_0_1_connected(Bitboard graph) {
    Bitboard start = 0x00000000000000ff; // Start from vertex 0
    Bitboard v_frontier = graph & start;
    while (true) {
        Bitboard s_frontier = v_frontier;
        s_frontier |= s_frontier >> 8;
        s_frontier |= s_frontier << 8;
        s_frontier |= s_frontier >> 16;
        s_frontier |= s_frontier << 16;
        s_frontier |= s_frontier >> 32;
        s_frontier |= s_frontier << 32;
        Bitboard h_frontier = s_frontier & graph;
        h_frontier |= h_frontier >> 1;
        h_frontier |= h_frontier << 1;
        h_frontier |= h_frontier >> 2;
        h_frontier |= h_frontier << 2;
        h_frontier |= h_frontier >> 4;
        h_frontier |= h_frontier << 4;
        Bitboard new_v_frontier = h_frontier & graph;
        if(new_v_frontier == v_frontier) break;
        v_frontier = new_v_frontier;
    }
    return v_frontier & 0x000000000000ff00; // Check if vertex 1 is reached
}

struct GameState {
    int graph_size;
    int num_cops;
    Bitboard cop;
    Bitboard robber;

    GameState(const int gs) {
        graph_size = gs;
        cop = get_cop_starting_bitboard_for_size_k_graph(graph_size);
        robber = 0ULL;
    }
};

bool did_robber_win(GameState graph) {
    return is_0_1_connected(graph.robber);
}

bool did_cop_win(GameState graph) {
    return !is_0_1_connected(~graph.cop);
}

bool is_move_legal(GameState graph, int u, int v) {
    return !has_edge(graph.cop, u, v) && !has_edge(graph.robber, u, v) && u != v;
}

int alpha_beta(GameState graph, int alpha, int beta, bool maximizingPlayer) {
    if (did_robber_win(graph)) return -1; // Robber wins
    if (did_cop_win(graph)) return 1; // Cop wins
    if (maximizingPlayer) {
        bool no_move_found = true;
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
                                int eval = alpha_beta(new_graph, alpha, beta, false);
                                alpha = max(alpha, eval);
                                if (beta <= alpha) break;
                            }
                        }
                    }
                }
            }
        }
        if(no_move_found) {
            for (int u = 0; u < graph.graph_size; u++) {
                for (int v = u + 1; v < graph.graph_size; v++) {
                    GameState new_graph = graph;
                    new_graph.cop = add_edge(new_graph.cop, u, v);
                    int eval = alpha_beta(new_graph, alpha, beta, false);
                    alpha = max(alpha, eval);
                    if (beta <= alpha) break;
                }
            }
        }
        return alpha;
    } else {
        for (int u = 0; u < graph.graph_size; u++) {
            for (int v = u + 1; v < graph.graph_size; v++) {
                if (is_move_legal(graph, u, v)) {
                    GameState new_graph = graph;
                    new_graph.robber = add_edge(new_graph.robber, u, v);
                    int eval = alpha_beta(new_graph, alpha, beta, true);
                    beta = min(beta, eval);
                    if (beta <= alpha) break;
                }
            }
        }
        return beta;
    }
}

void unit_tests() {
    Bitboard cop_graph = get_cop_starting_bitboard_for_size_k_graph(4);
    print_graph(cop_graph);
    Bitboard robber_graph = 0ULL;

    // Test adding edges
    cop_graph = add_edge(cop_graph, 0, 2);
    robber_graph = add_edge(robber_graph, 1, 3);

    // Test edge existence
    assert(has_edge(cop_graph, 0, 2));
    assert(!has_edge(cop_graph, 1, 3));

    // Test removing edges
    cop_graph = remove_edge(cop_graph, 0, 2);
    assert(!has_edge(cop_graph, 0, 2));

    // Test connectivity
    assert(!is_0_1_connected(robber_graph));
    robber_graph = add_edge(robber_graph, 2, 3);
    assert(!is_0_1_connected(robber_graph));
    robber_graph = add_edge(robber_graph, 2, 0);
    assert(is_0_1_connected(robber_graph));

    cout << "All unit tests passed!" << endl;
}

void run_game_test(int graph_size) {
    GameState initial_state(graph_size);
    int result = alpha_beta(initial_state, -1, 1, true);
    if (result == 1) {
        cout << "Cop wins on graph size " << graph_size << endl;
    } else {
        cout << "Robber wins on graph size " << graph_size << endl;
    }
}

int main() {
    unit_tests();
    for(int i = 0; i <= 6; i++) {
        run_game_test(i);
    }
    return 0;
}