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

	//
	// �berpr�fen, ob die Kamera die Kiste ber�hrt
	//
	class EventListenerCollision : public VEEventListener {
	protected:
		virtual void onFrameStarted(veEvent event) {
			static uint32_t cubeid = 0;


		};

	public:
		///Constructor of class EventListenerCollision
		EventListenerCollision(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerCollision
		virtual ~EventListenerCollision() {};
	};

	//
// �berpr�fen, ob die Kamera die Kiste ber�hrt
//
	class EventListenerKeyboard : public VEEventListener {
		glm::vec3 linearMomentum;
		glm::vec3 force;
		glm::vec3 angularMomentum;
		float rotSpeed;
		float mass = 1.0f;
		float static_friction = 0.0f;
		float dynamic_friction = 0.0f;
		glm::vec3 g = glm::vec3(0, -.0981f, 0);
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
			float d1 = -(1 - e) * d;
			glm::vec3 n_ = n - dF * t;
			glm::vec3 d2 = glm::translate(n) * (K * glm::vec4(n_.x, n_.y, n_.z, 0.0f));
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

			linearMomentum += (float)event.dt * mass * force;
			applyFriction();
			if (gravity)
				applyGravity(event);
			eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), linearMomentum));
		}

		void applyRotation(veEvent event) {
			inertiaTensor = glm::mat3(
				1.0f, -1.0f, -1.0f,
				-1.0f, 1.0f - 1.0f,
				-1.0f, -1.0f, 1.0f, 0.0f); //assume interia tensor is 1

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
			glm::vec3 positionCube0 = getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->getPosition();
			glm::mat4 rotationCube0 = getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->getTransform();
			glm::vec3 positionPlane = getSceneManagerPointer()->getSceneNode("The Plane")->getPosition();


			vpe::Box cube0{ positionCube0 };
			vpe::Box plane{ positionPlane, scale(mat4(1.0f), vec3(100.0f, 1.0f, 100.0f)) };

			vec3 mtv(0, 1, 0); //minimum translation vector
			mtv = glm::normalize(force + g) * -1;
			//cube0.m_matRS = rotate;

			bool hit = vpe::collision(cube0, plane, mtv);


			if (hit) {
				std::set<vpe::contact> ct;
				//vec3 mtv(1, 0, 0);
				vec3 mtv(0, 1, 0); //minimum translation vector
				mtv = glm::normalize(force + g) * -1;
				//glm::vec3 n = glm::normalize(g);

				vpe::contacts(cube0, plane, mtv, ct);

				/*
				======================================================================================================================================================
																			 Task 7
				======================================================================================================================================================
				*/

				float e = 0.1f;
				float dF = 1.0f;

				//resolv interpenetration
				getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->setPosition(glm::vec3(positionCube0.x, positionCube0.y + 1.0f, positionCube0.z));
				//assume postion as center
				glm::vec3 cA = getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->getPosition();

				glm::vec3 f_ = glm::vec3();
				glm::vec3 rA = glm::vec3();
				glm::vec3 rB = glm::vec3();
				std::set<vpe::contact>::iterator itr;
				for (itr = ct.begin(); itr != ct.end(); itr++) {
					glm::vec3 rA_ = itr->pos - cA;
					glm::vec3 rB_ = itr->pos - positionPlane;
					glm::vec3 f_part = fHat(force + g, glm::vec3(0.0f), angularVelocity, glm::vec3(0.0f), mass, 1, rA_, rB_, inertiaTensor, inertiaTensor, e, itr->normal, dF);
					rA += rA_;
					rB += rB_;
					f_ += f_part;
				}
				f_ /= ct.size();
				rA /= ct.size();
				rB /= ct.size();
				force = f_+g;
				linearMomentum += f_;
				angularMomentum += glm::cross(rA, f_);

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
			applyRotation(event);
			applyMovement(event, gravity);
			checkCollision();
			dampenForce(0.0002f, force);
		};

		virtual bool onKeyboard(veEvent event) {
			if (event.idata3 == GLFW_RELEASE) return false;

			if (event.idata1 == GLFW_KEY_SPACE && event.idata3 == GLFW_PRESS) {
				if (cube_spawned) {
					force += glm::vec3(0.02f, 0.0f, 0.0f);
					gravity = true;
					rotSpeed = (float)d(e);
				}
				else {
					gravity = false;
					getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->setPosition(glm::vec3(-5.0f, 5.0f, 10.0f));
					//getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->setTransform(glm::mat4(1.0f));
					linearMomentum = glm::vec3();
					angularMomentum = glm::vec4();
					angularVelocity = glm::vec3();;
					inertiaTensor = glm::mat3();
					force = glm::vec3();
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
			inertiaTensor = glm::mat3();
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

			registerEventListener(new EventListenerCollision("Collision"), { veEvent::VE_EVENT_FRAME_STARTED });
			registerEventListener(new EventListenerKeyboard("Keyboard"), { veEvent::VE_EVENT_FRAME_STARTED, veEvent::VE_EVENT_KEYBOARD });
			registerEventListener(new EventListenerGUI("GUI"), { veEvent::VE_EVENT_DRAW_OVERLAY });
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

