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

	//
	//Zeichne das GUI
	//
	class EventListenerGUI : public VEEventListener {
	protected:


		virtual void onDrawOverlay(veEvent event) {
		}

	public:
		///Constructor of class EventListenerGUI
		EventListenerGUI(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerGUI
		virtual ~EventListenerGUI() {};
	};


	static std::default_random_engine e{ 12345 };					//F�r Zufallszahlen
	static std::uniform_real_distribution<> d{ -10.0f, 10.0f };		//F�r Zufallszahlen

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
		float cost_so_far = 2.0;
		Connection connection;
		float estimated_total_cost;
		Status status = Unvisited;
	};


	//
	// Pathfinding algorithm
	//
	class EventListenerPathfinding : public VEEventListener {
	private:
		std::vector<Node*> open_list;
		std::vector<Node*> closed_list;
		std::vector<Node*> route;
		float cost_map[6][6] = {
			{0.1, 0.1, 1.0, 1.0, 1.0, 1.0},
			{1.0, 0.1, 0.5, 0.1, 0.1, 1.0},
			{1.0, 1.0, 0.5, 0.1, 0.1, 1.0},
			{1.0, 0.5, 0.5, 0.1, 0.1, 1.0},
			{1.0, 0.1, 0.1, 0.1, 0.1, 1.0},
			{1.0, 0.1, 0.1, 0.5, 1.0, 1.0}
		};
		Node map[6][6] = {};
		//glm::ivec2 start = glm::ivec2(0, 0);
		//glm::ivec2 end = glm::ivec2(3, 5);
		//glm::ivec2 start = glm::ivec2(1, 4);
		//glm::ivec2 end = glm::ivec2(3, 3);
		glm::ivec2 start = glm::ivec2(1, 1);
		glm::ivec2 end = glm::ivec2(1, 4);

	protected:
		virtual void onFrameStarted(veEvent event) {

		};

		Node* lowest_cost_so_far(bool dijk) {
			float minmal_cost = std::numeric_limits<float>::max();;
			Node* lowest;
			for (int i = 0; i < open_list.size(); i++) {
				Node* current_node = open_list[i];
				float cost;
				if(dijk)
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
				Node* left = &map[node->position.x - 1][node->position.y];
				if (left->status == Unvisited) {
					left->position = glm::ivec2(node->position.x - 1, node->position.y);
					neighbors->push_back(left);
				}
			}
			if (node->position.y - 1 >= 0) {
				Node* top = &map[node->position.x][node->position.y - 1];
				if (top->status == Unvisited) {
					top->position = glm::ivec2(node->position.x, node->position.y - 1);
					neighbors->push_back(top);
				}
			}
			if (node->position.x + 1 <= 5) {
				Node* right = &map[node->position.x + 1][node->position.y];
				if (right->status == Unvisited) {
					right->position = glm::ivec2(node->position.x + 1, node->position.y);
					neighbors->push_back(right);
				}
			}
			if (node->position.y + 1 <= 5) {
				Node* bottom = &map[node->position.x][node->position.y + 1];
				if (bottom->status == Unvisited) {
					bottom->position = glm::ivec2(node->position.x, node->position.y + 1);
					neighbors->push_back(bottom);
				}
			}
		}

		float hops_to_start(Node* node) {
			glm::ivec2 p = node->position;
			float i = 0;
			while (p != start) {
				i++;
				Node* n = &map[p.x][p.y];
				p = map[n->connection.from.x][n->connection.from.y].position;
			}
			return 1 / (i * 0.4);
		}

	public:
		///Constructor of class EventListenerCollision
		EventListenerPathfinding(std::string name) : VEEventListener(name) {
			open_list = std::vector<Node*>();
		glm:ivec2 n;
			closed_list = std::vector<Node*>();

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
				lowest = lowest_cost_so_far(true);
				std::vector<Node*> neigh;
				neighbors(lowest, &neigh);
				for (auto& n : neigh) {
					n->connection.from = lowest->position;
					n->heuristic = hops_to_start(n);
					n->cost_so_far = lowest->cost_so_far + cost_map[n->position.y][n->position.x];
					float estimetd_cost = (36 - closed_list.size()) * .005; // Total number of nodes - nodes visited * average cost
					n->estimated_total_cost += n->cost_so_far + estimetd_cost * n->heuristic;
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

				n = lowest->position;
				while (n != start) {
					std::cout << glm::to_string(n + 1) << std::endl;
					n = map[n.x][n.y].connection.from;
				}
				std::cout << std::endl;
			}

			n = end;
			while (n != start) {
				std::cout << glm::to_string(n + 1) << std::endl;
				n = map[n.x][n.y].connection.from;
			}


		};

		///Destructor of class EventListenerCollision
		virtual ~EventListenerPathfinding() {};
	};

	//
	// Überprüfen, ob die Kamera die Kiste berührt
	//
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

				/*
				======================================================================================================================================================
																			 Task 7
				======================================================================================================================================================
				*/
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

				/*
				======================================================================================================================================================
																			/Task 7
				======================================================================================================================================================
				*/

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

			registerEventListener(new EventListenerPathfinding("Pathfinding"), { veEvent::VE_EVENT_FRAME_STARTED });
		};


		///Load the first level into the game engine
		///The engine uses Y-UP, Left-handed
		virtual void loadLevel(uint32_t numLevel = 1) {

			VEEngine::loadLevel(numLevel);			//create standard cameras and lights

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

			VESceneNode* e1, * e1Parent;
			e1Parent = getSceneManagerPointer()->createSceneNode("The Cube0 Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("The Cube0", "media/models/test/crate0", "cube.obj"));
			e1Parent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 5.0f, 10.0f)));
			e1Parent->addChild(e1);

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

