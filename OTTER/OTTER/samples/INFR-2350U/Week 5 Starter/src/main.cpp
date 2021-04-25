//Just a simple handler for simple initialization stuffs
#include "Utilities/BackendHandler.h"

#include <filesystem>
#include <json.hpp>
#include <fstream>

//TODO: New for this tutorial
#include <DirectionalLight.h>
#include <PointLight.h>
#include <UniformBuffer.h>
/////////////////////////////

#include <Texture2D.h>
#include <Texture2DData.h>
#include <MeshBuilder.h>
#include <MeshFactory.h>
#include <NotObjLoader.h>
#include <ObjLoader.h>
#include <VertexTypes.h>
#include <ShaderMaterial.h>
#include <RendererComponent.h>
#include <TextureCubeMap.h>
#include <TextureCubeMapData.h>

#include <Timing.h>
#include <GameObjectTag.h>
#include <InputHelpers.h>

#include <IBehaviour.h>
#include <CameraControlBehaviour.h>
#include <FollowPathBehaviour.h>
#include <SimpleMoveBehaviour.h>

int main() {
	int frameIx = 0;
	float fpsBuffer[128];
	float minFps, maxFps, avgFps;
	int selectedVao = 0; // select cube by default
	std::vector<GameObject> controllables;

	float t = 0.0f;
	float totalTime;

	float speed = 1.0f;

	glm::vec3 point1 = glm::vec3(-5.0f, -5.0f, 2.0f);
	glm::vec3 point2 = glm::vec3(-5.0f, -5.0f, 5.0f);

	glm::vec3 box2Point1 = glm::vec3(5.0f, -5.0f, 5.0f);
	glm::vec3 box2Point2 = glm::vec3(5.0f, -5.0f, 2.0f);

	bool forwards = true;

	float distance = glm::distance(point2, point1);

	totalTime = distance / speed;

	bool textureOn = true;
	int lightNum = 4;

	bool shaderChanged = false;

	BackendHandler::InitAll();

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(BackendHandler::GlDebugMessage, nullptr);

	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	// Push another scope so most memory should be freed *before* we exit the app
	{
		#pragma region Shader and ImGui
		Shader::sptr passthroughShader = Shader::Create();
		passthroughShader->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
		passthroughShader->LoadShaderPartFromFile("shaders/passthrough_frag.glsl", GL_FRAGMENT_SHADER);
		passthroughShader->Link();

		Shader::sptr simpleDepthShader = Shader::Create();
		simpleDepthShader->LoadShaderPartFromFile("shaders/simple_depth_vert.glsl", GL_VERTEX_SHADER);
		simpleDepthShader->LoadShaderPartFromFile("shaders/simple_depth_frag.glsl", GL_FRAGMENT_SHADER);
		simpleDepthShader->Link();

		// Load our shaders
		Shader::sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		//Directional Light Shader
		shader->LoadShaderPartFromFile("shaders/directional_blinn_phong_frag.glsl", GL_FRAGMENT_SHADER);
		shader->Link();
		
		//Creates our directional Light
		DirectionalLight theSun;
		UniformBuffer directionalLightBuffer;

		//Allocates enough memory for one directional light (we can change this easily, but we only need 1 directional light)
		directionalLightBuffer.AllocateMemory(sizeof(DirectionalLight));
		//Casts our sun as "data" and sends it to the shader
		directionalLightBuffer.SendData(reinterpret_cast<void*>(&theSun), sizeof(DirectionalLight));

		directionalLightBuffer.Bind(0);

		//Basic effect for drawing to
		PostEffect* basicEffect;
		Framebuffer* shadowBuffer;

		//Post Processing Effects
		int activeEffect = 0;
		std::vector<PostEffect*> effects;
		SepiaEffect* sepiaEffect;
		GreyscaleEffect* greyscaleEffect;
		ColorCorrectEffect* colorCorrectEffect;
		DepthOfField* depthOfFieldEffect;
		
		// We'll add some ImGui controls to control our shader
		BackendHandler::imGuiCallbacks.push_back([&]() {
			if (ImGui::CollapsingHeader("Effect controls"))
			{
				if (activeEffect == 0)
				{
					ImGui::Text("Depth Of Field: Off");
				}

				if (activeEffect == 3)
				{
					ImGui::Text("Depth Of Field: On");

					DepthOfField* temp = (DepthOfField*)effects[activeEffect];

					float planeInFocus = temp->GetPlaneInFocus();

					if (ImGui::SliderFloat("Focal Distance", &planeInFocus, 0.1f, 0.9f))
					{
						temp->SetPlaneInFocus(planeInFocus);
					}

				}
			}
			if (ImGui::CollapsingHeader("Light Level Lighting Settings"))
			{
				if (ImGui::DragFloat3("Light Direction/Position", glm::value_ptr(theSun._lightDirection), 0.01f, -10.0f, 10.0f)) 
				{
					directionalLightBuffer.SendData(reinterpret_cast<void*>(&theSun), sizeof(DirectionalLight));
				}
				if (ImGui::ColorPicker3("Light Col", glm::value_ptr(theSun._lightCol)))
				{
					directionalLightBuffer.SendData(reinterpret_cast<void*>(&theSun), sizeof(DirectionalLight));
				}
			}

			ImGui::Checkbox("Texture Toggle", &textureOn);

			auto name = controllables[selectedVao].get<GameObjectTag>().Name;
			ImGui::Text(name.c_str());
			auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
			ImGui::Checkbox("Relative Rotation", &behaviour->Relative);

			ImGui::Text("Q/E -> Yaw\nLeft/Right -> Roll\nUp/Down -> Pitch\nY -> Toggle Mode");
		
			minFps = FLT_MAX;
			maxFps = 0;
			avgFps = 0;
			for (int ix = 0; ix < 128; ix++) {
				if (fpsBuffer[ix] < minFps) { minFps = fpsBuffer[ix]; }
				if (fpsBuffer[ix] > maxFps) { maxFps = fpsBuffer[ix]; }
				avgFps += fpsBuffer[ix];
			}
			ImGui::PlotLines("FPS", fpsBuffer, 128);
			ImGui::Text("MIN: %f MAX: %f AVG: %f", minFps, maxFps, avgFps / 128.0f);
			});

		#pragma endregion 

		// GL states
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LEQUAL); // New 

		///////////////////////////////////// Texture Loading //////////////////////////////////////////////////
		#pragma region Texture

		// Load some textures from files
		Texture2D::sptr drumstick = Texture2D::LoadFromFile("images/DrumstickTexture.png");
		Texture2D::sptr coilOff = Texture2D::LoadFromFile("images/Tesla_Coil_Texture_Off.png");
		Texture2D::sptr coilOn = Texture2D::LoadFromFile("images/Tesla_Coil_Texture_On.png");
		Texture2D::sptr button = Texture2D::LoadFromFile("images/ButtonTexture.png");
		Texture2D::sptr box = Texture2D::LoadFromFile("images/Box_Texture.png");
		Texture2D::sptr floor = Texture2D::LoadFromFile("images/FloorTilesetFinal.png");
		Texture2D::sptr noSpec = Texture2D::LoadFromFile("images/grassSpec.png");
		LUT3D testCube("cubes/BrightenedCorrection.cube");

		// Load the cube map
		//TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/sample.jpg");
		TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/ToonSky.jpg"); 

		// Creating an empty texture
		Texture2DDescription desc = Texture2DDescription();  
		desc.Width = 1;
		desc.Height = 1;
		desc.Format = InternalFormat::RGB8;
		Texture2D::sptr texture2 = Texture2D::Create(desc);
		// Clear it with a white colour
		texture2->Clear();

		#pragma endregion
		//////////////////////////////////////////////////////////////////////////////////////////

		///////////////////////////////////// Scene Generation //////////////////////////////////////////////////
		#pragma region Scene Generation
		
		// We need to tell our scene system what extra component types we want to support
		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		// Create a scene, and set it to be the active scene in the application
		GameScene::sptr scene = GameScene::Create("test");
		Application::Instance().ActiveScene = scene;

		// We can create a group ahead of time to make iterating on the group faster
		entt::basic_group<entt::entity, entt::exclude_t<>, entt::get_t<Transform>, RendererComponent> renderGroup =
			scene->Registry().group<RendererComponent>(entt::get_t<Transform>());

		// Create a material and set some properties for it
		ShaderMaterial::sptr chickenMat = ShaderMaterial::Create();  
		chickenMat->Shader = shader;
		chickenMat->Set("s_Diffuse", drumstick);
		chickenMat->Set("s_Specular", noSpec);
		chickenMat->Set("u_Shininess", 2.0f);
		chickenMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr coilOffMat = ShaderMaterial::Create();
		coilOffMat->Shader = shader;
		coilOffMat->Set("s_Diffuse", coilOff);
		coilOffMat->Set("s_Specular", noSpec);
		coilOffMat->Set("u_Shininess", 2.0f);
		coilOffMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr coilOnMat = ShaderMaterial::Create();
		coilOnMat->Shader = shader;
		coilOnMat->Set("s_Diffuse", coilOn);
		coilOnMat->Set("s_Specular", noSpec);
		coilOnMat->Set("u_Shininess", 2.0f);
		coilOnMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr boxMat = ShaderMaterial::Create();
		boxMat->Shader = shader;
		boxMat->Set("s_Diffuse", box);
		boxMat->Set("s_Specular", noSpec);
		boxMat->Set("u_Shininess", 2.0f);
		boxMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr buttonMat = ShaderMaterial::Create();
		buttonMat->Shader = shader;
		buttonMat->Set("s_Diffuse", button);
		buttonMat->Set("s_Specular", noSpec);
		buttonMat->Set("u_Shininess", 2.0f);
		buttonMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr floorMat = ShaderMaterial::Create();
		floorMat->Shader = shader;
		floorMat->Set("s_Diffuse", floor);
		floorMat->Set("s_Specular", noSpec);
		floorMat->Set("u_Shininess", 2.0f);
		floorMat->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr clearMat = ShaderMaterial::Create();
		clearMat->Shader = shader;
		clearMat->Set("s_Diffuse", texture2);
		clearMat->Set("s_Specular", noSpec);
		clearMat->Set("u_Shininess", 2.0f);
		clearMat->Set("u_TextureMix", 0.0f);

		GameObject obj1 = scene->CreateEntity("Ground"); 
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/L1-Floor.obj");
			obj1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(floorMat);
			obj1.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			obj1.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
		}

		GameObject obj2 = scene->CreateEntity("Drumstick");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Idle1.obj");
			obj2.emplace<RendererComponent>().SetMesh(vao).SetMaterial(chickenMat);
			obj2.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			obj2.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			obj2.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj2);
		}

		GameObject obj3 = scene->CreateEntity("Coil");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TeslaCoil.obj");
			obj3.emplace<RendererComponent>().SetMesh(vao).SetMaterial(coilOffMat);
			obj3.get<Transform>().SetLocalPosition(0.0f, -8.0f, 0.0f);
			obj3.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			obj3.get<Transform>().SetLocalScale(1.5f, 1.5f, 1.5f);
		}

		GameObject obj4 = scene->CreateEntity("Button 1");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Button.obj");
			obj4.emplace<RendererComponent>().SetMesh(vao).SetMaterial(buttonMat);
			obj4.get<Transform>().SetLocalPosition(-5.0f, -5.0f, 0.0f);
			obj4.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			obj4.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
		}

		GameObject obj5 = scene->CreateEntity("Button 2");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Button.obj");
			obj5.emplace<RendererComponent>().SetMesh(vao).SetMaterial(buttonMat);
			obj5.get<Transform>().SetLocalPosition(5.0f, -5.0f, 0.0f);
			obj5.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			obj5.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
		}

		GameObject obj6 = scene->CreateEntity("Box 1");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Box.obj");
			obj6.emplace<RendererComponent>().SetMesh(vao).SetMaterial(boxMat);
			obj6.get<Transform>().SetLocalPosition(-5.0f, -5.0f, 2.0f);
			obj6.get<Transform>().SetLocalRotation(0.0f, 90.0f, 90.0f);
			obj6.get<Transform>().SetLocalScale(0.2f, 0.2f, 0.2f);
		}

		GameObject obj7 = scene->CreateEntity("Box 2");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Box.obj");
			obj7.emplace<RendererComponent>().SetMesh(vao).SetMaterial(boxMat);
			obj7.get<Transform>().SetLocalPosition(5.0f, -5.0f, 5.0f);
			obj7.get<Transform>().SetLocalRotation(0.0f, 90.0f, 90.0f);
			obj7.get<Transform>().SetLocalScale(0.2f, 0.2f, 0.2f);
		}

		// Create an object to be our camera
		GameObject cameraObject = scene->CreateEntity("Camera");
		{
			cameraObject.get<Transform>().SetLocalPosition(0, 3, 3).LookAt(glm::vec3(0, 0, 0));

			// We'll make our camera a component of the camera object
			Camera& camera = cameraObject.emplace<Camera>();// Camera::Create();
			camera.SetPosition(glm::vec3(0, 3, 3));
			camera.SetUp(glm::vec3(0, 0, 1));
			camera.LookAt(glm::vec3(0));
			camera.SetFovDegrees(90.0f); // Set an initial FOV
			camera.SetOrthoHeight(3.0f);
			BehaviourBinding::Bind<CameraControlBehaviour>(cameraObject);
		}

		int width, height;
		glfwGetWindowSize(BackendHandler::window, &width, &height);

		int shadowWidth = 4096;
		int shadowHeight = 4096;

		GameObject shadowBufferObject = scene->CreateEntity("Shadow Buffer");
		{
			shadowBuffer = &shadowBufferObject.emplace<Framebuffer>();
			shadowBuffer->AddDepthTarget();
			shadowBuffer->Init(shadowWidth, shadowHeight);
		}


		GameObject framebufferObject = scene->CreateEntity("Basic Effect");
		{
			basicEffect = &framebufferObject.emplace<PostEffect>();
			basicEffect->Init(width, height);
		}

		GameObject sepiaEffectObject = scene->CreateEntity("Sepia Effect");
		{
			sepiaEffect = &sepiaEffectObject.emplace<SepiaEffect>();
			sepiaEffect->Init(width, height);
			sepiaEffect->SetIntensity(0.0f);
		}
		effects.push_back(sepiaEffect);

		GameObject greyscaleEffectObject = scene->CreateEntity("Greyscale Effect");
		{
			greyscaleEffect = &greyscaleEffectObject.emplace<GreyscaleEffect>();
			greyscaleEffect->Init(width, height);
		}
		effects.push_back(greyscaleEffect);
		
		GameObject colorCorrectEffectObject = scene->CreateEntity("Greyscale Effect");
		{
			colorCorrectEffect = &colorCorrectEffectObject.emplace<ColorCorrectEffect>();
			colorCorrectEffect->Init(width, height);
		}
		effects.push_back(colorCorrectEffect);

		GameObject depthOfFieldEffectObj = scene->CreateEntity("Depth of Field");
		{
			depthOfFieldEffect = &depthOfFieldEffectObj.emplace<DepthOfField>();
			depthOfFieldEffect->Init(width, height);
		}
		effects.push_back(depthOfFieldEffect);

		#pragma endregion 
		//////////////////////////////////////////////////////////////////////////////////////////

		/////////////////////////////////// SKYBOX ///////////////////////////////////////////////
		{
			// Load our shaders
			Shader::sptr skybox = std::make_shared<Shader>();
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.vert.glsl", GL_VERTEX_SHADER);
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.frag.glsl", GL_FRAGMENT_SHADER);
			skybox->Link();
			
			ShaderMaterial::sptr skyboxMat = ShaderMaterial::Create();
			skyboxMat->Shader = skybox;  
			skyboxMat->Set("s_Environment", environmentMap);
			skyboxMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));
			skyboxMat->RenderLayer = 100;

			MeshBuilder<VertexPosNormTexCol> mesh;
			MeshFactory::AddIcoSphere(mesh, glm::vec3(0.0f), 1.0f);
			MeshFactory::InvertFaces(mesh);
			VertexArrayObject::sptr meshVao = mesh.Bake();
			
			GameObject skyboxObj = scene->CreateEntity("skybox");  
			skyboxObj.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			skyboxObj.get_or_emplace<RendererComponent>().SetMesh(meshVao).SetMaterial(skyboxMat).SetCastShadow(false);
		}
		////////////////////////////////////////////////////////////////////////////////////////


		// We'll use a vector to store all our key press events for now (this should probably be a behaviour eventually)
		std::vector<KeyPressWatcher> keyToggles;
		{
			// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
			// how this is implemented. Note that the ampersand here is capturing the variables within
			// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
			// use std::bind
			keyToggles.emplace_back(GLFW_KEY_T, [&]() { cameraObject.get<Camera>().ToggleOrtho(); });

			keyToggles.emplace_back(GLFW_KEY_1, [&]() { lightNum = 1; });
			keyToggles.emplace_back(GLFW_KEY_2, [&]() { lightNum = 2; });
			keyToggles.emplace_back(GLFW_KEY_3, [&]() { lightNum = 3; });
			keyToggles.emplace_back(GLFW_KEY_4, [&]() { lightNum = 4; });
			keyToggles.emplace_back(GLFW_KEY_5, [&]() { lightNum = 5; });

			controllables.push_back(obj2);

			keyToggles.emplace_back(GLFW_KEY_KP_ADD, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao++;
				if (selectedVao >= controllables.size())
					selectedVao = 0;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});
			keyToggles.emplace_back(GLFW_KEY_KP_SUBTRACT, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao--;
				if (selectedVao < 0)
					selectedVao = controllables.size() - 1;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});

			keyToggles.emplace_back(GLFW_KEY_Y, [&]() {
				auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
				behaviour->Relative = !behaviour->Relative;
				});
		}

		// Initialize our timing instance and grab a reference for our use
		Timing& time = Timing::Instance();
		time.LastFrame = glfwGetTime();

		///// Game loop /////
		while (!glfwWindowShouldClose(BackendHandler::window)) {
			glfwPollEvents();

			if (forwards)
				t += time.DeltaTime / totalTime;
			else
				t -= time.DeltaTime / totalTime;

			if (t < 0.0f)
				t = 0.0f;

			if (t > 1.0f)
				t = 1.0f;

			if (t >= 1.0f || t <= 0.0f)
				forwards = !forwards;

			obj6.get<Transform>().SetLocalPosition(glm::mix(point1, point2, t));
			obj7.get<Transform>().SetLocalPosition(glm::mix(box2Point1, box2Point2, t));

			if (forwards)
				obj3.get<RendererComponent>().SetMaterial(coilOffMat);
			else
				obj3.get<RendererComponent>().SetMaterial(coilOnMat);

			shader->SetUniform("u_LightNum", lightNum);


			// Update the timing
			time.CurrentFrame = glfwGetTime();
			time.DeltaTime = static_cast<float>(time.CurrentFrame - time.LastFrame);

			time.DeltaTime = time.DeltaTime > 1.0f ? 1.0f : time.DeltaTime;

			// Update our FPS tracker data
			fpsBuffer[frameIx] = 1.0f / time.DeltaTime;
			frameIx++;
			if (frameIx >= 128)
				frameIx = 0;

			// We'll make sure our UI isn't focused before we start handling input for our game
			if (!ImGui::IsAnyWindowFocused()) {
				// We need to poll our key watchers so they can do their logic with the GLFW state
				// Note that since we want to make sure we don't copy our key handlers, we need a const
				// reference!
				for (const KeyPressWatcher& watcher : keyToggles) {
					watcher.Poll(BackendHandler::window);
				}
			}

			// Iterate over all the behaviour binding components
			scene->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
				// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
				for (const auto& behaviour : binding.Behaviours) {
					if (behaviour->Enabled) {
						behaviour->Update(entt::handle(scene->Registry(), entity));
					}
				}
			});

			// Clear the screen
			basicEffect->Clear();
			/*greyscaleEffect->Clear();
			sepiaEffect->Clear();*/
			for (int i = 0; i < effects.size(); i++)
			{
				effects[i]->Clear();
			}
			shadowBuffer->Clear();


			glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
			glEnable(GL_DEPTH_TEST);
			glClearDepth(1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Update all world matrices for this frame
			scene->Registry().view<Transform>().each([](entt::entity entity, Transform& t) {
				t.UpdateWorldMatrix();
			});
			
			// Grab out camera info from the camera object
			Transform& camTransform = cameraObject.get<Transform>();
			glm::mat4 view = glm::inverse(camTransform.LocalTransform());
			glm::mat4 projection = cameraObject.get<Camera>().GetProjection();
			glm::mat4 viewProjection = projection * view;

			//Set up light space matrix
			glm::mat4 lightProjectionMatrix = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, -30.0f, 30.0f);
			glm::mat4 lightViewMatrix = glm::lookAt(glm::vec3(-theSun._lightDirection), glm::vec3(), glm::vec3(0.0f, 0.0f, 1.0f));
			glm::mat4 lightSpaceViewProj = lightProjectionMatrix * lightViewMatrix;
						
			// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
			renderGroup.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
				// Sort by render layer first, higher numbers get drawn last
				if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
				if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

				// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
				if (l.Material->Shader < r.Material->Shader) return true;
				if (l.Material->Shader > r.Material->Shader) return false;

				// Sort by material pointer last (so we can minimize switching between materials)
				if (l.Material < r.Material) return true;
				if (l.Material > r.Material) return false;
				
				return false;
			});

			// Start by assuming no shader or material is applied
			Shader::sptr current = nullptr;
			ShaderMaterial::sptr currentMat = nullptr;

			glViewport(0, 0, shadowWidth, shadowHeight);
			shadowBuffer->Bind();

			renderGroup.each([&](entt::entity e, RendererComponent& renderer, Transform& transform) {
				// Render the mesh
				if (renderer.CastShadows)
				{
					BackendHandler::RenderVAO(simpleDepthShader, renderer.Mesh, viewProjection, transform, lightSpaceViewProj);
				}
			});

			shadowBuffer->Unbind();

			
			glfwGetWindowSize(BackendHandler::window, &width, &height);

			glViewport(0, 0, width, height);
			
			depthOfFieldEffect->BindBuffer(0);


			renderGroup.each([&](entt::entity e, RendererComponent& renderer, Transform& transform) {

				if (lightNum == 5)
				{
					depthOfFieldEffect->GetShaders()[1]->Bind();

					BackendHandler::RenderVAO(depthOfFieldEffect->GetShaders()[1], renderer.Mesh, viewProjection, transform, lightSpaceViewProj);
				}
				
				});

			depthOfFieldEffect->UnbindBuffer();

			basicEffect->BindBuffer(0);

			// Iterate over the render group components and draw them
			renderGroup.each( [&](entt::entity e, RendererComponent& renderer, Transform& transform) {

				if (lightNum != 5)
				{
					//renderer.Material->Shader = depthOfFieldEffect->GetShaders()[1];
					// If the shader has changed, set up it's uniforms
					if (current != renderer.Material->Shader) {
						current = renderer.Material->Shader;
						current->Bind();
						BackendHandler::SetupShaderForFrame(current, view, projection);
					}
				}
				else
				{
					current = depthOfFieldEffect->GetShaders()[1];
					current->Bind();
 
				}

				if (textureOn)
				{
					// If the material has changed, apply it
					if (currentMat != renderer.Material) {
						currentMat = renderer.Material;
						currentMat->Apply();
					}
				}
				else
				{
						currentMat = clearMat;
						currentMat->Apply();
				}

				shadowBuffer->BindDepthAsTexture(30);
				// Render the mesh
				BackendHandler::RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform, lightSpaceViewProj);
			});

			shadowBuffer->UnbindTexture(30);
			basicEffect->UnbindBuffer();

			if (lightNum == 5)
			{

				activeEffect = 3;

				effects[3]->ApplyEffect(basicEffect);

				effects[3]->DrawToScreen();
			}
			else
			{

				activeEffect = 0;

				basicEffect->ApplyEffect(basicEffect);

				basicEffect->DrawToScreen();
			}

			/*effects[activeEffect]->ApplyEffect(basicEffect);
			
			effects[activeEffect]->DrawToScreen();*/
			
			// Draw our ImGui content
			BackendHandler::RenderImGui();

			scene->Poll();
			glfwSwapBuffers(BackendHandler::window);
			time.LastFrame = time.CurrentFrame;
		}

		directionalLightBuffer.Unbind(0);

		// Nullify scene so that we can release references
		Application::Instance().ActiveScene = nullptr;
		//Clean up the environment generator so we can release references
		EnvironmentGenerator::CleanUpPointers();
		BackendHandler::ShutdownImGui();
	}	

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}