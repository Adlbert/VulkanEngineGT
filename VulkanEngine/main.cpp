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
	bool g_gameWon = false;
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

			if (!g_gameLost && !g_gameWon) {
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
					if (g_gameLost)
						nk_label(ctx, "Game Over", NK_TEXT_LEFT);
					if (g_gameWon)
						nk_label(ctx, "Game Won", NK_TEXT_LEFT);
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
	static std::uniform_real_distribution<> d{ -20.0f, 20.0f };		//Für Zufallszahlen



	//
	// Wirf die Kist in die richtung der Kamera
	//
	class EventListenerThrow : public VEEventListener {

	private:
		float force;
		glm::vec3 direction;
		bool charging;
		bool moving;

		int getScore(float distance) {
			int score = (-glm::pow(distance, 2)) + 1000;
			if (score < 0)
				score = 0;
			return score;
		}

	protected:
		virtual bool onKeyboard(veEvent event) {
			static uint32_t cubeid = 0;


			if (!moving && event.idata1 == GLFW_KEY_SPACE && event.idata3 == GLFW_PRESS) {
				charging = true;
				std::cout << "Space pressed" << std::endl;
				return false;
			}

			if (!moving && event.idata1 == GLFW_KEY_SPACE && event.idata3 == GLFW_RELEASE) {
				VECamera* camera = getSceneManagerPointer()->getCamera();
				direction = camera->getTransform()[2];
				std::cout << "Space released" << std::endl;
				std::cout << "Space released" + std::to_string(force) << std::endl;
				charging = false;
				return false;
			}


		};

		virtual void onFrameStarted(veEvent event) {

			if (g_restart) {
				g_gameLost = false;
				g_gameWon = false;
				g_restart = false;
				g_time = 30;
				g_score = 0;
				getSceneManagerPointer()->getSceneNode("The Cube Parent")->setPosition(glm::vec3(0.0f, 1.0f, 10.0f));
				getSceneManagerPointer()->getSceneNode("The Rabit Parent")->setPosition(glm::vec3(d(e), 1.0f, d(e)));
				return;
			}
			if (g_gameLost || g_gameWon) return;

			if (charging) {
				force += event.dt;
			}

			if (!charging && force > 0) {
				moving = true;
				VESceneNode* cubeParent = getSceneManagerPointer()->getSceneNode("The Cube Parent");
				VESceneNode* rabbitParent = getSceneManagerPointer()->getSceneNode("The Rabit Parent");

				VECamera* camera = getSceneManagerPointer()->getCamera();
				float distance = glm::length(cubeParent->getPosition() - rabbitParent->getPosition());
				g_score = getScore(distance);


				glm::vec3 eye = camera->getTransform()[0];
				glm::vec3 v = direction * force;
				cubeParent->multiplyTransform(glm::translate(glm::mat4(1.0f), v));
				force -= event.dt;
				if (force <= 0) {
					force = 0;
					moving = false;
				}
			}
			g_time -= event.dt;
			if (g_time <= 0 || g_score > 990) {
				if (g_score > 990)
					g_gameWon = true;
				else
					g_gameLost = true;
				getEnginePointer()->m_irrklangEngine->removeAllSoundSources();
				getEnginePointer()->m_irrklangEngine->play2D("media/sounds/gameover.wav", false);
			}

		}




	public:
		///Constructor of class EventListenerCollision
		EventListenerThrow(std::string name) : VEEventListener(name) {
			force = 0;
			direction = glm::vec3(0.0f, 0.0f, 0.0f);
			charging = false;
			moving = false;
		};

		///Destructor of class EventListenerCollision
		virtual ~EventListenerThrow() {};
	};



	///user defined manager class, derived from VEEngine
	class MyVulkanEngine : public VEEngine {
	public:

		MyVulkanEngine(bool debug = false) : VEEngine(debug) {};
		~MyVulkanEngine() {};


		///Register an event listener to interact with the user

		virtual void registerEventListeners() {
			VEEngine::registerEventListeners();

			registerEventListener(new EventListenerThrow("Throw"), { veEvent::VE_EVENT_KEYBOARD, veEvent::VE_EVENT_FRAME_STARTED });
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

			VESceneNode* e1, * eParent;
			eParent = getSceneManagerPointer()->createSceneNode("The Cube Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("The Cube0", "media/models/test/crate0", "cube.obj"));
			eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 10.0f)));
			eParent->addChild(e1);

			VESceneNode* rabbit1, * rabbitParent;
			eParent = getSceneManagerPointer()->createSceneNode("The Rabit Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("The Rabit", "media/models/test/crate1", "cube.obj"));
			eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(d(e), 1.0f, d(e))));
			eParent->addChild(e1);
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

