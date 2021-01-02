/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"

#include "gjk_epa.h"
#include "contact.h"

namespace ve {


	uint32_t g_score = 0;				//derzeitiger Punktestand
	double g_time = 30.0;				//zeit die noch �brig ist
	bool g_gameLost = false;			//true... das Spiel wurde verloren
	bool g_restart = false;			//true...das Spiel soll neu gestartet werden

	static std::default_random_engine e{ 12345 };					//F�r Zufallszahlen
	static std::uniform_real_distribution<> d{ -10.0f, 10.0f };		//F�r Zufallszahlen

	//Implemented MoveTowardsEnemyFlag and MoveTowardsOwnFlag
	enum class Action {
		None = 0,
		MoveTowardsEnemyFlag = 1,
		MoveTowardsOwnFlag = 2,
		Heal = 3,
		Shoot = 4,
		Melee = 5,
		Flank = 6,
		Reload = 7,
		Idle = 8,
		MoveTowardsEnemy = 9,
	};

	enum class Feature {
		Health = 0,
		Ammu = 1,
		EnemyDist = 2,
		TeamMateDist = 3,
		DistToFlag = 4
	};

	enum class Comperator {
		Less = 0,
		LessOrEqual = 1,
		Equal = 2,
		GreaterOrEqual = 3,
		Greater = 4
	};

	class DecisionTree {
	public:
		struct BaseNode {
			std::string Id;
			std::string Decision;
			Feature Feature;
			Comperator Comperator;
			float Value;
		};
		struct Node : BaseNode {
			Node* BranchTrue = nullptr;
			Node* BranchFalse = nullptr;
			Action ActionTrue;
			Action ActionFalse;
		};

		Node* m_RootNode;
		std::unordered_map<std::string, Node*> Nodemap;

		//DecisionTree() {
		//	m_RootNode = nullptr;
		//};
		//DecisionTree(const DecisionTree&);
		//~DecisionTree() {};

		static void createNode(DecisionTree* dT, Node* node, std::vector<std::string> tokens) {
			std::vector<std::string> leafs;
			for (auto i = std::strtok(&tokens[4][0], "|"); i != NULL; i = std::strtok(NULL, "|"))
				leafs.push_back(i);

			node->Id = tokens[0];
			node->Feature = Feature(std::stoi(tokens[1]));
			node->Comperator = Comperator(std::stoi(tokens[2]));
			node->Value = std::stof(tokens[3]);
			if (std::isdigit(leafs[0][0])) {
				node->ActionTrue = Action(std::stoi(leafs[0]));
			}
			else {
				node->BranchTrue = new DecisionTree::Node();
				node->BranchTrue->Id = leafs[0];
				dT->Nodemap.insert(std::pair<std::string, Node*>(leafs[0], node->BranchTrue));
			}
			if (std::isdigit(leafs[1][0])) {
				node->ActionFalse = Action(std::stoi(leafs[1]));
			}
			else {
				node->BranchFalse = new DecisionTree::Node();
				node->BranchFalse->Id = leafs[1];
				dT->Nodemap.insert(std::pair<std::string, Node*>(leafs[1], node->BranchFalse));
			}
			dT->Nodemap.insert(std::pair<std::string, Node*>(tokens[0], node));
		}

		static DecisionTree LoadTree(std::string filename) {
			std::string line;
			std::ifstream file(filename);
			DecisionTree dT = DecisionTree();
			while (file.is_open()) {
				//Id*Feature*Comperator*Value*Action(True|False)
				while (std::getline(file, line)) {
					// Vector to store tokens
					std::vector<std::string> tokens;

					for (auto i = std::strtok(&line[0], "*"); i != NULL; i = std::strtok(NULL, "*"))
						tokens.push_back(i);
					if (dT.m_RootNode == nullptr) {
						dT.m_RootNode = new DecisionTree::Node();
						createNode(&dT, dT.m_RootNode, tokens);
					}
					else {
						std::string id = tokens[0];
						createNode(&dT, dT.Nodemap.find(id)->second, tokens);
					}
				}
				file.close();
			}
			return dT;
		}
	};

	//
	// Pathfinding algorithm
	//
	class Pathfinder {
	private:
		enum  Status {
			Open,
			Closed,
			Unvisited

		};

		struct Connection {
			glm::ivec2 from;
			float cost;
		};

		struct Node {
			glm::ivec2 position;
			float heuristic;
			float cost_so_far = std::numeric_limits<float>::max();
			Connection connection;
			float estimated_total_cost;
			Status status = Unvisited;
		};

		std::vector<Node*> open_list;
		std::vector<Node*> closed_list;
		std::vector<Node*> route;

		glm::ivec2 start;
		glm::ivec2 end;
		//glm::ivec2 start = glm::ivec2(1, 4);
		//glm::ivec2 end = glm::ivec2(3, 3);
		//glm::ivec2 start = glm::ivec2(1, 1);
		//glm::ivec2 end = glm::ivec2(1, 4);

	protected:
		virtual void onFrameStarted(veEvent event) {

		};

		Node* lowest_cost_so_far(bool dijk) {
			float minmal_cost = std::numeric_limits<float>::max();;
			Node* lowest;
			for (int i = 0; i < open_list.size(); i++) {
				Node* current_node = open_list[i];
				float cost;
				if (dijk)
					cost = current_node->cost_so_far;
				else
					cost = current_node->estimated_total_cost;
				if (cost < minmal_cost) {
					lowest = current_node;
					minmal_cost = cost;
				}
			}
			return lowest;
		}

		void neighbors(Node* node, std::vector<Node*>* neighbors) {
			if (node->position.x - 1 >= 0) {
				Node* left = &map[node->position.y][node->position.x - 1];
				if (left->status == Unvisited) {
					left->position = glm::ivec2(node->position.x - 1, node->position.y);
					neighbors->push_back(left);
				}
			}
			if (node->position.y - 1 >= 0) {
				Node* top = &map[node->position.y - 1][node->position.x];
				if (top->status == Unvisited) {
					top->position = glm::ivec2(node->position.x, node->position.y - 1);
					neighbors->push_back(top);
				}
			}
			if (node->position.x + 1 <= MAP_WIDTH - 1) {
				Node* right = &map[node->position.y][node->position.x + 1];
				if (right->status == Unvisited) {
					right->position = glm::ivec2(node->position.x + 1, node->position.y);
					neighbors->push_back(right);
				}
			}
			if (node->position.y + 1 <= MAP_HEIGHT - 1) {
				Node* bottom = &map[node->position.y + 1][node->position.x];
				if (bottom->status == Unvisited) {
					bottom->position = glm::ivec2(node->position.x, node->position.y + 1);
					neighbors->push_back(bottom);
				}
			}
		}

		float hops_to_start(Node* node) {
			glm::ivec2 p = node->position;
			float i = 0;
			//p != start
			//p.x != start.x && p.y != start.y
			while (p != start) {
				i++;
				Node* n = &map[p.y][p.x];
				p = map[n->connection.from.y][n->connection.from.x].position;
			}
			return 1 / (i * 0.4);
		}

	public:

		static const int MAP_HEIGHT = 40;
		static const int MAP_WIDTH = 28;

		float cost_map[MAP_HEIGHT][MAP_WIDTH] = {
			{0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.5, 0.1, 0.1, 1.0, 0.1, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 1.0, 0.5, 0.1, 0.1, 1.0, 0.1, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.5, 0.5, 0.1, 0.1, 1.0, 0.1, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.1, 0.1, 1.0, 0.5, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 0.1, 0.1, 0.1, 9.0, 9.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 9.0, 9.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 9.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 9.0, 9.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.5, 0.1, 0.1, 1.0, 0.1, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 1.0, 0.5, 0.1, 0.1, 1.0, 0.1, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.5, 0.5, 0.1, 0.1, 1.0, 0.1, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.1, 0.1, 1.0, 0.5, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0},
			{1.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0},
			{1.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, 9.0},
			{1.0, 0.3, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.3, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.3, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0},
			{0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.3, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.3, 0.5, 0.1, 0.1, 1.0, 0.3, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 1.0, 0.5, 0.1, 0.1, 1.0, 0.1, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.3, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.5, 0.5, 0.1, 0.1, 1.0, 0.3, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.3, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.1, 0.1, 1.0, 0.5, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 9.0, 9.0, 1.0},
			{1.0, 0.1, 0.3, 0.5, 1.0, 1.0, 0.1, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 9.0, 9.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 9.0, 9.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 9.0, 9.0, 1.0, 0.3, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 9.0, 9.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.3, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0},
			{0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.5, 0.1, 0.1, 1.0, 0.3, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 1.0, 0.5, 0.1, 0.3, 1.0, 0.1, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.5, 0.5, 0.1, 0.1, 1.0, 0.1, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.1, 0.1, 1.0, 0.5, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.3, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 0.1, 0.3, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.3, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0},
		};
		Node map[MAP_HEIGHT][MAP_WIDTH] = {};

		std::vector<glm::ivec2> result;


		std::vector < glm::ivec2> execude() {
			glm::ivec2 n;
			//create starting node
			Node* start_node = &map[start.x][start.y];
			start_node->position = start;
			//start_node.connection = NULL;
			start_node->cost_so_far = 0.0;
			start_node->estimated_total_cost = 0.0;
			start_node->heuristic = 0.0;
			start_node->status = Open;
			//Add starting point to open list
			open_list.push_back(start_node);
			Node* lowest = &map[start.x][start.y];
			while (lowest->position != end) {
				lowest = lowest_cost_so_far(false);
				std::vector<Node*> neigh;
				neighbors(lowest, &neigh);
				for (auto& n : neigh) {
					n->connection.from = lowest->position;
					n->heuristic = hops_to_start(n);
					n->cost_so_far = lowest->cost_so_far + cost_map[n->position.y][n->position.x];
					float estimetd_cost = (MAP_HEIGHT * MAP_WIDTH - closed_list.size()) * .005; // Total number of nodes - nodes visited * average cost
					n->estimated_total_cost = n->cost_so_far + n->heuristic;
					n->status = Open;
					open_list.push_back(n);
				}

				for (auto it = open_list.begin(); it != open_list.end(); ) {
					if (*it == lowest) {
						it = open_list.erase(it);
						(*it)->status = Closed;
						closed_list.push_back(*it);
						break;
					}
					it++;
				}

				//n = lowest->position;
				//while (n != start) {
				//	std::cout << glm::to_string(n + 1) << std::endl;
				//	n = map[n.x][n.y].connection.from;
				//}
				//std::cout << std::endl;
			}

			n = end;
			std::vector<glm::ivec2> result;
			while (n != start) {
				std::cout << glm::to_string(n + 1) << std::endl;
				n = map[n.x][n.y].connection.from;
				result.push_back(n);
			}
			std::reverse(result.begin(), result.end());
			return result;
		}

		///Constructor of class EventListenerCollision
		Pathfinder(glm::ivec2 _start, glm::ivec2 _end) {
			this->start = _start;
			this->end = _end;
			open_list = std::vector<Node*>();
			closed_list = std::vector<Node*>();

			glm::ivec2 n;
			//create starting node
			Node* start_node = &map[start.y][start.x];
			start_node->position = start;
			//start_node.connection = NULL;
			start_node->cost_so_far = 0.0;
			start_node->estimated_total_cost = 0.0;
			start_node->heuristic = 0.0;
			start_node->status = Open;
			//Add starting point to open list
			open_list.push_back(start_node);
			Node* lowest = &map[start.y][start.x];
			//lowest->position.x != end.x && lowest->position.y != end.y
			//(lowest->position != end)
			while (lowest->position != end && open_list.size() > 0) {
				lowest = lowest_cost_so_far(true);
				std::vector<Node*> neigh;
				neighbors(lowest, &neigh);
				for (auto& n : neigh) {
					n->connection.from = lowest->position;
					n->heuristic = hops_to_start(n);
					n->cost_so_far = lowest->cost_so_far + cost_map[n->position.y][n->position.x];
					float estimetd_cost = (MAP_HEIGHT * MAP_WIDTH - closed_list.size()) * .005; // Total number of nodes - nodes visited * average cost
					n->estimated_total_cost = n->cost_so_far + n->heuristic;
					n->status = Open;
					open_list.push_back(n);
				}

				for (auto it = open_list.begin(); it != open_list.end(); ) {
					if (*it == lowest) {
						(*it)->status = Closed;
						closed_list.push_back(*it);
						it = open_list.erase(it);
						break;
					}
					it++;
				}
			}

			n = end;
			result.push_back(n);
			while (n != start) {
				//std::cout << glm::to_string(n + 1) << std::endl;
				n = map[n.y][n.x].connection.from;
				result.push_back(n);
			}
			std::reverse(result.begin(), result.end());
		};

		///Destructor of class EventListenerCollision
		~Pathfinder() {};
	};

	class Agent {
	private:
	public:
		glm::ivec2 position;
		glm::ivec2 nextPositionToMoveToEnemy;
		glm::ivec2 nextPositionToMoveToFlag;
		std::string name;
		float health;
		float ammu;
		float distToFlag;
		float enemyDist;
		DecisionTree dT;

		void MoveTowardsEnemy() {
			std::cout << "MoveTowardsEnemy" << std::endl;
			position = nextPositionToMoveToEnemy;
		}
		void MoveTowardsEnemyFlag() {
			std::cout << "MoveTowardsEnemyFlag" << std::endl;
			position = nextPositionToMoveToFlag;
		}
		void MoveTowardsOwnFlag() {
			std::cout << "MoveTowardsOwnFlag" << std::endl;
		}
		void Heal() {
			std::cout << "Heal" << std::endl;
		}
		void Shoot() {
			std::cout << "Shoot" << std::endl;
		}
		void Melee() {
			std::cout << "Melee" << std::endl;
		}
		void Flank() {
			std::cout << "Flank" << std::endl;
		}
		void Reload() {
			std::cout << "Reload" << std::endl;
		}
		void Idle() {
			std::cout << "Idle" << std::endl;
		}

		Action makeDecision() {
			Action result = ve::Action::None;
			DecisionTree::Node* node = dT.m_RootNode;
			while (result == ve::Action::None) {
				float agent_value = getFeature(node->Feature);
				bool decision = false;
				switch (node->Comperator) {
				case Comperator::Equal:
					decision = agent_value == node->Value;
					break;
				case Comperator::Greater:
					decision = agent_value > node->Value;
					break;
				case Comperator::GreaterOrEqual:
					decision = agent_value >= node->Value;
					break;
				case Comperator::Less:
					decision = agent_value < node->Value;
					break;
				case Comperator::LessOrEqual:
					decision = agent_value <= node->Value;
					break;
				}
				if (decision) {
					result = node->ActionTrue;
					node = node->BranchTrue;
				}
				else {
					result = node->ActionFalse;
					node = node->BranchFalse;
				}
			}
			return result;
		}

		float getFeature(Feature feature) {
			switch (feature) {
			case Feature::Health:
				return health;
			case Feature::Ammu:
				return health;
			case Feature::EnemyDist:
				return health;
			case Feature::TeamMateDist:
				return health;
			case Feature::DistToFlag:
				return health;
			}
		}
	};




	//
	//Perform Actions for NPCS
	//
	class EventListenerNPC : public VEEventListener {
	private:

		Agent* createAgent(int i, VESceneNode* parent) {
			Agent* agent = new Agent();
			agent->name = "Agent" + std::to_string(i);
			agent->position = glm::ivec2(parent->getPosition().x, parent->getPosition().z);
			agent->health = 25;
			agent->ammu = 2;
			agent->enemyDist = Pathfinder::MAP_HEIGHT - 1;
			agent->distToFlag = Pathfinder::MAP_HEIGHT - 1;
			agent->dT = DecisionTree::LoadTree("media/DT.txt");
			return agent;
		}


		std::vector<Agent*> blueTeam;
		std::vector<Agent*> redTeam;

		void executeDT(Agent* agent) {
			Action action = agent->makeDecision();
			switch (action) {
			case Action::None:
			case Action::Idle:
				agent->Idle();
				break;
			case Action::Flank:
				agent->Flank();
				break;
			case Action::Heal:
				agent->Heal();
				break;
			case Action::Melee:
				agent->Melee();
				break;
			case Action::MoveTowardsEnemy:
				agent->MoveTowardsEnemy();
				break;
			case Action::MoveTowardsEnemyFlag:
				agent->MoveTowardsEnemyFlag();
				break;
			case Action::MoveTowardsOwnFlag:
				agent->MoveTowardsOwnFlag();
				break;
			case Action::Reload:
				agent->Reload();
				break;
			case Action::Shoot:
				agent->Shoot();
				break;
			}
		}

		void prepareDecsion(Agent* agent, std::vector<Agent*> enemyTeam, glm::ivec2 captureFlagPos) {
			float minDistance = std::numeric_limits<float>::max();
			for (Agent* enemy : enemyTeam) {
				Pathfinder* pf = new Pathfinder(agent->position, enemy->position);
				std::vector<glm::ivec2> pathToEnemy = pf->result;
				float distance = pathToEnemy.size();
				if (distance < minDistance) {
					minDistance = distance;
					if (pathToEnemy.size() > 1)
						agent->nextPositionToMoveToEnemy = pathToEnemy[1];
					else
						agent->nextPositionToMoveToEnemy = pathToEnemy[0];
				}
				delete pf;
			}
			agent->enemyDist = minDistance;
			Pathfinder* pf = new Pathfinder(agent->position, captureFlagPos);
			std::vector<glm::ivec2> pathToFlag = pf->result;
			agent->distToFlag = pathToFlag.size();
			if (pathToFlag.size() > 1)
				agent->nextPositionToMoveToFlag = pathToFlag[1];
			else
				agent->nextPositionToMoveToFlag = pathToFlag[0];
			delete pf;
		}

	protected:

		virtual void onFrameStarted(veEvent event) {
			if (g_gameLost)
				return;
			for (Agent* agent : redTeam) {
				prepareDecsion(agent, blueTeam, glm::ivec2(0, 3));
				executeDT(agent);
				if (agent->position == glm::ivec2(0, 3)) {
					std::cout << "Red Team Won" << std::endl;
					g_gameLost = true;
				}
			}
			for (Agent* agent : blueTeam) {
				prepareDecsion(agent, redTeam, glm::ivec2(3, 7));
				executeDT(agent);
				if (agent->position == glm::ivec2(3, 7)) {
					std::cout << "Blue Team Won" << std::endl;
					g_gameLost = true;
				}
			}
		}

	public:
		///Constructor of class EventListenerNPC
		EventListenerNPC(std::string name) : VEEventListener(name) {
			for (int i = 0; i < 5; i++) {
				VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Cube" + std::to_string(i) + " Parent");
				redTeam.push_back(createAgent(i, eParent));
			}
			for (int i = 5; i < 10; i++) {
				VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Cube" + std::to_string(i) + " Parent");
				blueTeam.push_back(createAgent(i, eParent));
			}
		};

		///Destructor of class EventListenerNPC
		virtual ~EventListenerNPC() {};
	};

	class EventListenerKeyboard : public VEEventListener {
		glm::vec3 linearMomentum;
		glm::vec3 force;
		glm::vec3 angularMomentum;
		float rotSpeed;
		float mass = 1.0f;
		float static_friction = 0.0f;
		float dynamic_friction = 0.0f;
		glm::vec3 g = glm::vec3(0, -0.0981f, 0);
		glm::vec3 angularVelocity;
		glm::mat3 inertiaTensor;
		bool gravity = false;
		bool cube_spawned = true;

	private:
		//m .. mass
		glm::mat4 get_K(float mA, float mB, glm::vec3 rA, glm::vec3 rB, glm::mat3 itA, glm::mat3 itB) {
			glm::mat4 lhs = -(1 / mB) * glm::mat3() + glm::matrixCross3(rB) * glm::inverse(itB) * glm::matrixCross3(rB);
			glm::mat4 rhs = (1 / mA) * glm::mat3() + glm::matrixCross3(rA) * glm::inverse(itA) * glm::matrixCross3(rA);
			return lhs - rhs;
		}

		glm::vec3 get_prel(glm::vec3 lvA, glm::vec3 lvB, glm::vec3 aVA, glm::vec3 aVB, glm::vec3 rA, glm::vec3 rB) {
			glm::vec3 pA = lvA + glm::cross(aVA, rA);
			glm::vec3 pB = lvB + glm::cross(aVB, rB);
			return pB - pA;
		}

		float get_d(glm::vec3 n, glm::vec3 prel) {
			return glm::dot(n, prel);
		}

		//always at time before the collision
		//vA ... linearVELOCITY
		//wA ... angularVELOCITY
		//rA = collisionPoint - center
		//𝑷𝐴(𝑡)= 𝒗𝐴(𝑡)+ 𝝎𝐴(𝑡)× 𝒓A
		//𝑷rel(𝑡) = 𝑷𝐵(𝑡) − 𝑷𝐴(𝑡)
		//𝑑(𝑡)= 𝒏 ∙ 𝑷rel(𝑡)
		//t
		glm::vec3 get_tangetial_velocity(glm::vec3 lvA, glm::vec3 lvB, glm::vec3 aVA, glm::vec3 aVB, glm::vec3 rA, glm::vec3 rB, glm::vec3 n) {
			glm::vec3 prel = get_prel(lvA, lvB, aVA, aVB, rA, rB);
			float d = get_d(n, prel);
			glm::vec3 t_ = prel - d * n;
			return glm::normalize(t_);
		}

		//f
		glm::vec3 get_mag_imp_dirN(float e, float d, glm::vec3 n, glm::mat4 K, float dF, glm::vec3 t) {
			float d1 = -(1 + e) * d;
			glm::vec3 n_ = n - dF * t;
			glm::vec3 d2 = glm::translate(n) * K * glm::vec4(n_.x, n_.y, n_.z, 0.0f);
			return d1 / d2;
		}

		glm::vec3 fHat(glm::vec3 lvA, glm::vec3 lvB, glm::vec3 aVA, glm::vec3 aVB, float mA, float mB, glm::vec3 rA, glm::vec3 rB, glm::mat3 itA, glm::mat3 itB, float e, glm::vec3 n, float dF) {
			glm::vec3 prel = get_prel(lvA, lvB, aVA, aVB, rA, rB);
			float d = get_d(n, prel);
			glm::mat4 K = get_K(mA, mB, rA, rB, itA, itB);
			glm::vec3 t = get_tangetial_velocity(lvA, lvB, aVA, aVB, rA, rB, n);
			glm::vec3 f = get_mag_imp_dirN(e, d, n, K, dF, t);
			return f * n - dF * f * t;
		}

		void dampenForce(float dampening, glm::vec3& forcetd) {
			glm::vec3 nextForce = forcetd - dampening * forcetd;
			if (glm::length(nextForce) > 0) {
				forcetd = nextForce;
			}
			else {
				forcetd = glm::vec3(0, 0, 0);
			}
		}

		glm::vec3 getFF(float friction) {
			glm::vec3 ff = g * friction;
			if (glm::length(linearMomentum) <= glm::length(ff))
				return linearMomentum;
			return ff;
		}

		void applyFriction() {
			linearMomentum -= getFF(static_friction);
			if (glm::length(linearMomentum) > 0) {
				linearMomentum -= getFF(dynamic_friction);
			}
		}

		void applyGravity(veEvent event) {
			VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Cube0 Parent");

			//Assume Object is in the air if y is above y
			if (eParent->getPosition().y > 1.0f) {
				linearMomentum += (float)event.dt * mass * g;
			}
		}

		void applyMovement(veEvent event, bool gravity) {
			VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Cube0 Parent");

			if (gravity) {
				if (eParent->getPosition().y > 1.0f) {
					linearMomentum += (float)event.dt * mass * (g + force);
				}
			}
			else {
				linearMomentum += (float)event.dt * mass * force;
			}
			applyFriction();
			eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), linearMomentum));
		}

		void applyRotation(veEvent event) {

			VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Cube0 Parent");
			VESceneNode* e1 = getSceneManagerPointer()->getSceneNode("The Cube0");

			glm::vec3 position = eParent->getPosition(); //in Local Space

			//Assume that the objects center is its postion
			glm::vec4 center = glm::vec4(position.x, position.y, position.z, 1);

			glm::mat4 orientation = eParent->getTransform(); //in Local 
			// Kreuzprodukt von (dem Produkt zwischen orientierungsmatrix und center vector) und dem force vector.
			glm::vec4 c = orientation * center;
			glm::vec3 torque = glm::cross(glm::vec3(c.x, c.y, c.z), force);
			// mass * velocity; mass = 1;
			angularMomentum += (float)event.dt * torque; //es selbst plus delta time mal torque.
			//orientierungsmatrix mal dem inversen inertia tensor mal der transponierten orientierungsmatrix mal dem angular momentum.
			angularVelocity = orientation * glm::mat4(inertiaTensor) * glm::transpose(orientation) * glm::vec4(angularMomentum.x, angularMomentum.y, angularMomentum.z, 1);

			glm::vec4 rot4 = glm::vec4(1.0);
			float angle = rotSpeed * (float)event.dt;
			rot4 = e1->getTransform() * glm::vec4(0.0, 0.1, 0.0, 1.0);
			glm::vec3  rot3 = glm::vec3(rot4.x, rot4.y, rot4.z);

			rot3 = rot3 + float(event.dt) * glm::matrixCross3(angularVelocity) * rot3;
			glm::mat4  rotate = glm::rotate(glm::mat4(1.0), angle, rot3);
			e1->multiplyTransform(rotate);
		}

		void checkCollision() {
			glm::vec3 positionCube0 = getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->getTransform()[3];
			glm::mat4 rotationCube0 = getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->getWorldRotation();
			glm::vec3 positionPlane = glm::vec3(getSceneManagerPointer()->getSceneNode("The Plane")->getPosition());

			//lm::mat3(getSceneManagerPointer()->getSceneNode("The Cube0")->getRotation())
			vpe::Box cube0{ positionCube0, rotationCube0 };
			//assume that the center of the plane is directly under the cube
			positionPlane = positionCube0;
			positionPlane.y = 0;
			vpe::Box plane{ positionPlane, scale(mat4(1.0f), vec3(100.0f, 1.0f, 100.0f)) };


			vec3 mtv(0, 1, 0); //minimum translation vector
			mtv = glm::normalize(force + (g * -1));

			bool hit = vpe::collision(cube0, plane, mtv);


			if (hit) {
				std::set<vpe::contact> ct;
				vec3 mtv(0, 1, 0); //minimum translation vector
				mtv = glm::normalize(force + (g * -1));


				vpe::contacts(cube0, plane, mtv, ct);

				//resolv interpenetration
				//works better without resolving
				//Maybe this is resolved somewhere else 
				//getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->multiplyTransform(glm::translate(glm::mat4(1.0f), mtv));

				float e = 0.1f;
				float dF = 0.2;
				//assume postion as center
				glm::vec3 cA = positionCube0;
				glm::vec3 f_ = glm::vec3(0.0f);
				glm::vec3 rA = glm::vec3(0.0f);
				glm::vec3 rB = glm::vec3(0.0f);
				std::set<vpe::contact>::iterator itr;
				for (itr = ct.begin(); itr != ct.end(); itr++) {
					//Vectors in World Space
					//some how the vectors make more sense if converting them from World to Local Space
					glm::vec3 p = itr->obj2->posW2L(itr->pos);
					glm::vec3 rA_ = p - itr->obj2->pos();

					p = itr->obj1->posW2L(itr->pos);
					glm::vec3 rB_ = p - itr->obj1->pos();

					//rA_ = itr->obj2->posL2W(rA_);;
					//rB_ = itr->obj1->posL2W(rB_);;

					glm::vec3 lvA = force + g;
					glm::vec3 lvB = glm::vec3();
					glm::vec3 aVB = glm::vec3();
					glm::vec3 prel = get_prel(lvA, lvB, angularVelocity, aVB, rA_, rB_);
					glm::vec3 n = itr->normal;
					float d = get_d(n, prel);
					if (d < 0) {
						glm::vec3 f_part = fHat(lvA, lvB, angularVelocity, aVB, mass, 1, rA_, rB_, inertiaTensor, inertiaTensor, e, n, dF);
						rA += rA_;
						rB += rB_;
						f_ += f_part;
					}
					if (d == 0) {
						glm::vec3 f_part = fHat(lvA, lvB, angularVelocity, aVB, mass, 1, rA_, rB_, inertiaTensor, inertiaTensor, 0.0f, n, dF);
						rA += rA_;
						rB += rB_;
						f_ += f_part;
					}
				}


				f_ /= ct.size();
				rA /= ct.size();
				rB /= ct.size();
				force = f_;
				linearMomentum = f_;
				angularMomentum = glm::cross(rA, f_);

				getEnginePointer()->m_irrklangEngine->removeAllSoundSources();
				getEnginePointer()->m_irrklangEngine->play2D("media/sounds/gameover.wav", false);
			}
		}

	protected:
		virtual void onFrameStarted(veEvent event) {
			checkCollision();
			applyRotation(event);
			applyMovement(event, gravity);
			dampenForce(0.002f, force);
			dampenForce(0.002f, linearMomentum);
		};

		virtual bool onKeyboard(veEvent event) {
			if (event.idata3 == GLFW_RELEASE) return false;

			if (event.idata1 == GLFW_KEY_SPACE && event.idata3 == GLFW_PRESS) {
				if (cube_spawned) {
					force += glm::vec3(0.0f, 0.0f, 0.0f);
					gravity = true;
					rotSpeed = 1.5f;
					rotSpeed = (float)d(e) / 10;
				}
				else {
					gravity = false;
					getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->setPosition(glm::vec3(-5.0f, 5.0f, 10.0f));
					//getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->setTransform(glm::mat4(1.0f));
					linearMomentum = glm::vec3(0.0f);
					angularMomentum = glm::vec4(0.0f);
					angularVelocity = glm::vec3(0.0f);;
					force = glm::vec3(0.0f);
					rotSpeed = 0;
				}
				cube_spawned = !cube_spawned;
				return true;
			}
			return false;
		}

	public:
		///Constructor of class EventListenerCollision
		EventListenerKeyboard(std::string name) : VEEventListener(name) {
			linearMomentum = glm::vec3();
			angularMomentum = glm::vec4();
			angularVelocity = glm::vec3();;
			inertiaTensor = glm::mat3(
				1.0f, -1.0f, -1.0f,
				-1.0f, 1.0f - 1.0f,
				-1.0f, -1.0f, 1.0f, 0.0f); //assume interia tensor is 1
			force = glm::vec3();
			rotSpeed = 0;
		};

		///Destructor of class EventListenerCollision
		virtual ~EventListenerKeyboard() {};
	};


	///user defined manager class, derived from VEEngine
	class MyVulkanEngine : public VEEngine {
	public:

		MyVulkanEngine(bool debug = false) : VEEngine(debug) {};
		~MyVulkanEngine() {};


		///Register an event listener to interact with the user

		virtual void registerEventListeners() {
			VEEngine::registerEventListeners();

			registerEventListener(new EventListenerNPC("NPC"), { veEvent::VE_EVENT_FRAME_STARTED });
		};


		///Load the first level into the game engine
		///The engine uses Y-UP, Left-handed
		virtual void loadLevel(uint32_t numLevel = 1) {


			VESceneNode* pScene;
			VECHECKPOINTER(pScene = getSceneManagerPointer()->createSceneNode("Level 1", getRoot()));

			//scene models

			VESceneNode* sp1;
			VECHECKPOINTER(sp1 = getSceneManagerPointer()->createSkybox("The Sky", "media/models/test/sky/cloudy",
				{ "bluecloud_ft.jpg", "bluecloud_bk.jpg", "bluecloud_up.jpg",
					"bluecloud_dn.jpg", "bluecloud_rt.jpg", "bluecloud_lf.jpg" }, pScene));

			VESceneNode* e4;
			VECHECKPOINTER(e4 = getSceneManagerPointer()->loadModel("The Plane", "media/models/test", "plane_t_n_s.obj", 0, pScene));
			e4->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f, 1.0f, 1000.0f)));

			VEEntity* pE4;
			VECHECKPOINTER(pE4 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Plane/plane_t_n_s.obj/plane/Entity_0"));
			pE4->setParam(glm::vec4(1000.0f, 1000.0f, 0.0f, 0.0f));

			//Generate Red Team
			//Cube 0-4
			for (int i = 0; i < 5; i++) {
				VESceneNode* e1, * e1Parent;
				e1Parent = getSceneManagerPointer()->createSceneNode("The Cube" + std::to_string(i) + " Parent", pScene, glm::mat4(1.0));
				VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(i), "media/models/test/crate_red", "cube.obj"));
				e1Parent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3((int)(Pathfinder::MAP_WIDTH / 2 - 3) + i, 1.0f, Pathfinder::MAP_HEIGHT - 1)));
				e1Parent->addChild(e1);
			}

			//Generate Blue Team
			//Cube 5-9
			for (int i = 5; i < 10; i++) {
				VESceneNode* e1, * e1Parent;
				e1Parent = getSceneManagerPointer()->createSceneNode("The Cube" + std::to_string(i) + " Parent", pScene, glm::mat4(1.0));
				VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(i), "media/models/test/crate_blue", "cube.obj"));
				e1Parent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3((int)(Pathfinder::MAP_WIDTH / 2 - 3) + i, 1.0f, 0.0f)));
				e1Parent->addChild(e1);
			}

			VEEngine::loadLevel(numLevel);			//create standard cameras and lights

			m_irrklangEngine->play2D("media/sounds/ophelia.wav", true);
		};
	};


}

using namespace ve;

int main() {

	bool debug = true;

	MyVulkanEngine mve(debug);	//enable or disable debugging (=callback, validation layers)


	mve.initEngine();
	mve.loadLevel(1);
	mve.run();

	return 0;
}

