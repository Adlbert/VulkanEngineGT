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
	int g_tries = 0;
	int g_max_tries = 10;
	bool g_gameLost = false;			//true... das Spiel wurde verloren
	bool g_restart = false;			//true...das Spiel soll neu gestartet werden
	glm::vec2 rotation_pos;
	float pressed_force;


	static std::default_random_engine e{ 12345 };					//F�r Zufallszahlen
	static std::uniform_real_distribution<> d{ -10.0f, 10.0f };		//F�r Zufallszahlen

	class EventListenerGUI : public VEEventListener {
	private:
		const struct nk_vec2 ball_position = nk_vec2(20, 140);
		const float ball_height = 100;
		const float ball_width = 100;
		//Let it be a little off
		struct nk_vec2 rot_pos = nk_vec2(19 + ball_width / 2, 139 + ball_height / 2);

	protected:

		virtual void onDrawOverlay(veEvent event) {
			VESubrenderFW_Nuklear* pSubrender = (VESubrenderFW_Nuklear*)getRendererPointer()->getOverlay();
			if (pSubrender == nullptr) return;

			struct nk_context* ctx = pSubrender->getContext();
			struct nk_command_buffer* canvas;

			if (!g_gameLost) {
				if (nk_begin(ctx, "", nk_rect(0, 0, 200, 270), NK_WINDOW_BORDER)) {
					canvas = nk_window_get_canvas(ctx);
					char outbuffer[100];

					nk_layout_row_dynamic(ctx, 45, 1);
					sprintf(outbuffer, "Score: %03d", g_score);
					nk_label(ctx, outbuffer, NK_TEXT_LEFT);

					nk_layout_row_dynamic(ctx, 45, 1);
					sprintf(outbuffer, "Tries:%02d / %02d", g_tries, g_max_tries);
					nk_label(ctx, outbuffer, NK_TEXT_LEFT);

					struct nk_rect ball = nk_rect(ball_position.x, ball_position.y, ball_height, ball_width);
					nk_fill_circle(canvas, ball, nk_rgb(255, 255, 255));
					if (nk_input_any_mouse_click_in_rect(&ctx->input, ball)) {
						rot_pos = *(&ctx->input.mouse.pos);
					}
					rotation_pos = glm::vec2(rot_pos.x - ball_position.x - ball_width / 2, rot_pos.y - ball_position.y - ball_height / 2);
					rotation_pos /= 50;

					struct nk_rect circle = nk_rect(rot_pos.x, rot_pos.y, 15, 15);
					nk_fill_circle(canvas, circle, nk_rgb(255, 0, 0));

					nk_fill_rect(canvas, nk_rect(150, 140, 20, 100), 0, nk_rgb(255, 0, 0));
					float height = pressed_force * 20;
					nk_fill_rect(canvas, nk_rect(150, 140, 20, height), 0, nk_rgb(0, 255, 0));
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
		EventListenerGUI(std::string name) : VEEventListener(name) {
		};

		///Destructor of class EventListenerGUI
		virtual ~EventListenerGUI() {};
	};

	class EventListenerKeyboard : public VEEventListener {
		glm::vec3 linearMomentum;
		const float max_pressed_force = 5.0f;
		glm::vec3 direction;
		glm::vec3 force;
		glm::vec3 angularMomentum;
		glm::vec3 angularMomentumKeeper;
		float rotSpeed;
		const float mass = 1.0f;
		const float static_friction = 0.0005f;
		const float dynamic_friction_air = 0.0000000000001f;
		const float dynamic_friction_grass = 0.000000001f;
		glm::vec3 g = glm::vec3(0, -0.0981f, 0);
		glm::vec3 angularVelocity;
		glm::mat3 inertiaTensor;
		bool gravity = false;
		bool cube_spawned = true;
		bool apply = true;
		int contact_count = 0;
		glm::vec3 impactPosition;
		glm::mat4 keeperStdTransform;
		glm::mat4 keeperParentStdTransform;
		float keeperSpeed;

		//Balancing
		const float maxDistance = 100.0f;
		const float keeperSpeed_Std = 28.0f;

	private:

		glm::vec3 calclualteBallImpact(veEvent event) {
			VESceneNode* ballParent = getSceneManagerPointer()->getSceneNode("The Ball Parent");
			glm::mat4 ballTransform = ballParent->getTransform();
			int maxLoops = 100;
			int loops = 0;
			glm::vec3 momentum = glm::vec3();
			glm::vec3 m_force = force;
			while (ballTransform[3][2] < 75 && loops < maxLoops) {
				momentum += (g + m_force);
				ballTransform *= glm::translate(glm::mat4(1.0f), momentum);
				dampenForce(0.1f, m_force);
				loops++;
			}
			float y = glm::max(ballTransform[3][1] - 5.0f, 1.0f);
			y = glm::min(y, 6.0f);
			float x = glm::min(ballTransform[3][0], 8.0f);
			x = glm::max(-8.0f, x);
			return glm::vec3(x, y, ballTransform[3][2]);
		}

		void moveKI(veEvent event) {
			float rotSpeedKeeper = 1.0f;
			VESceneNode* ballParent = getSceneManagerPointer()->getSceneNode("The Ball Parent");
			VESceneNode* keeperParent = getSceneManagerPointer()->getSceneNode("The Keeper Parent");
			VESceneNode* e1 = getSceneManagerPointer()->getSceneNode("The Keeper");
			glm::vec3 keeperPosition = keeperParent->getPosition(); //in Local Space
			if (impactPosition.x < 0)
				rotSpeedKeeper *= -1;
			if (impactPosition.y < 2.5)
				rotSpeedKeeper *= 2.5;
			glm::vec3 keeperForce = glm::normalize(impactPosition - keeperPosition);
			glm::vec3 keeperMomentum = keeperForce * event.dt * keeperSpeed;
			//only move on goal line
			keeperMomentum.z = 0;

			float distance = glm::distance(ballParent->getPosition(), keeperPosition);
			if (distance < maxDistance) {
				keeperParent->multiplyTransform(glm::translate(glm::mat4(1.0f), keeperMomentum));
			}

			//Assume that the objects center is its postion
			glm::vec4 center = glm::vec4(keeperPosition.x, keeperPosition.y, keeperPosition.z, 1);

			glm::mat4 orientation = keeperParent->getTransform(); //in Local 
			glm::vec4 c = orientation * center;
			// Kreuzprodukt von (dem Produkt zwischen orientierungsmatrix und center vector) und dem force vector.
			glm::vec3 torque = glm::cross(glm::vec3(c.x, c.y, c.z), keeperForce);
			// mass * velocity; mass = 1;
			angularMomentumKeeper += (float)event.dt * torque; //es selbst plus delta time mal torque.
			//orientierungsmatrix mal dem inversen inertia tensor mal der transponierten orientierungsmatrix mal dem angular momentum.
			glm::vec3 angularVelocity = orientation * glm::mat4(inertiaTensor) * glm::transpose(orientation) * glm::vec4(angularMomentumKeeper.x, angularMomentumKeeper.y, angularMomentumKeeper.z, 1);

			glm::vec4 rot4 = glm::vec4(1.0);
			float angle = rotSpeedKeeper * (float)event.dt;
			glm::vec3 rot_axis = glm::vec3(0.0, 0.0, 1.0) * -1.0f;
			rot4 = e1->getTransform() * glm::vec4(rot_axis.x, rot_axis.y, rot_axis.z, 1.0);
			glm::vec3  rot3 = glm::vec3(rot4.x, rot4.y, rot4.z);

			rot3 += float(event.dt) * glm::matrixCross3(angularVelocity) * rot3;
			glm::mat4  rotate = glm::rotate(glm::mat4(1.0), angle, rot3);
			e1->multiplyTransform(rotate);
		}

		//m .. mass
		glm::mat3 get_K(float mA, float mB, glm::vec3 rA, glm::vec3 rB, glm::mat3 itA, glm::mat3 itB) {
			glm::mat3 a = (-1) * (1 / mB) * glm::mat3(1);
			glm::mat3 b = glm::matrixCross3(rB) * glm::inverse(itB) * glm::matrixCross3(rB);
			glm::mat3 c = (-1) * (1 / mA) * glm::mat3(1);
			glm::mat3 d = glm::matrixCross3(rA) * glm::inverse(itA) * glm::matrixCross3(rA);

			return a + b + c + d;
			//glm::mat4 lhs = glm::matrixCross3(rB) * glm::inverse(itB) * glm::matrixCross3(rB);
			//glm::mat4 rhs = glm::matrixCross3(rA) * glm::inverse(itA) * glm::matrixCross3(rA);
			//return (-1) * lhs + (-1) * rhs;
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
		glm::vec3 get_mag_imp_dirN(float e, float d, glm::vec3 n, glm::mat3 K, float dF, glm::vec3 t) {
			float d1 = (-1) * (1 + e) * d;
			glm::vec3 n_ = n - dF * t;
			glm::vec3 d2 = n * K * n_;
			return d1 / d2;
		}

		glm::vec3 fHat(glm::vec3 lvA, glm::vec3 lvB, glm::vec3 aVA, glm::vec3 aVB, float mA, float mB, glm::vec3 rA, glm::vec3 rB, glm::mat3 itA, glm::mat3 itB, float e, glm::vec3 n, float dF) {
			glm::vec3 prel = get_prel(lvA, lvB, aVA, aVB, rA, rB);
			float d = get_d(n, prel);
			return fHat(d, prel, linearMomentum, lvB, angularVelocity, aVB, mass, 1, rA, rB, inertiaTensor, inertiaTensor, e, n, dF);
		}

		glm::vec3 fHat(float d, glm::vec3 prel, glm::vec3 lvA, glm::vec3 lvB, glm::vec3 aVA, glm::vec3 aVB, float mA, float mB, glm::vec3 rA, glm::vec3 rB, glm::mat3 itA, glm::mat3 itB, float e, glm::vec3 n, float dF) {
			glm::mat3 K = get_K(mA, mB, rA, rB, itA, itB);
			//glm::vec3 t = glm::vec3(0, 0, 0);
			//if (glm::length(aVA) != 0 || glm::length(aVB) != 0)
			//	t = get_tangetial_velocity(lvA, lvB, aVA, aVB, rA, rB, n);
			//else
			//	return glm::vec3(0, 0, 0);
			glm::vec3 t = glm::normalize(prel - d * n);
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
			VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Ball Parent");

			linearMomentum -= getFF(static_friction);
			if (glm::length(linearMomentum) > 0) {
				if (eParent->getPosition().y > 1.1f) {
					linearMomentum -= getFF(dynamic_friction_air);
				}
				else {
					linearMomentum -= getFF(dynamic_friction_grass);
				}
			}
		}

		void applyGravity(veEvent event) {
			VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Ball Parent");

			//Assume Object is in the air if y is above y
			if (eParent->getPosition().y > 1.1f) {
				linearMomentum += (float)event.dt * mass * g;
			}
		}

		void applyMovement(veEvent event, bool gravity) {
			VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Ball Parent");

			if (gravity) {
				if (eParent->getPosition().y > 0.6f) {
					linearMomentum += (float)event.dt * mass * (g + force);
				}
			}
			else {
				linearMomentum += (float)event.dt * mass * force;
			}
			//applyFriction();
			eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), linearMomentum));
		}

		void applyRotation(veEvent event) {
			rotSpeed = glm::distance(rotation_pos, glm::vec2(0, 0));
			if (rotSpeed <= 0)
				return;

			VESceneNode* eParent = getSceneManagerPointer()->getSceneNode("The Ball Parent");
			VESceneNode* e1 = getSceneManagerPointer()->getSceneNode("The Ball");

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
			glm::vec2 rot_axis = glm::normalize(rotation_pos) * -1;
			rot4 = e1->getTransform() * glm::vec4(rot_axis.x, rot_axis.y, 0.0, 1.0);
			glm::vec3  rot3 = glm::vec3(rot4.x, rot4.y, rot4.z);

			rot3 = rot3 + float(event.dt) * glm::matrixCross3(angularVelocity) * rot3;
			glm::mat4  rotate = glm::rotate(glm::mat4(1.0), angle, rot3);
			e1->multiplyTransform(rotate);
		}

		void resolveContacts(std::set<vpe::contact> contacts) {
			std::set<vpe::contact>::iterator itr;
			glm::vec3 f_hat = glm::vec3(0.0f);
			std::cout << glm::to_string(getSceneManagerPointer()->getSceneNode("The Ball Parent")->getPosition()) << std::endl;
			getSceneManagerPointer()->getSceneNode("The Ball Parent")->multiplyTransform(glm::translate(glm::mat4(1.0f), linearMomentum * -20));
			std::cout << glm::to_string(getSceneManagerPointer()->getSceneNode("The Ball Parent")->getPosition()) << std::endl;
			float e = 0.4f;
			float dF = 0.2;
			int used_contacts = 0;

			std::cout << "force" << glm::to_string(force) << std::endl;
			std::cout << "linearMomentum" << glm::to_string(linearMomentum) << std::endl;
			std::cout << "angularMomentum" << glm::to_string(angularMomentum) << std::endl;
			std::cout << "::::::::::::::::::" << std::endl;

			for (itr = contacts.begin(); itr != contacts.end(); itr++) {
				////assume postion as center
				glm::vec3 rA = itr->obj1->pos() - itr->pos;
				glm::vec3 rB = itr->obj2->pos() - itr->pos;
				//rA = glm::normalize(rA);
				//rB = glm::normalize(rB);
				//rA = itr->obj1->posW2L(rA);
				//rB = itr->obj1->posW2L(rB);
				//glm::vec3 rB_l_2 = itr->obj2->posW2L(rB);
				//glm::vec3 rB_w_2 = itr->obj2->posW2L(rB);
				//glm::vec3 rB_l_1 = itr->obj1->posW2L(rB);
				//glm::vec3 rB_w_1 = itr->obj1->posW2L(rB);
				glm::vec3 lvB = glm::vec3(0, 0, 0);
				glm::vec3 aVB = glm::vec3(0, 0, 0);
				glm::vec3 n = itr->normal;

				//relative velocity
				glm::vec3 prel = get_prel(linearMomentum, lvB, angularMomentum, aVB, rA, rB);
				//Closing Velocity
				float d = get_d(n, prel);
				//resting contacts
				if (d == 0) {
					f_hat += fHat(d, prel, linearMomentum, lvB, angularMomentum, aVB, mass, 1, rA, rB, inertiaTensor, inertiaTensor, e, n, dF) / contacts.size();
					used_contacts++;
				}
				//seperating contacts
				if (d > 0) {
					f_hat += fHat(d, prel, linearMomentum, lvB, angularMomentum, aVB, mass, 1, rA, rB, inertiaTensor, inertiaTensor, e, n, dF) / contacts.size();
					used_contacts++;
				}
			}
			f_hat /= 1000;
			contact_count++;
			linearMomentum += f_hat;
			std::cout << "angularVelocity" << glm::to_string(angularMomentum) << std::endl;
			std::cout << "force" << glm::to_string(force) << std::endl;
			std::cout << "__________________" << std::endl;
		}

		void checkCollision() {
			if (contact_count > 40) {
				contact_count = 0;
				respawn();
				g_tries++;
			}
			glm::vec3 positionBall = getSceneManagerPointer()->getSceneNode("The Ball Parent")->getTransform()[3];
			glm::mat4 transformBall = getSceneManagerPointer()->getSceneNode("The Ball Parent")->getTransform();
			glm::vec3 positionPlane = glm::vec3(getSceneManagerPointer()->getSceneNode("The Goal Parent")->getPosition());
			glm::vec3 positionGoal = getSceneManagerPointer()->getSceneNode("The Goal Parent")->getTransform()[3];
			glm::vec3 positionKeeper = getSceneManagerPointer()->getSceneNode("The Keeper Parent")->getPosition();
			glm::mat4 transformKeeper = getSceneManagerPointer()->getSceneNode("The Keeper Parent")->getTransform();

			positionPlane = positionBall;
			positionPlane.y = 0;

			vpe::Box ball{ positionBall, transformBall };
			vpe::Box goal{ positionGoal + glm::vec3(7.5, 0, 0) , scale(mat4(1.0f), vec3(15.0f, 12.0f, 1.0f)) };
			vpe::Box leftBar{ positionGoal + glm::vec3(0,0,0), scale(mat4(1.0f), vec3(0.5f, 12.0f, 1.0f)) };
			vpe::Box rightBar{ positionGoal + glm::vec3(15,0,0), scale(mat4(1.0f), vec3(0.5f, 12.0f, 1.0f)) };
			vpe::Box topBar{ positionGoal + glm::vec3(7.5,7.5,0), scale(mat4(1.0f), vec3(16, 1.0f, 1)) };
			vpe::Box keeper{ positionKeeper, transformKeeper };
			vpe::Box plane{ positionPlane, scale(mat4(1.0f), vec3(1000.0f, 1.0f, 1000.0f)) };

			std::set<vpe::contact> contacts;
			vec3 mtv(0, 1, 0); //minimum translation vector

			bool hit_plane = vpe::collision(ball, plane, mtv);
			if (hit_plane) {
				vpe::contacts(ball, plane, mtv, contacts);
				std::cout << "hit_plane" << contacts.size() << std::endl;
			}

			bool hit_goal = vpe::collision(ball, goal, mtv);
			bool hit_leftBar = vpe::collision(ball, leftBar, mtv);
			bool hit_rightBar = vpe::collision(ball, rightBar, mtv);
			bool hit_topBar = vpe::collision(ball, topBar, mtv);
			bool hit_keeper = vpe::collision(ball, keeper, mtv);
			bool hit_any = hit_plane || hit_leftBar || hit_rightBar || hit_topBar || hit_keeper;

			if (glm::distance(glm::vec3(0.0f, 1.1f, 21.0f), positionBall) > 90) {
				g_tries++;
				respawn();
				return;
			}

			if (hit_keeper) {
				vpe::contacts(ball, keeper, mtv, contacts);
				std::cout << "hit_keeper" << contacts.size() << std::endl;
				respawn();
			}
			if (hit_leftBar) {
				vpe::contacts(ball, leftBar, mtv, contacts);
				std::cout << "hit_leftBar" << contacts.size() << std::endl;
				respawn();
			}
			if (hit_rightBar) {
				vpe::contacts(ball, rightBar, mtv, contacts);
				std::cout << "hit_rightBar" << contacts.size() << std::endl;
				respawn();
			}
			if (hit_topBar) {
				vpe::contacts(ball, topBar, mtv, contacts);
				std::cout << "hit_topBar" << contacts.size() << std::endl;
				respawn();
			}

			if (contacts.size() > 0)
				resolveContacts(contacts);
			else if (!hit_any && hit_goal) {
				respawn();
				g_tries++;
				g_score++;
				std::cout << "hit_goal" << std::endl;
				std::cout << glm::to_string(positionBall) << std::endl;
				std::cout << glm::to_string(impactPosition) << std::endl;
				return;
			}
		}

		void respawn() {
			gravity = false;
			getSceneManagerPointer()->getSceneNode("The Ball Parent")->setPosition(glm::vec3(0.0f, 1.1f, 21.0f));
			getSceneManagerPointer()->getSceneNode("The Keeper Parent")->setTransform(keeperParentStdTransform);
			getSceneManagerPointer()->getSceneNode("The Keeper")->setTransform(keeperStdTransform);
			linearMomentum = glm::vec3(0.0f);
			angularMomentum = glm::vec4(0.0f);
			angularMomentumKeeper = glm::vec4(0.0f);
			angularVelocity = glm::vec3(0.0f);
			force = glm::vec3(0.0f);
			rotSpeed = 0;
			apply = false;
			//Some randomg factor to the keeper movement
			keeperSpeed = keeperSpeed_Std;
			keeperSpeed += d(e);
		}

	protected:
		virtual void onFrameStarted(veEvent event) {
			if (g_tries == g_max_tries) {
				g_gameLost = true;
			}
			if (g_restart) {
				g_restart = false;
				g_gameLost = false;
				respawn();
				g_tries = 0;
				g_score = 0;
			}
			checkCollision();
			if (apply) {
				applyRotation(event);
				applyMovement(event, gravity);
				dampenForce(0.1f, force);
				moveKI(event);
			}
		};

		virtual bool onKeyboard(veEvent event) {
			if (event.idata1 == GLFW_KEY_SPACE && event.idata3 == GLFW_RELEASE) {
				if (cube_spawned) {
					VECamera* c = getSceneManagerPointer()->getCamera();
					direction = getSceneManagerPointer()->getCamera()->getWorldTransform()[2];
					force += pressed_force * direction;
					gravity = true;
					impactPosition = calclualteBallImpact(event);
					//rotSpeed = 1.5f;
					//rotSpeed = (float)d(e) / 10;
					//rotSpeed = 0.0f;
					apply = true;
					std::system("CLS");
				}
				else {
					respawn();
				}
				//cube_spawned = !cube_spawned;
				pressed_force = 0;
				return true;
			}

			if (event.idata1 == GLFW_KEY_SPACE && (event.idata3 == GLFW_PRESS || event.idata3 == GLFW_REPEAT)) {
				pressed_force += event.dt * 10;
				if (pressed_force > max_pressed_force)
					pressed_force = max_pressed_force;
			}
			if (event.idata1 == GLFW_KEY_R && event.idata3 == GLFW_PRESS) {
				respawn();
			}
			return false;
		}

	public:
		///Constructor of class EventListenerCollision
		EventListenerKeyboard(std::string name) : VEEventListener(name) {
			pressed_force = 0;
			linearMomentum = glm::vec3();
			angularMomentum = glm::vec4();
			angularVelocity = glm::vec3();;
			inertiaTensor = glm::mat3(
				1.0f, -1.0f, -1.0f,
				-1.0f, 1.0f - 1.0f,
				-1.0f, -1.0f, 1.0f, 0.0f); //assume interia tensor is 1
			force = glm::vec3();
			rotSpeed = 0;
			apply = false;
			keeperStdTransform = getSceneManagerPointer()->getSceneNode("The Keeper")->getTransform();
			keeperParentStdTransform = getSceneManagerPointer()->getSceneNode("The Keeper Parent")->getTransform();
			keeperSpeed = keeperSpeed_Std;
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

			registerEventListener(new EventListenerKeyboard("Ball"), { veEvent::VE_EVENT_FRAME_STARTED, veEvent::VE_EVENT_KEYBOARD });
			registerEventListener(new EventListenerGUI("GUI"), { veEvent::VE_EVENT_DRAW_OVERLAY });
		};


		///Load the first level into the game engine
		///The engine uses Y-UP, Left-handed
		virtual void loadLevel(uint32_t numLevel = 1) {


			VESceneNode* pScene;
			VECHECKPOINTER(pScene = getSceneManagerPointer()->createSceneNode("Level 1", getRoot()));

			//scene models

			//VESceneNode* sp1;
			//VECHECKPOINTER(sp1 = getSceneManagerPointer()->createSkybox("The Sky", "media/models/test/sky/cloudy",
			//	{ "bluecloud_ft.jpg", "bluecloud_bk.jpg", "bluecloud_up.jpg",
			//		"bluecloud_dn.jpg", "bluecloud_rt.jpg", "bluecloud_lf.jpg" }, pScene));

			VESceneNode* e4;
			VECHECKPOINTER(e4 = getSceneManagerPointer()->loadModel("The Plane", "media/models/test", "plane_t_n_s.obj", 0, pScene));
			e4->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f, 1.0f, 1000.0f)));

			VEEntity* pE4;
			VECHECKPOINTER(pE4 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Plane/plane_t_n_s.obj/plane/Entity_0"));
			pE4->setParam(glm::vec4(1000.0f, 1000.0f, 0.0f, 0.0f));

			VESceneNode* b1, * b1Parent;
			b1Parent = getSceneManagerPointer()->createSceneNode("The Ball Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(b1 = getSceneManagerPointer()->loadModel("The Ball", "media/models/game/", "ball.obj"));
			b1->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.6f, 0.6f)));
			b1Parent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.1f, 21.0f)));
			b1Parent->addChild(b1);

			VESceneNode* g1, * g1Parent;
			g1Parent = getSceneManagerPointer()->createSceneNode("The Goal Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(g1 = getSceneManagerPointer()->loadModel("The Goal", "media/models/game/", "goal.obj"));
			g1->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.03f, 0.03f, 0.03f)));
			//goal seems to be 15 width
			//goal seems to be 5 height
			g1Parent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-7.5f, 0.55f, 75.0f)));
			g1Parent->addChild(g1);

			VESceneNode* k1, * k1Parent;
			k1Parent = getSceneManagerPointer()->createSceneNode("The Keeper Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(k1 = getSceneManagerPointer()->loadModel("The Keeper", "media/models/test/", "cube1.obj"));
			k1->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(2, 4, 1)));
			//goal seems to be 15 width
			//goal seems to be 5 height
			k1Parent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 70.0f)));
			k1Parent->addChild(k1);

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

