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
#include "ConfigSerializer.h"
#include "NodeEditor.h"
#include "ThrustCurveEditor.h"
#include "RobotCommManager.h"
#include "RobotStatus.h"
#include "Robot_API/robot_api.h"
#include "imgui.h"
#include <memory>
#include <thread>
#include <atomic>
#include <string>

class Robot_UI_Layer : public Walnut::Layer
{
public:
	Robot_UI_Layer();
	~Robot_UI_Layer();

	virtual void OnUIRender() override;

	void ShowOption() { m_OptionOpen = true; }
	void ShowAbout() { m_AboutOpen = true; }
	void ShowRobotStatus() { m_RobotStatusOpen = true; }
	void ShowNodeEditor();
	void ShowThrustCurveEditor();
	void ShowRobotCommConfig();
	void SyncNodeEditorModes();

	bool& GetLiveStreamerOpen() { return m_LiveStreamerOpen; }
	bool& GetShowRobotComm() { return m_RobotCommOpen; }
	bool& GetShowRobotStatus() { return m_RobotStatusOpen; }

	// File 菜单操作
	void FileOpen();
	void FileSave();
	void FileSaveAs();

private:
	void LoadConfigFile(const std::string& path);  // 静默加载（不弹框）
	void ApplyUIState(const UIState& st);            // 恢复 UI 状态

	bool m_AboutOpen;
	bool m_OptionOpen;
	bool m_LiveStreamerOpen;
	bool m_RobotStatusOpen;
	bool m_RobotCommOpen = true;
	bool m_NodeEditorOpen;
	bool m_ThrustCurveEditorOpen = false;

	std::unique_ptr<OptionPanel> m_OptionPanel;
	std::unique_ptr<StreamManager> m_StreamManager;
	std::unique_ptr<NodeEditor> m_NodeEditor;
	std::unique_ptr<ThrustCurveEditor> m_ThrustCurveEditor;
	std::unique_ptr<RobotCommManager> m_RobotCommManager;
	std::unique_ptr<RobotStatus> m_RobotStatus;

	std::atomic<std::shared_ptr<const ActuatorData>> m_CurrentCommand;

	std::thread m_GamepadThread;
	std::atomic<bool> m_Running;
	std::string m_CurrentSavePath;  // 当前 .rbt 保存路径（空=未保存过）
	void GamepadRoutine();
};