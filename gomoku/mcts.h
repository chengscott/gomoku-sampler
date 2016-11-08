namespace MCTS {
struct ComputeOptions {
	int number_of_threads;
	int number_of_repeat;
	int iterations_param1;
	int iterations_param2;
	int max_iterations;
	int top_n;
	int board_size;
	double max_time;
	bool verbose;
	double best_wins;
	int best_visits;
	bool quit;

	ComputeOptions() :
		number_of_threads(8),
		number_of_repeat(100),
		iterations_param1(10),
		iterations_param2(1000),
		max_iterations(10000),
		top_n(1),
		board_size(15),
		max_time(-1.0), // default is no time limit.
		verbose(false),
		best_wins(0.),
		best_visits(0),
		quit(false)
	{ }
};

template<typename State>
typename State::Move compute_move(const State root_state,
                                  ComputeOptions& options = ComputeOptions());
}

#include <algorithm>
#include <cstdlib>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef USE_OPENMP
#include <omp.h>
#endif

namespace MCTS {
using std::endl;
using std::vector;

//
// This class is used to build the game tree. The root is created by the users and
// the rest of the tree is created by add_node.
//
template<typename State>
class Node {
public:
	typedef typename State::Move Move;

	Node(const State& state);
	~Node();

	bool has_untried_moves() const;
	template<typename RandomEngine>
	Move get_untried_move(RandomEngine* engine) const;
	Node* best_child() const;

	bool has_children() const {
		return ! children.empty();
	}

	Node* select_child_UCT() const;
	Node* add_child(const Move& move, const State& state);
	void update(double result);

	std::string to_string() const;

	const Move move;
	Node* const parent;
	const int player_to_move;
	
	//std::atomic<double> wins;
	//std::atomic<int> visits;
	double wins;
	int visits;

	std::vector<Move> moves;
	std::vector<Node*> children;

private:
	Node(const State& state, const Move& move, Node* parent);

	std::string indent_string(int indent) const;

	Node(const Node&);

	double UCT_score;
};


/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////


template<typename State>
Node<State>::Node(const State& state) :
	move(State::no_move),
	parent(nullptr),
	player_to_move(state.player_to_move),
	wins(0),
	visits(0),
	moves(state.get_moves()),
	UCT_score(0)
{ }

template<typename State>
Node<State>::Node(const State& state, const Move& move_, Node* parent_) :
	move(move_),
	parent(parent_),
	player_to_move(state.player_to_move),
	wins(0),
	visits(0),
	moves(state.get_moves()),
	UCT_score(0)
{ }

template<typename State>
Node<State>::~Node() {
	for (auto child: children) delete child;
}

template<typename State>
bool Node<State>::has_untried_moves() const {
	return ! moves.empty();
}

template<typename State>
template<typename RandomEngine>
typename State::Move Node<State>::get_untried_move(RandomEngine* engine) const {
	std::uniform_int_distribution<std::size_t> moves_distribution(0, moves.size() - 1);
	return moves[moves_distribution(*engine)];
}

template<typename State>
Node<State>* Node<State>::best_child() const {
	return *std::max_element(children.begin(), children.end(),
		[](Node* a, Node* b) { return a->visits < b->visits; });;
}

template<typename State>
Node<State>* Node<State>::select_child_UCT() const {
	for (auto child: children) {
		child->UCT_score = double(child->wins) / double(child->visits) +
			std::sqrt(2.0 * std::log(double(this->visits)) / child->visits);
	}

	return *std::max_element(children.begin(), children.end(),
		[](Node* a, Node* b) { return a->UCT_score < b->UCT_score; });
}

template<typename State>
Node<State>* Node<State>::add_child(const Move& move, const State& state) {
	auto node = new Node(state, move, this);
	children.push_back(node);

	auto itr = moves.begin();
	for (; itr != moves.end() && *itr != move; ++itr);
	moves.erase(itr);
	return node;
}

template<typename State>
void Node<State>::update(double result) {
	visits++;

	wins += result;
	//double my_wins = wins.load();
	//while ( ! wins.compare_exchange_strong(my_wins, my_wins + result));
}

template<typename State>
std::string Node<State>::to_string() const {
	std::stringstream sout;
	sout << "["
	     << "P" << 3 - player_to_move << " "
	     << "M:" << move << " "
	     << "W/V: " << wins << "/" << visits << " "
	     << "U: " << moves.size() << "]\n";
	return sout.str();
}

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////


template<typename State>
std::unique_ptr<Node<State>> compute_tree(const State root_state,
                                           const ComputeOptions options,
                                           std::mt19937_64::result_type initial_seed)
{
	std::mt19937_64 random_engine(initial_seed);

	if (options.max_time >= 0) {
		#ifndef USE_OPENMP
		throw std::runtime_error("ComputeOptions::max_time requires OpenMP.");
		#endif
	}
	// Will support more players later.
	auto root = std::unique_ptr<Node<State>>(new Node<State>(root_state));

	#ifdef USE_OPENMP
	double start_time = ::omp_get_wtime();
	double print_time = start_time;
	#endif

	for (int iter = 1; iter <= options.max_iterations || options.max_iterations < 0; ++iter) {
		auto node = root.get();
		State state = root_state;

		// Select a path through the tree to a leaf node.
		while (!node->has_untried_moves() && node->has_children()) {
			node = node->select_child_UCT();
			state.do_move(node->move);
		}

		// If we are not already at the final state, expand the
		// tree with a new node and move there.
		if (node->has_untried_moves()) {
			auto move = node->get_untried_move(&random_engine);
			state.do_move(move);
			node = node->add_child(move, state);
		}

		// We now play randomly until the game ends.
		while (state.has_moves()) {
			state.do_random_move(&random_engine);
		}

		// We have now reached a final state. Backpropagate the result
		// up the tree to the root node.
		while (node != nullptr) {
			node->update(state.get_result(node->player_to_move));
			node = node->parent;
		}

		#ifdef USE_OPENMP
		if (options.verbose || options.max_time >= 0) {
			double time = ::omp_get_wtime();
			if (options.verbose && (time - print_time >= 1.0 || iter == options.max_iterations)) {
				std::cerr << iter << " games played (" << double(iter) / (time - start_time) << " / second)." << endl;
				print_time = time;
			}

			if (time - start_time >= options.max_time) {
				break;
			}
		}
		#endif
	}

	return root;
}

template<typename State>
typename State::Move compute_move(const State root_state,
                                  ComputeOptions& options) {
	using namespace std;

	// Will support more players later.

	auto moves = root_state.get_moves();
	if (moves.size() == 1) {
		return moves[0];
	}

	#ifdef USE_OPENMP
	double start_time = ::omp_get_wtime();
	#endif

	std::random_device device;
	auto seed = (static_cast<uint64_t>(device()) << 32) | device();

	// Start all jobs to compute trees.
	vector<future<unique_ptr<Node<State>>>> root_futures;
	ComputeOptions job_options = options;
	job_options.verbose = false;
	for (int t = 0; t < options.number_of_threads; ++t) {
		auto func = [t, &root_state, &job_options, seed] () -> std::unique_ptr<Node<State>>
		{
			//return compute_tree(root_state, job_options, 1012411 * t + 12515);
			return compute_tree(root_state, job_options, seed);
		};

		root_futures.push_back(std::async(std::launch::async, func));
	}

	// Collect the results.
	vector<unique_ptr<Node<State>>> roots;
	for (int t = 0; t < options.number_of_threads; ++t) {
		roots.push_back(std::move(root_futures[t].get()));
	}

	// Merge the children of all root nodes.
	map<typename State::Move, int> visits;
	map<typename State::Move, double> wins;
	long long games_played = 0;
	for (int t = 0; t < options.number_of_threads; ++t) {
		auto root = roots[t].get();
		games_played += root->visits;
		for (auto child = root->children.cbegin(); child != root->children.cend(); ++child) {
			visits[(*child)->move] += (*child)->visits;
			wins[(*child)->move]   += (*child)->wins;
		}
	}

	//int bound = std::min<int>(static_cast<int>(visits.size()), options.top_n);
	//std::uniform_int_distribution<int> dist(1, bound);
	//std::random_device rd;
	//std::mt19937 gen(rd());
	//int nth = dist(gen) - 1;
	//vector<std::pair<typename State::Move, int> > mapcopy(visits.begin(), visits.end());
	//sort(mapcopy.begin(), mapcopy.end(), [&](auto a, auto b) -> auto {
	//	auto movea = a.first;
	//	auto va = a.second;
	//	auto wa = wins[movea];
	//	auto moveb = b.first;
	//	auto vb = b.second;
	//	auto wb = wins[moveb];
	//	return (wa + 1) / (va + 2) > (wb + 1) / (vb + 2);
	//});

	//typename State::Move best_move = mapcopy.at(nth).first;

	//if (options.verbose) {
	//	for (auto itr : mapcopy) {
	//		cerr << "Move: " << itr.first
	//			<< " (" << setw(2) << right << int(100.0 * itr.second / double(games_played) + 0.5) << "% visits)"
	//			<< " (" << setw(2) << right << int(100.0 * wins[itr.first] / itr.second + 0.5) << "% wins)" << endl;
	//	}
	//	cout << endl;
	//}

	// Find the node with the most visits.
	double best_score = -1;
	typename State::Move best_move = typename State::Move();
	for (auto itr: visits) {
		auto move = itr.first;
		double v = itr.second;
		double w = wins[move];
		// Expected success rate assuming a uniform prior (Beta(1, 1)).
		// https://en.wikipedia.org/wiki/Beta_distribution
		double expected_success_rate = (w + 1) / (v + 2);
		if (expected_success_rate > best_score) {
			best_move = move;
			best_score = expected_success_rate;
		}

		if (options.verbose) {
			cerr << "Move: " << itr.first
			     << " (" << setw(2) << right << int(100.0 * v / double(games_played) + 0.5) << "% visits)"
			     << " (" << setw(2) << right << int(100.0 * w / v + 0.5)    << "% wins)" << endl;
		}
	}

	if (options.verbose) {
		auto best_wins = wins[best_move];
		auto best_visits = visits[best_move];
		cerr << "----" << endl;
		cerr << "Best: " << best_move
		     << " (" << 100.0 * best_visits / double(games_played) << "% visits)"
		     << " (" << 100.0 * best_wins / best_visits << "% wins)" << endl;
	}

	#ifdef USE_OPENMP
	if (options.verbose) {
		double time = ::omp_get_wtime();
		std::cerr << games_played << " games played in " << double(time - start_time) << " s. " 
		          << "(" << double(games_played) / (time - start_time) << " / second, "
		          << options.number_of_threads << " parallel jobs)." << endl;
	}
	#endif

	return best_move;
}

}
