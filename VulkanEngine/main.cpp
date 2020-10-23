/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"



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
			VESubrenderFW_Nuklear* pSubrender = (VESubrenderFW_Nuklear*)getRendererPointer()->getOverlay();
			if (pSubrender == nullptr) return;

			struct nk_context* ctx = pSubrender->getContext();

			if (!g_gameLost) {
				if (nk_begin(ctx, "", nk_rect(0, 0, 200, 170), NK_WINDOW_BORDER)) {
					char outbuffer[100];
					nk_layout_row_dynamic(ctx, 45, 1);
					sprintf(outbuffer, "Score: %03d", g_score);
					nk_label(ctx, outbuffer, NK_TEXT_LEFT);

					nk_layout_row_dynamic(ctx, 45, 1);
					sprintf(outbuffer, "Time: %004.1lf", g_time);
					nk_label(ctx, outbuffer, NK_TEXT_LEFT);
				}
			}
			else {
				if (nk_begin(ctx, "", nk_rect(500, 500, 200, 170), NK_WINDOW_BORDER)) {
					nk_layout_row_dynamic(ctx, 45, 1);
					nk_label(ctx, "Game Over", NK_TEXT_LEFT);
					if (nk_button_label(ctx, "Restart")) {
						g_restart = true;
					}
				}

			};

			nk_end(ctx);
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
	// Manage spring force
	//
	class EventListenerFriction : public VEEventListener {

		const float g = 9.81;
		float cube0speed;
		float cube1speed;
		float cube2speed;
		float cube3speed;

		float getFF(float speed, float friction) {
			float ff = g * friction;
			if (speed <= ff)
				ff = -speed;
			return ff;
		}

	protected:
		virtual void onFrameEnded(veEvent event) {
			VESceneNode* cube0 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Cube Parent0");
			VESceneNode* cube1 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Cube Parent1");
			VESceneNode* cube2 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Cube Parent2");
			VESceneNode* cube3 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Cube Parent3");

			glm::vec3 trans;
			glm::vec4 translate = glm::vec4(0.0, 0.0, 1.0, 0.0);

			trans = cube0speed * glm::vec3(translate.x, translate.y, translate.z);
			cube0->multiplyTransform(glm::translate(glm::mat4(1.0f), (float)event.dt * trans));

			trans = cube1speed * glm::vec3(translate.x, translate.y, translate.z);
			cube1->multiplyTransform(glm::translate(glm::mat4(1.0f), (float)event.dt * trans));

			trans = cube2speed * glm::vec3(translate.x, translate.y, translate.z);
			cube2->multiplyTransform(glm::translate(glm::mat4(1.0f), (float)event.dt * trans));

			trans = cube3speed * glm::vec3(translate.x, translate.y, translate.z);
			cube3->multiplyTransform(glm::translate(glm::mat4(1.0f), (float)event.dt * trans));
		};

		virtual bool onKeyboard(veEvent event) {
			if (event.idata3 == GLFW_RELEASE)
				return false;
			if (event.idata1 == GLFW_KEY_SPACE && event.idata3 == GLFW_PRESS) {
				cube0speed -= getFF(cube0speed, 0.94);
				cube1speed -= getFF(cube1speed, 0.1);
				cube2speed -= getFF(cube2speed, 0.3);
				cube3speed -= getFF(cube3speed, 0.7);
			}
			return true;
		}

	public:
		///Constructor of class EventListenerCollision
		EventListenerFriction(std::string name) : VEEventListener(name) {
			cube0speed = 6.0f;
			cube1speed = 6.0f;
			cube2speed = 6.0f;
			cube3speed = 6.0f;
		};

		///Destructor of class EventListenerCollision
		virtual ~EventListenerFriction() {};
	};



	///user defined manager class, derived from VEEngine
	class MyVulkanEngine : public VEEngine {
	public:

		MyVulkanEngine(bool debug = false) : VEEngine(debug) {};
		~MyVulkanEngine() {};


		///Register an event listener to interact with the user

		virtual void registerEventListeners() {
			VEEngine::registerEventListeners();

			//registerEventListener(new EventListenerGUI("GUI"), { veEvent::VE_EVENT_DRAW_OVERLAY});
			registerEventListener(new EventListenerFriction("Friction"), { veEvent::VE_EVENT_KEYBOARD, veEvent::VE_EVENT_FRAME_ENDED });
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

			VESceneNode* e0, * e0Parent;
			e0Parent = getSceneManagerPointer()->createSceneNode("The Cube Parent0", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e0 = getSceneManagerPointer()->loadModel("The Cube0", "media/models/test/crate0", "cube.obj"));
			e0Parent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-4.5f, 1.0f, 0.0f)));
			e0Parent->addChild(e0);

			VESceneNode* e1, * e1Parent;
			e1Parent = getSceneManagerPointer()->createSceneNode("The Cube Parent1", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("The Cube1", "media/models/test/crate0", "cube.obj"));
			e1Parent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 1.0f, 0.0f)));
			e1Parent->addChild(e1);

			VESceneNode* e2, * e2Parent;
			e2Parent = getSceneManagerPointer()->createSceneNode("The Cube Parent2", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e2 = getSceneManagerPointer()->loadModel("The Cube2", "media/models/test/crate0", "cube.obj"));
			e2Parent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 1.0f, 0.0f)));
			e2Parent->addChild(e2);

			VESceneNode* e3, * e3Parent;
			e3Parent = getSceneManagerPointer()->createSceneNode("The Cube Parent3", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e3 = getSceneManagerPointer()->loadModel("The Cube3", "media/models/test/crate0", "cube.obj"));
			e3Parent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(4.5f, 1.0f, 0.0f)));
			e3Parent->addChild(e3);


			//m_irrklangEngine->play2D("media/sounds/ophelia.wav", true);
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

