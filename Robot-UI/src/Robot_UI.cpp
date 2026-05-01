#include "Robot_UI.h"
#include "Robot_API/ROV_API/hardware_interface.h"
#include "Walnut/EntryPoint.h"
#include <GLFW/glfw3.h>
#include <chrono>

Robot_UI_Layer::Robot_UI_Layer(std::shared_ptr<RobotAPI> robotAPI)
	: m_AboutOpen(false), m_OptionOpen(false),
	m_SimulationOpen(false), m_LiveStreamerOpen(true), m_RobotStatusOpen(true),
	m_RobotAPI(robotAPI)
{
	m_OptionPanel = std::make_unique<OptionPanel>();
	m_StreamManager = std::make_unique<StreamManager>();

	// KeyMapping 结构体字段说明:
	// 1. action_name: 动作名称 (如 "Move X")
	// 2. gamepad_key: 当前绑定的手柄按键名称 (初始无绑定，填 "")
	// 3. key_pos: 按键的 UI 坐标 (初始填 {})
	// 4. action_pos: 动作对应 UI 的坐标 (初始填 {})
	// 5. is_bound: 是否已经被绑定 (初始为 false)
	// 6. is_analog: 是否是模拟轴(摇杆、扳机为 true，普通按键为 false)
	m_OptionPanel->GetGamepadMapper()->SetMappings({
		{"Speed Mode", "", {}, {}, false, false},
		{"Servo Linear", "", {}, {}, false, true},
		{"Joystick Correction", "", {}, {}, false, false},
		{"Fast Surface", "", {}, {}, false, false},
		{"Servo Preset X", "", {}, {}, false, false},
		{"Servo Preset Y", "", {}, {}, false, false},
		{"Servo Open", "", {}, {}, false, false},
		{"Servo Close", "", {}, {}, false, false},
		{"Catch Mode Toggle", "", {}, {}, false, false},
		{"Input Block", "", {}, {}, false, false},
		{"Servo Preset 4", "", {}, {}, false, false}
	});

	m_Running = true;
	m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);
}

Robot_UI_Layer::~Robot_UI_Layer()
{
	m_Running = false;
	if (m_GamepadThread.joinable())
	{
		m_GamepadThread.join();
	}
}

void Robot_UI_Layer::GamepadRoutine()
{
	while (m_Running)
	{
		if (m_RobotAPI && m_IsConnected)
		{
			ActuatorData data;

			if (m_OptionPanel->GetRobotConfig()) {
				// 获取配置面板中设置的数据，这其中就已经包含了带有推力曲线参数(curve)和ID的电机列表，
				// 以及在界面上添加或删除好的舵机列表
				data = m_OptionPanel->GetRobotConfig()->active_actuator_config;
			}

			if (m_OptionPanel->GetGamepadMapper()) {
				// 通过 GamepadMapper 中绑定的动作名称拉取数据
				data.motion.x = m_OptionPanel->GetGamepadMapper()->GetActionValue("Move X");
				data.motion.y = m_OptionPanel->GetGamepadMapper()->GetActionValue("Move Y");
				data.motion.z = m_OptionPanel->GetGamepadMapper()->GetActionValue("Move Z");
				data.motion.rx = m_OptionPanel->GetGamepadMapper()->GetActionValue("Roll");
				data.motion.ry = m_OptionPanel->GetGamepadMapper()->GetActionValue("Pitch");
				data.motion.rz = m_OptionPanel->GetGamepadMapper()->GetActionValue("Yaw");

				float speedMode = m_OptionPanel->GetGamepadMapper()->GetActionValue("Speed Mode");
				for (auto& pair : data.brushlessmotor) {
					// 这里只需要修改速度，其推力曲线参数 (curve) 已经在上面的 data 赋值时带过来了，会一同发给下位机
					pair.second.target_speed = speedMode;
				}

				float servoValue = m_OptionPanel->GetGamepadMapper()->GetActionValue("Servo Linear") * 180.0;
				for (auto& pair : data.servo) {
					// 同理，舵机ID也继承自 config
					pair.second.angle = servoValue;
				}
				//{ "Speed Mode", "", {}, {}, false, false },
				//{ "Servo Linear", "", {}, {}, false, true },
				//{ "Joystick Correction", "", {}, {}, false, false },
				//{ "Fast Surface", "", {}, {}, false, false },
				//{ "Servo Preset X", "", {}, {}, false, false },
				//{ "Servo Preset Y", "", {}, {}, false, false },
				//{ "Servo Open", "", {}, {}, false, false },
				//{ "Servo Close", "", {}, {}, false, false },
				//{ "Catch Mode Toggle", "", {}, {}, false, false },
				//{ "Input Block", "", {}, {}, false, false },
				//{ "Servo Preset 4", "", {}, {}, false, false }
			}

			{
				std::lock_guard<std::mutex> lock(m_CommandMutex);
				m_CurrentCommand = data;
			}

			if (m_IsConnected) {
				m_RobotAPI->SendActuatorData(data);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20Hz update rate
	}
}

void Robot_UI_Layer::OnUIRender()
{
	if (m_StreamManager) {
		m_StreamManager->UpdateAll();
	}

	if (m_LiveStreamerOpen && m_StreamManager)
	{
		m_StreamManager->DrawUI("Live Streamer", &m_LiveStreamerOpen);
	}

	if (m_AboutOpen)
	{
		if (ImGui::Begin("About", nullptr, ImGuiWindowFlags_AlwaysVerticalScrollbar))
		{
			ImGui::Text("Robot UI v1.0");
			if (ImGui::Button("Close##1", ImVec2(-1, 0)))
			{
				m_AboutOpen = false;
			}
		}
		ImGui::End();
	}

	if (m_OptionOpen && m_OptionPanel)
	{
		if (ImGui::Begin("Option", nullptr, ImGuiWindowFlags_AlwaysVerticalScrollbar))
		{
			m_OptionPanel->DrawOptionPanel();

			float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

			// Apply 按钮
			if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
			{
				if (m_OptionPanel->Apply()) {
					// Config参数更新后自动断开当前连接，等待重新主动连接
					m_IsConnected = false;
				}
			}

			ImGui::SameLine();

			// Close 按钮
			if (ImGui::Button("Close##2", ImVec2(buttonWidth, 0)))
			{
				m_OptionOpen = false;
				m_OptionPanel->Revert();
			}
		}
		ImGui::End();
	}

	if (m_RobotStatusOpen)
	{
		if (ImGui::Begin("Robot Status", &m_RobotStatusOpen))
		{
			// 连接网口按钮逻辑
			if (m_IsConnected) {
				if (ImGui::Button("Disconnect")) {
					m_IsConnected = false;
				}
			}
			else {
				if (ImGui::Button("Connect")) {
					if (m_RobotAPI && m_OptionPanel->GetRobotConfig()) {
						auto& config = m_OptionPanel->GetRobotConfig();
						m_IsConnected = m_RobotAPI->Initialize(config->active_host_ip, config->active_remote_port, config->active_local_port);
					}
				}
			}

			ImGui::Separator();

			std::lock_guard<std::mutex> lock(m_CommandMutex);
			ImGui::Text("Current Motion Command:");
			ImGui::Text("X (Forward/Back): %.2f", m_CurrentCommand.motion.x);
			ImGui::Text("Y (Left/Right):   %.2f", m_CurrentCommand.motion.y);
			ImGui::Text("Z (Up/Down):      %.2f", m_CurrentCommand.motion.z);
			ImGui::Text("Roll (RX):        %.2f", m_CurrentCommand.motion.rx);
			ImGui::Text("Pitch (RY):       %.2f", m_CurrentCommand.motion.ry);
			ImGui::Text("Yaw (RZ):         %.2f", m_CurrentCommand.motion.rz);

			ImGui::Separator();
			ImGui::Text("Sensor Data:");
			if (m_RobotAPI && m_IsConnected)
			{
				SensorData sensorData = m_RobotAPI->GetSensorData();
				if (sensorData.is_valid)
				{
					if (m_OptionPanel->GetRobotConfig()->active_has_temperature)
						ImGui::Text("Temperature: %.2f *C", sensorData.temperature.value);
					if (m_OptionPanel->GetRobotConfig()->active_has_humidity)
						ImGui::Text("Humidity:    %.2f %%", sensorData.humidity.value);
					if (m_OptionPanel->GetRobotConfig()->active_has_depth)
						ImGui::Text("Depth:       %.2f m", sensorData.depth.value);
				}
				else
				{
					ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Sensor data offline or invalid");
				}
			}
			else
			{
				ImGui::TextDisabled("Robot API Not Connected");
			}
		}
		ImGui::End();
	}

}

// --- Entry Point ---
Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
		Walnut::ApplicationSpecification spec;
		spec.Name = "Robot UI";
		spec.CustomTitlebar = true;

		Walnut::Application* app = new Walnut::Application(spec);

		// 关闭 ImGui 多视口 (Multi-Viewport) 功能
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

		// 实例化具体的接口类
		std::shared_ptr<RobotAPI> hardwareAPI = std::make_shared<HardwareInterface>();

		// 将实例传递给 UI 层
		std::shared_ptr<Robot_UI_Layer> uiLayer = std::make_shared<Robot_UI_Layer>(hardwareAPI);
		app->PushLayer(uiLayer);

		app->SetMenubarCallback([app, uiLayer]()
		{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Exit"))
					{
						app->Close();
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("View"))
				{
					ImGui::MenuItem("Live Streamer", nullptr, &uiLayer->GetLiveStreamerOpen());
					ImGui::MenuItem("Robot Status", nullptr, &uiLayer->GetShowRobotStatus());
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Tools"))
				{
					if (ImGui::MenuItem("Option"))
					{
						uiLayer->ShowOption();
					}

					if (ImGui::MenuItem("About"))
					{
						uiLayer->ShowAbout();
					}
					ImGui::EndMenu();
				}
		});

		return app;
}