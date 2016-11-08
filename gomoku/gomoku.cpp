#pragma GCC optimize("O7122")

#include <fstream>
#include <regex>
#include <iostream>
#include <time.h>
#include <conio.h>
using namespace std;

#define USE_OPENMP

#include "mcts.h"
#include "gomoku.h"

int main()
{
	using namespace std;

	bool human_player = true;

	MCTS::ComputeOptions player1_options, player2_options;
	player1_options.max_iterations = 10000000;
	player1_options.max_time = 20;
	player1_options.verbose = true;
	player2_options.max_iterations = 100;
	player2_options.verbose = true;

	GomokuState state(15);
	while (state.has_moves()) {
		cout << endl << "State: " << state << endl;

		GomokuState::Move move = GomokuState::no_move;
		if (state.player_to_move == 1) {
			move = MCTS::compute_move(state, player1_options);
			state.do_move(move);
		}
		else {
			if (human_player) {
				while (true) {
					cout << "Input your move: ";
					move = GomokuState::no_move;
					char row;
					int col;
					cin >> row >> col;
					move = make_pair(GomokuState::LABLE_POS.at(row), col);
					state.do_move(move);
				}
			}
			else {
				move = MCTS::compute_move(state, player2_options);
				state.do_move(move);
			}
		}
	}

	cout << endl << "Final state: " << state << endl;

	if (state.get_result(2) == 1.0)
		cout << "Player 1 wins!" << endl;
	else if (state.get_result(1) == 1.0)
		cout << "Player 2 wins!" << endl;
	else
		cout << "Draw!" << endl;
}
