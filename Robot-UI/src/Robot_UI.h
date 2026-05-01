#pragma once

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include "Walnut/Application.h"
#include "Walnut/Layer.h"
#include "OptionPanel.h"
#include "LiveStream.h"
#include "StreamManager.h"
#include "Robot_API/robot_api.h"
#include "imgui.h"
#include <memory>
#include <thread>
#include <atomic>

class Robot_UI_Layer : public Walnut::Layer
{
public:
	Robot_UI_Layer(std::shared_ptr<RobotAPI> robotAPI = nullptr);
	~Robot_UI_Layer();

	virtual void OnUIRender() override;

	void ShowOption() { 
		m_OptionOpen = true; 
		if (m_OptionPanel) m_OptionPanel->Revert();
	}
	void ShowAbout() { m_AboutOpen = true; }
	void ShowRobotStatus() { m_RobotStatusOpen = true; }

	bool& GetLiveStreamerOpen() { return m_LiveStreamerOpen; }
	bool& GetShowRobotStatus() { return m_RobotStatusOpen; }

private:
	bool m_AboutOpen;
	bool m_OptionOpen;
	bool m_SimulationOpen;
	bool m_LiveStreamerOpen;
	bool m_RobotStatusOpen;

	std::unique_ptr<OptionPanel> m_OptionPanel;
	std::unique_ptr<StreamManager> m_StreamManager;

	ActuatorData m_CurrentCommand;
	std::mutex m_CommandMutex;

	std::shared_ptr<RobotAPI> m_RobotAPI;

	std::thread m_GamepadThread;
	std::atomic<bool> m_Running;
	std::atomic<bool> m_IsConnected{false};
	void GamepadRoutine();
};