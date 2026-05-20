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
    bool lastModeToggle = false;
    while (m_Running)
    {
        if (m_RobotAPI && m_IsConnected)
        {
            ActuatorData data;

            if (m_OptionPanel->GetRobotConfig()) {
                data = m_OptionPanel->GetRobotConfig()->GetActiveActuatorConfig();
            }

            if (m_OptionPanel->GetGamepadMapper()) {
                float modeToggleVal = m_OptionPanel->GetGamepadMapper()->GetActionValue("Mode Toggle");
                bool currentModeToggle = modeToggleVal > 0.5f;
                if (currentModeToggle && !lastModeToggle) {
                    if (m_OptionPanel->GetRobotConfig()) {
                        m_OptionPanel->GetRobotConfig()->NextActiveMode();
                        data = m_OptionPanel->GetRobotConfig()->GetActiveActuatorConfig();

                        // 切换到新模式时，同步手柄映射预设
                        auto& active_modes = m_OptionPanel->GetRobotConfig()->GetActiveModes();
                        int active_idx = m_OptionPanel->GetRobotConfig()->GetActiveModeIndex();
                        if (!active_modes.empty() && active_idx < (int)active_modes.size()) {
                            std::string Mode = active_modes[active_idx].gamepad_mapping_Mode;
                            if (m_OptionPanel->GetGamepadMapper()->ModeExists(Mode)) {
                                m_OptionPanel->GetGamepadMapper()->SetActiveMode(Mode);
                            }
                        }
                    }
                }
                lastModeToggle = currentModeToggle;

                // 通过 GamepadMapper 中绑定的动作名称拉取数据
                data.motion.x = m_OptionPanel->GetGamepadMapper()->GetActionValue("Move X");
                data.motion.y = m_OptionPanel->GetGamepadMapper()->GetActionValue("Move Y");
                data.motion.z = m_OptionPanel->GetGamepadMapper()->GetActionValue("Move Z");
                data.motion.rx = m_OptionPanel->GetGamepadMapper()->GetActionValue("Roll");
                data.motion.ry = m_OptionPanel->GetGamepadMapper()->GetActionValue("Pitch");
                data.motion.rz = m_OptionPanel->GetGamepadMapper()->GetActionValue("Yaw");

                float speedMode = m_OptionPanel->GetGamepadMapper()->GetActionValue("Speed Mode");
                for (auto& pair : data.brushlessmotor) {
                    pair.second.target_speed = speedMode;
                }

                float servoValue = m_OptionPanel->GetGamepadMapper()->GetActionValue("Servo Linear") * 180.0;
                for (auto& pair : data.servo) {
                    pair.second.angle = servoValue;
                }
            }

            {
                std::lock_guard<std::mutex> lock(m_CommandMutex);
                m_CurrentCommand = data;
            }

            if (m_IsConnected) {
                m_RobotAPI->SendActuatorData(data);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20Hz
    }
}

void Robot_UI_Layer::OnUIRender()
{
    if (m_OptionPanel && m_OptionPanel->GetGamepadMapper()) {
        m_OptionPanel->GetGamepadMapper()->UpdateGamepadState();
    }

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

            if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
            {
                if (m_OptionPanel->Apply()) {
                    m_IsConnected = false;  // 配置变更后断开连接
                }
            }

            ImGui::SameLine();

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
            if (m_IsConnected) {
                if (ImGui::Button("Disconnect")) {
                    m_IsConnected = false;
                }
            }
            else {
                if (ImGui::Button("Connect")) {
                    if (m_RobotAPI && m_OptionPanel->GetRobotConfig()) {
                        auto& config = m_OptionPanel->GetRobotConfig();
                        m_IsConnected = m_RobotAPI->Initialize(
                            config->GetActiveHostIP(),
                            config->GetActiveRemotePort(),
                            config->GetActiveLocalPort());
                    }
                }
            }

            ImGui::Separator();

            std::string currentModeName = "None";
            if (m_OptionPanel->GetRobotConfig()) {
                currentModeName = m_OptionPanel->GetRobotConfig()->GetActiveModeName();
            }
            ImGui::Text("Current Mode: %s", currentModeName.c_str());

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
                    if (m_OptionPanel->GetRobotConfig()->HasTemperature())
                        ImGui::Text("Temperature: %.2f *C", sensorData.temperature.value);
                    if (m_OptionPanel->GetRobotConfig()->HasHumidity())
                        ImGui::Text("Humidity:    %.2f %%", sensorData.humidity.value);
                    if (m_OptionPanel->GetRobotConfig()->HasDepth())
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

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    std::shared_ptr<RobotAPI> hardwareAPI = std::make_shared<HardwareInterface>();

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