/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"

#include "sat.h"
#include "gjk_epa.h"
#include "contact.h"

namespace ve {


	uint32_t g_score = 0;				//derzeitiger Punktestand
	double g_time = 30.0;				//zeit die noch übrig ist
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


	static std::default_random_engine e{ 12345 };					//Für Zufallszahlen
	static std::uniform_real_distribution<> d{ -10.0f, 10.0f };		//Für Zufallszahlen

	//
	// Überprüfen, ob die Kamera die Kiste berührt
	//
	class EventListenerCollision : public VEEventListener {
	protected:
		virtual void onFrameStarted(veEvent event) {
			static uint32_t cubeid = 0;

			glm::vec3 positionCube0 = getSceneManagerPointer()->getSceneNode("The Cube0 Parent")->getPosition();
			glm::vec3 positionCube1 = getSceneManagerPointer()->getSceneNode("The Plane")->getPosition();


			gjk::Box cube0{ positionCube0 };
			gjk::Box cube1{ positionCube1 };
			vec3 mtv(1, 0, 0); //minimum translation vector

			bool hit = gjk::sat(cube0, cube1, mtv);


			if (hit) {
				std::set<gjk::contact> ct;
				vec3 mtv(1, 0, 0);
				gjk::contacts(cube0, cube1, mtv, ct);

				getEnginePointer()->m_irrklangEngine->removeAllSoundSources();
				getEnginePointer()->m_irrklangEngine->play2D("media/sounds/gameover.wav", false);
			}
		};

	public:
		///Constructor of class EventListenerCollision
		EventListenerCollision(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerCollision
		virtual ~EventListenerCollision() {};
	};

	//
// Überprüfen, ob die Kamera die Kiste berührt
//
	class EventListenerKeyboard : public VEEventListener {
		glm::vec3 linearMomentum;
		glm::vec3 force;
		glm::vec3 angularMomentum;
		float rotSpeed;

	protected:
		virtual void onFrameStarted(veEvent event) {
			VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Cube0 Parent");
			VESceneNode* e1 = getSceneManagerPointer()->getSceneNode("The Cube0");

			glm::vec3 position = eParent->getPosition(); //in Local Space
			glm::mat4 orientation = eParent->getTransform(); //in Local 
			//Assume that the objects center is its postion
			glm::vec4 center = glm::vec4(position.x, position.y, position.z, 1);
			float inertiaTensor = 1.0f; //assume interia tensor is 1
			float mass = 1.0f;

			// Kreuzprodukt von (dem Produkt zwischen orientierungsmatrix und center vector) und dem force vector.
			glm::vec4 c = orientation * center;
			glm::vec3 torque = glm::cross(glm::vec3(c.x, c.y, c.z), force);
			// mass * velocity; mass = 1;
			linearMomentum += (float)event.dt * mass * force;
			angularMomentum += (float)event.dt * torque; //es selbst plus delta time mal torque.
			//orientierungsmatrix mal dem inversen inertia tensor mal der transponierten orientierungsmatrix mal dem angular momentum.
			glm::vec3 angularVelocity = orientation * inertiaTensor * glm::transpose(orientation) * glm::vec4(angularMomentum.x, angularMomentum.y, angularMomentum.z, 1);

			glm::vec4 rot4 = glm::vec4(1.0);
			float angle = rotSpeed * (float)event.dt;
			rot4 = e1->getTransform() * glm::vec4(0.0, 0.1, 0.0, 1.0);
			glm::vec3  rot3 = glm::vec3(rot4.x, rot4.y, rot4.z);

			rot3 = rot3 + float(event.dt) * glm::matrixCross3(angularVelocity) * rot3;
			glm::mat4  rotate = glm::rotate(glm::mat4(1.0), angle, rot3);

			eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), linearMomentum));
			e1->multiplyTransform(rotate);
			//force += (force/10)*(-1);
			force = glm::vec3(0, 0, 0);
		};

		virtual bool onKeyboard(veEvent event) {
			if (event.idata3 == GLFW_RELEASE) return false;

			if (event.idata1 == GLFW_KEY_SPACE && event.idata3 == GLFW_PRESS) {
				force = glm::vec3(0, -1, 0);
				rotSpeed = d(e);

			}
		}

	public:
		///Constructor of class EventListenerCollision
		EventListenerKeyboard(std::string name) : VEEventListener(name) {
			linearMomentum = glm::vec3(0, 0, 0);
			angularMomentum = glm::vec4(0, 0, 0, 0);
			force = glm::vec3(0, 0, 0);
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

