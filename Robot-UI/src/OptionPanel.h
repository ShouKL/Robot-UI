#pragma once

#include "EditDraftBase.h"
#include "GamepadMapper.h"
#include "imgui_style.h"
#include "RobotComponent.h"
#include <memory>

// ============================================================================
// OptionPanel — 配置面板（组件/手柄/风格三合一）
// 统一快照机制：BeginEdit 拍照，ApplyEdit 清空，CancelEdit 恢复
// ============================================================================

class OptionPanel : public EditDraftBase
{
public:
    OptionPanel();
    ~OptionPanel();

    // ---- 编辑生命周期 ----
    void BeginEdit() override;
    void ApplyEdit() override;
    void CancelEdit() override;

    // ---- 子组件访问 ----
    std::unique_ptr<GamepadMapper>&     GetGamepadMapper()     { return m_GamepadMapper; }
    std::unique_ptr<ImGuiStyleManager>& GetImGuiStyleManager() { return m_ImGuiStyleManager; }
    std::unique_ptr<RobotComponent>&    GetRobotComponent()    { return m_RobotComponent; }

    // ---- UI ----
    void DrawOptionPanel();

private:
    void TakeSnapshots();

    // 子组件
    std::unique_ptr<GamepadMapper>     m_GamepadMapper;
    std::unique_ptr<ImGuiStyleManager> m_ImGuiStyleManager;
    std::unique_ptr<RobotComponent>    m_RobotComponent;

    // UI 状态
    int selected_id = 0;    // 左侧大类：0=Component, 1=Gamepad, 2=Style

    // 快照（进入编辑时拍照，Apply 清空，Cancel 恢复）
    std::vector<RobotMode>   m_ComponentSnapshot;
    std::vector<GamepadMode> m_GamepadSnapshot;
    ImGuiTheme               m_StyleSnapshot_Theme  = ImGuiTheme::WalnutDefault;
    bool                     m_StyleSnapshot_Invert  = false;
    float                    m_StyleSnapshot_Alpha   = 1.0f;
};