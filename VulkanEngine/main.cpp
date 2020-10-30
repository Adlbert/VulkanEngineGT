/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"



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
		float upward;
		bool cubeSpawned;
		uint32_t cubeId;

	protected:
		virtual void onFrameStarted(veEvent event) {
			if (!cubeSpawned)
				return;
			VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Cube Parent");
			VESceneNode* e1 = getSceneManagerPointer()->getSceneNode("The Cube" + std::to_string(cubeId));

			float g = 9.81f;
			glm::vec4 translate_gravity = glm::vec4(0.0, 0.0, 0.0, 1.0);
			translate_gravity = e1->getTransform() * glm::vec4(0.0, -1.0, 0.0, 1.0);
			glm::vec3 gravity = g * glm::vec3(translate_gravity.x, translate_gravity.y, translate_gravity.z);

			glm::vec4 translate_upward = glm::vec4(0.0, 0.0, 0.0, 1.0);
			translate_upward = e1->getTransform() * glm::vec4(0.0, 1.0, 0.0, 1.0);
			glm::vec3 trans_upward = upward * glm::vec3(translate_upward.x, translate_upward.y, translate_upward.z);

			float forward = 20.0;
			glm::vec4 translate_forward = glm::vec4(0.0, 0.0, 0.0, 1.0);
			translate_forward = e1->getTransform() * glm::vec4(0.0, 0.0, 1, 1.0);
			glm::vec3 trans_forward = forward * glm::vec3(translate_forward.x, translate_forward.y, translate_forward.z);


			glm::vec4 rot4 = glm::vec4(1.0);
			float rotSpeed = 0.1;
			float angle = rotSpeed * (float)event.dt;
			rot4 = e1->getTransform() * glm::vec4(0.0, 0.1, 0.0, 1.0);
			glm::vec3  rot3 = glm::vec3(rot4.x, rot4.y, rot4.z);
			glm::mat4  rotate = glm::rotate(glm::mat4(1.0), angle, rot3);

			eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), (float)event.dt * (trans_forward + gravity + trans_upward)));
			e1->multiplyTransform(rotate);

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
				}
				else {
					VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Cube Parent");
					VESceneNode* e1;
					VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(cubeId), "media/models/test/crate0", "cube.obj"));
					eParent->setPosition(glm::vec3(0.0f, 1.0f, 10.0f));
					//eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
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

