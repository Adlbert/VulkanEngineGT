/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"

#include "glm/gtx/matrix_cross_product.hpp"


namespace ve {


	//
	//Zeichne das GUI
	//
	class EventListenerGUI : public VEEventListener {
	protected:

		virtual void onDrawOverlay(veEvent event) {
			VESubrenderFW_Nuklear* pSubrender = (VESubrenderFW_Nuklear*)getRendererPointer()->getOverlay();
			if (pSubrender == nullptr) return;

			struct nk_context* ctx = pSubrender->getContext();
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
	class EventListenerThrowing : public VEEventListener {
		glm::vec3 angularMomentum;
		glm::vec3 linearMomentum;
		glm::vec3 force;
		float upward;
		bool cubeSpawned;
		uint32_t cubeId;

	protected:
		virtual void onFrameStarted(veEvent event) {
			if (!cubeSpawned)
				return;
			//could be input

			VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Cube Parent");
			VESceneNode* e1 = getSceneManagerPointer()->getSceneNode("The Cube" + std::to_string(cubeId));

			glm::vec3 position = eParent->getPosition(); //in Local Space
			glm::mat4 orientation = eParent->getTransform(); //in Local 
			//Assume that the objects center is its postion
			glm::vec4 center = glm::vec4(position.x, position.y, position.z, 1);
			float inertiaTensor = 1.0f; //assume interia tensor is 1
			float mass = 1.0f;

			//force -= glm::vec3(0, .01, 0);
			 
			//// Kreuzprodukt von (dem Produkt zwischen orientierungsmatrix und center vector) und dem force vector.
			glm::vec4 c = orientation * center;
			glm::vec3 torque = glm::cross(glm::vec3(c.x,c.y,c.z), force); 
			// mass * velocity; mass = 1;
			linearMomentum += (float)event.dt * mass * force;
			angularMomentum += (float)event.dt * torque; //es selbst plus delta time mal torque.
			//orientierungsmatrix mal dem inversen inertia tensor mal der transponierten orientierungsmatrix mal dem angular momentum.
			glm::vec3 angularVelocity = orientation * inertiaTensor * glm::transpose(orientation) * glm::vec4(angularMomentum.x, angularMomentum.y , angularMomentum.z, 1);

			glm::vec4 rot4 = glm::vec4(1.0);
			float rotSpeed = 10;
			float angle = rotSpeed * (float)event.dt;
			rot4 = e1->getTransform() * glm::vec4(0.0, 0.1, 0.0, 1.0);
			glm::vec3  rot3 = glm::vec3(rot4.x, rot4.y, rot4.z);

			rot3 = rot3 + float(event.dt) * glm::matrixCross3(angularVelocity) * rot3;
			glm::mat4  rotate = glm::rotate(glm::mat4(1.0), angle, rot3);

			//rotationsmatrix R =  R + dt * matrixCross3(angular velocity) * R die noch normalisiert werden muss.


			//float g = .81f;
			//glm::vec4 translate_gravity = glm::vec4(0.0, 0.0, 0.0, 1.0);
			//translate_gravity = e1->getTransform() * glm::vec4(0.0, -1.0, 0.0, 1.0);
			//glm::vec3 gravity = g * glm::vec3(translate_gravity.x, translate_gravity.y, translate_gravity.z);

			//glm::vec4 translate_upward = glm::vec4(0.0, 0.0, 0.0, 1.0);
			//translate_upward = e1->getTransform() * glm::vec4(0.0, 1.0, 0.0, 1.0);
			//glm::vec3 trans_upward = upward * glm::vec3(translate_upward.x, translate_upward.y, translate_upward.z);

			eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), linearMomentum));
			e1->multiplyTransform(rotate);

			force = glm::vec3(0,0,0);

			if (upward > 0)
				upward -= event.dt;
			else upward = 0;
		};

		virtual bool onKeyboard(veEvent event) {
			if (event.idata3 == GLFW_RELEASE) return false;

			if (event.idata1 == GLFW_KEY_SPACE && event.idata3 == GLFW_PRESS) {

				if (cubeSpawned) {
					getSceneManagerPointer()->deleteSceneNodeAndChildren("The Cube" + std::to_string(cubeId));
					cubeSpawned = false;
					cubeId++;
					upward = 18.0f;
					force = glm::vec3(1, 1, 1);
				}
				else {
					VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Cube Parent");
					VESceneNode* e1;
					VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(cubeId), "media/models/test/crate0", "cube.obj"));
					eParent->setPosition(glm::vec3(0.0f, 1.0f, 10.0f));
					eParent->addChild(e1);

					cubeSpawned = true;
				}
				return true;
			}
			return false;
		}

	public:
		///Constructor of class EventListenerCollision
		EventListenerThrowing(std::string name) : VEEventListener(name) {
			cubeSpawned = false;
			upward = 18.0f;
			cubeId = 0;
			angularMomentum = glm::vec4(0,0,0,0);
			linearMomentum = glm::vec3(0, 0, 0);
			force = glm::vec3(0, 0, 0);
		};

		///Destructor of class EventListenerCollision
		virtual ~EventListenerThrowing() {};
	};



	///user defined manager class, derived from VEEngine
	class MyVulkanEngine : public VEEngine {
	public:

		MyVulkanEngine(bool debug = false) : VEEngine(debug) {};
		~MyVulkanEngine() {};


		///Register an event listener to interact with the user

		virtual void registerEventListeners() {
			VEEngine::registerEventListeners();

			registerEventListener(new EventListenerThrowing("Throwing"), { veEvent::VE_EVENT_FRAME_STARTED, veEvent::VE_EVENT_KEYBOARD });
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

			VESceneNode* eParent;
			eParent = getSceneManagerPointer()->createSceneNode("The Cube Parent", pScene, glm::mat4(1.0));
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

