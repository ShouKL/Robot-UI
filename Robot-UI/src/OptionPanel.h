#pragma once
#include <memory>
#include "EditDraftBase.h"
#include "GamepadMapper.h"
#include "imgui_style.h"
#include "RobotComponent.h"

class OptionPanel : public EditDraftBase {
public:
    OptionPanel();
    ~OptionPanel();

    void DrawOptionPanel();

    void BeginEdit() override;  // 打开 Options 时拍照
    void ApplyEdit() override;  // 应用所有配置
    void CancelEdit() override; // 撤销所有修改

    std::unique_ptr<GamepadMapper>& GetGamepadMapper() { return m_GamepadMapper; }
    std::unique_ptr<ImGuiStyleManager>& GetImGuiStyleManager() { return m_ImGuiStyleManager; }
    std::unique_ptr<RobotComponent>& GetRobotComponent() { return m_RobotComponent; }

private:
    std::unique_ptr<GamepadMapper> m_GamepadMapper;
    std::unique_ptr<ImGuiStyleManager> m_ImGuiStyleManager;
    std::unique_ptr<RobotComponent> m_RobotComponent;

    int selected_id = 0;                // 左侧大类：0=Component, 1=Gamepad, 2=Style

    // ===== 统一快照（三个 tab 进入时拍照，Apply 清空，Cancel 恢复） =====
    void TakeSnapshots();

    // Component 快照
    std::vector<RobotMode> m_ComponentSnapshot;

    // Gamepad 快照
    std::vector<GamepadMode> m_GamepadSnapshot;

    // Style 快照
    ImGuiTheme m_StyleSnapshot_Theme = ImGuiTheme::WalnutDefault;
    bool       m_StyleSnapshot_Invert = false;
    float      m_StyleSnapshot_Alpha = 1.0f;
};