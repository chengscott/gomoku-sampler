#ifndef MCTS
#define MCTS
#include <algorithm>
#include <iostream>
using namespace std;

#include "mcts.h"

class GomokuState {
public:
	typedef pair<int, int> Move;
	static const Move no_move;

	static const char player_markers[3];
	static const string LABLES;
	static const map<string::value_type, int> LABLE_POS;

	GomokuState(int side_len_ = 9) :
		player_to_move(1),
		num_rows(side_len_),
		num_cols(side_len_),
		last_col(-1),
		last_row(-1)
	{
		board_size = num_rows * num_cols;
		board.resize(num_rows, vector<char>(num_cols, player_markers[0]));

		for (int i = 0; i < num_rows; ++i)
			for (int j = 0; j < num_cols; ++j)
				empty_places.push_back(make_pair(i, j));
	}

	void do_move(Move move) {
		board[move.first][move.second] = player_markers[player_to_move];
		empty_places.erase(std::remove(empty_places.begin(), empty_places.end(), move), empty_places.end());
		last_row = move.first, last_col = move.second;
		player_to_move = 3 - player_to_move;
	}

	template<typename RandomEngine>
	void do_random_move(RandomEngine* engine) {
		std::uniform_int_distribution<int> moves(0, static_cast<int>(empty_places.size()) - 1);
		int idx = moves(*engine);

		auto it(empty_places.begin());
		advance(it, idx);
		do_move(*it);
	}

	bool has_moves() const {
		char winner = get_winner();
		if (winner != player_markers[0])
			return false;
		return !empty_places.empty();
	}

	std::vector<Move> get_moves() const {
		std::vector<Move> moves;
		if (get_winner() != player_markers[0])
			return moves;
		return empty_places;
	}

	char get_winner() const {
		if (last_col < 0) return player_markers[0];
		// We only need to check around the last piece played.
		auto piece = board[last_row][last_col];
		// five in a column
		int left = 0, right = 0;
		for (int col = last_col - 1; col >= 0 && board[last_row][col] == piece; --col) left++;
		for (int col = last_col + 1; col < num_cols && board[last_row][col] == piece; ++col) right++;
		if (left + 1 + right >= 5)
			return piece;
		// five in a row
		int up = 0, down = 0;
		for (int row = last_row - 1; row >= 0 && board[row][last_col] == piece; --row) up++;
		for (int row = last_row + 1; row < num_rows && board[row][last_col] == piece; ++row) down++;
		if (up + 1 + down >= 5)
			return piece;
		// five in a diagonal
		up = 0; down = 0;
		for (int row = last_row - 1, col = last_col - 1; row >= 0 && col >= 0 && board[row][col] == piece; --row, --col) up++;
		for (int row = last_row + 1, col = last_col + 1; row < num_rows && col < num_cols && board[row][col] == piece; ++row, ++col) down++;
		if (up + 1 + down >= 5)
			return piece;
		// five in an antidiagonal
		up = 0; down = 0;
		for (int row = last_row + 1, col = last_col - 1; row < num_rows && col >= 0 && board[row][col] == piece; ++row, --col) up++;
		for (int row = last_row - 1, col = last_col + 1; row >= 0 && col < num_cols && board[row][col] == piece; --row, ++col) down++;
		if (up + 1 + down >= 5)
			return piece;

		return player_markers[0];
	}

	double get_result(int current_player_to_move) const {
		auto winner = get_winner();
		if (winner == player_markers[0])
			return 0.5;
		if (winner == player_markers[current_player_to_move])
			return 0.0;
		return 1.0;
	}

	void print(ostream& out) const {
		out << endl;
		out << "  ";
		for (int col = 0; col < num_cols - 1; ++col) {
			out << LABLES[col] << ' ';
		}
		out << LABLES[num_cols - 1] << endl;
		for (int row = 0; row < num_rows; ++row) {
			out << LABLES[row] << "|";
			for (int col = 0; col < num_cols - 1; ++col) {
				out << board[row][col] << ' ';
			}
			out << board[row][num_cols - 1] << "|" << endl;
		}
		out << " +";
		for (int col = 0; col < num_cols - 1; ++col) {
			out << "--";
		}
		out << "-+" << endl;
		out << player_markers[player_to_move] << " to move " << endl << endl;
	}

	string str() const {
		stringstream ss;
		for (int row = 0; row < num_rows; ++row) {
			for (int col = 0; col < num_cols; ++col) {
				char ch = board[row][col];
				int n = 0;
				if (ch == player_markers[1])
					n = 1;
				else if (ch == player_markers[2])
					n = 2;
				ss << n << ",";
			}
		}
		return ss.str();
	}

	int player_to_move;


private:
	int num_rows, num_cols;
	int board_size;
	vector<vector<char>> board;
	int last_col, last_row;
	vector<Move> empty_places;
};

ostream& operator << (ostream& out, const GomokuState& state) {
	state.print(out);
	return out;
}

ostream& operator << (ostream& out, const GomokuState::Move& move) {
	out << "[" << move.first << ", " << move.second << "]";
	return out;
}


const char GomokuState::player_markers[3] = { '.', 'X', 'O' };
const string GomokuState::LABLES = "0123456789ABCDEFGHI";
const map<string::value_type, int> GomokuState::LABLE_POS = {
	{'A',0 },{'B',1 },{'C',2 },{'D',3 },{'E',4 },{'F',5 },{'G',6 },{'H',7 },
	{'I',8 },{'J',9 },{'K',10 },{'L',11 },{'M',12 },{'N',13 },{'O',14 }
};
const GomokuState::Move GomokuState::no_move = make_pair(-1, -1);
#endif