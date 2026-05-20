#include "GamepadMapper.h"

GamepadMapper::GamepadMapper() {
    m_GamepadImage = std::make_shared<Walnut::Image>("../asset/picture/gamepadmap.png");

    // 按键定义（归一化坐标建议另行改造，此处保留原始坐标）
    m_Keys = {
        {ImVec2(938, 340), ImVec2(1250, 351), 17.0f, "Button_A", "A", GLFW_GAMEPAD_BUTTON_A, false},
        {ImVec2(987, 300), ImVec2(1250, 283), 17.0f, "Button_B", "B", GLFW_GAMEPAD_BUTTON_B, false},
        {ImVec2(898, 310), ImVec2(1250, 418), 17.0f, "Button_X", "X", GLFW_GAMEPAD_BUTTON_X, false},
        {ImVec2(945, 265), ImVec2(1250, 218), 17.0f, "Button_Y", "Y", GLFW_GAMEPAD_BUTTON_Y, false},
        {ImVec2(550, 205), ImVec2(312, 154), 17.0f, "LB", "LB", GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, false},
        {ImVec2(995, 205), ImVec2(1250, 154), 17.0f, "RB", "RB", GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, false},
        {ImVec2(595, 327), ImVec2(312, 314), 15.0f, "L_Stick_Click", "L3", GLFW_GAMEPAD_BUTTON_LEFT_THUMB, false},
        {ImVec2(857, 416), ImVec2(1250, 556), 15.0f, "R_Stick_Click", "R3", GLFW_GAMEPAD_BUTTON_RIGHT_THUMB, false},
        {ImVec2(723, 309), ImVec2(655, 548), 15.0f, "Button_View", "View", GLFW_GAMEPAD_BUTTON_BACK, false},
        {ImVec2(818, 309), ImVec2(879, 548), 15.0f, "Button_Menu", "Menu", GLFW_GAMEPAD_BUTTON_START, false},
        {ImVec2(685, 370), ImVec2(312, 350), 13.0f, "DPad_Up", "U", GLFW_GAMEPAD_BUTTON_DPAD_UP, false},
        {ImVec2(685, 420), ImVec2(312, 485), 13.0f, "DPad_Down", "D", GLFW_GAMEPAD_BUTTON_DPAD_DOWN, false},
        {ImVec2(655, 395), ImVec2(312, 439), 13.0f, "DPad_Left", "L", GLFW_GAMEPAD_BUTTON_DPAD_LEFT, false},
        {ImVec2(715, 395), ImVec2(312, 396), 13.0f, "DPad_Right", "R", GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, false},
        {ImVec2(575, 160), ImVec2(312, 80), 20.0f, "LT", "LT", GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, true},
        {ImVec2(960, 160), ImVec2(1250, 80), 20.0f, "RT", "RT", GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, true},
        {ImVec2(595, 327), ImVec2(312, 235), 30.0f, "L_Stick", "LS", GLFW_GAMEPAD_AXIS_LEFT_X, true},
        {ImVec2(857, 416), ImVec2(1250, 485), 30.0f, "R_Stick", "RS", GLFW_GAMEPAD_AXIS_RIGHT_X, true}
    };

    GamepadMode defaultMode;
    strncpy(defaultMode.name, "Default", sizeof(defaultMode.name));
    defaultMode.mappings = {
        {"Speed Mode", "", {}, {}, false, false},
        {"Mode Toggle", "", {}, {}, false, false},
        {"Servo Linear", "", {}, {}, false, true},
        {"Joystick Correction", "", {}, {}, false, false},
        {"Fast Surface", "", {}, {}, false, false},
        {"Servo Mode X", "", {}, {}, false, false},
        {"Servo Mode Y", "", {}, {}, false, false},
        {"Servo Open", "", {}, {}, false, false},
        {"Servo Close", "", {}, {}, false, false},
        {"Catch Mode Toggle", "", {}, {}, false, false},
        {"Input Block", "", {}, {}, false, false},
        {"Servo Mode 4", "", {}, {}, false, false},
        {"Move X", "", {}, {}, false, true},
        {"Move Y", "", {}, {}, false, true},
        {"Move Z", "", {}, {}, false, true},
        {"Roll", "", {}, {}, false, true},
        {"Pitch", "", {}, {}, false, true},
        {"Yaw", "", {}, {}, false, true}
    };
    m_Modes.push_back(defaultMode);
    m_ActiveModeIndex = 0;
    m_SelectedModeIndex = 0;

    memset(&m_LastState, 0, sizeof(GLFWgamepadstate));
    // 不在此处 BeginEdit，由外部在 UI 打开时调用
}

GamepadMapper::~GamepadMapper() {}

// ================= 模式管理 =================

std::vector<std::string> GamepadMapper::GetModeNames() const {
    std::vector<std::string> names;
    for (const auto& mode : m_Modes) {
        names.push_back(mode.name);
    }
    return names;
}

void GamepadMapper::SetActiveMode(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_ModeMutex);
    for (int i = 0; i < (int)m_Modes.size(); ++i) {
        if (m_Modes[i].name == name) {
            m_ActiveModeIndex = i;
            return;
        }
    }
}

void GamepadMapper::SetActiveModeByIndex(int index) {
    std::lock_guard<std::mutex> lock(m_ModeMutex);
    if (index >= 0 && index < (int)m_Modes.size()) {
        m_ActiveModeIndex = index;
    }
}

void GamepadMapper::AddMode() {
    GamepadMode newMode = m_Modes[m_ActiveModeIndex];
    int idx = 0;
    while (true) {
        snprintf(newMode.name, sizeof(newMode.name), "Mode %d", idx);
        if (!ModeExists(newMode.name)) break;
        ++idx;
    }
    m_Modes.push_back(newMode);
    int newIndex = (int)m_Modes.size() - 1;
    m_SelectedModeIndex = newIndex;   // 新模式立即成为当前选中
    BeginEdit(newIndex);
}

void GamepadMapper::DeleteMode(int index) {
    if (m_Modes.size() <= 1) return;
    if (index < 0 || index >= (int)m_Modes.size()) return;
    m_Modes.erase(m_Modes.begin() + index);

    // 调整激活索引
    if (m_ActiveModeIndex >= (int)m_Modes.size())
        m_ActiveModeIndex = (int)m_Modes.size() - 1;

    // 调整选中索引
    if (m_SelectedModeIndex == index) {
        int newSel = (index >= (int)m_Modes.size()) ? ((int)m_Modes.size() - 1) : index;
        m_SelectedModeIndex = newSel;
        BeginEdit(newSel);   // 如果当前正在编辑被删模式，进入邻近模式的编辑
    }
    else if (m_SelectedModeIndex > index) {
        m_SelectedModeIndex--;
    }
    // 若编辑草稿的模式被删，已在上面处理
}

bool GamepadMapper::ModeExists(const std::string& name) const {
    for (const auto& m : m_Modes) {
        if (m.name == name) return true;
    }
    return false;
}

// ================= 草稿机制 =================

void GamepadMapper::BeginEdit(int modeIndex) {
    if (modeIndex < 0 || modeIndex >= (int)m_Modes.size()) return;
    m_EditingMode = m_Modes[modeIndex];   // 深拷贝
    m_EditingModeIndex = modeIndex;
    m_IsEditing = true;
    m_SelectedAction.clear();
    RefreshBoundActions();
}

void GamepadMapper::ApplyEdit() {
    if (!m_IsEditing) return;
    {
        std::lock_guard<std::mutex> lock(m_ModeMutex);
        m_Modes[m_EditingModeIndex] = m_EditingMode;
    }
    m_IsEditing = false;
    m_SelectedAction.clear();
}

void GamepadMapper::CancelEdit() {
    m_IsEditing = false;
    m_SelectedAction.clear();
    // 显示将回退到 m_Modes 中的原始模式（GetDisplayMode 会返回激活模式）
}

const GamepadMode& GamepadMapper::GetDisplayMode() const {
    if (m_IsEditing) return m_EditingMode;
    return m_Modes[m_ActiveModeIndex];
}

GamepadMode& GamepadMapper::GetDisplayMode() {
    if (m_IsEditing) return m_EditingMode;
    return m_Modes[m_ActiveModeIndex];
}

// ================= 绑定逻辑（操作编辑区） =================

void GamepadMapper::BindAction() {
    if (!m_IsEditing || m_SelectedAction.empty()) return;
    auto& mappings = m_EditingMode.mappings;
    for (auto& mapping : mappings) {
        if (mapping.action_name != m_SelectedAction) continue;
        for (const auto& key : m_Keys) {
            if (mapping.is_analog != key.is_axis) continue;
            if (m_ButtonIntensity[key.name] < k_BindThreshold) continue;

            auto& boundList = m_KeyBoundActions[key.name];
            if (boundList.size() >= 2) break;

            // 安全解绑旧按键
            UnbindAction(m_SelectedAction);

            mapping.is_bound = true;
            mapping.gamepad_key = key.name;
            mapping.key_pos = key.pos;
            boundList.push_back(m_SelectedAction);
            m_SelectedAction.clear();
            break;
        }
        if (m_SelectedAction.empty()) break;
    }
    RefreshBoundActions();
}

void GamepadMapper::UnbindAction(const std::string& actionName) {
    if (!m_IsEditing) return;
    auto& mappings = m_EditingMode.mappings;
    for (auto& mapping : mappings) {
        if (mapping.action_name == actionName && mapping.is_bound) {
            // 防止空键污染
            if (!mapping.gamepad_key.empty()) {
                auto& boundList = m_KeyBoundActions[mapping.gamepad_key];
                auto it = std::find(boundList.begin(), boundList.end(), actionName);
                if (it != boundList.end()) boundList.erase(it);
            }
            mapping.is_bound = false;
            mapping.gamepad_key.clear();
            mapping.key_pos = {};
            break;
        }
    }
}

void GamepadMapper::RefreshBoundActions() {
    m_KeyBoundActions.clear();
    const GamepadMode& mode = GetDisplayMode();
    for (const auto& m : mode.mappings) {
        if (m.is_bound && !m.gamepad_key.empty()) {
            m_KeyBoundActions[m.gamepad_key].push_back(m.action_name);
        }
    }
}

// ================= 手柄状态更新 =================

float GamepadMapper::CalculateIntensity(const KeyInfo& key, const GLFWgamepadstate& state) {
    if (!key.is_axis) {
        return (state.buttons[key.glfw_id] != GLFW_RELEASE) ? 1.0f : 0.0f;
    }
    if (key.glfw_id == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER || key.glfw_id == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        float val = (state.axes[key.glfw_id] + 1.0f) * 0.5f;
        return (val < k_VisualDeadzone) ? 0.0f : val;
    }
    else if (key.name == "L_Stick") {
        float val = std::max(std::abs(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X]),
            std::abs(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]));
        return (val < k_VisualDeadzone) ? 0.0f : val;
    }
    else if (key.name == "R_Stick") {
        float val = std::max(std::abs(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]),
            std::abs(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]));
        return (val < k_VisualDeadzone) ? 0.0f : val;
    }
    return 0.0f;
}

void GamepadMapper::UpdateGamepadState() {
    GLFWgamepadstate state;
    if (!glfwJoystickPresent(GLFW_JOYSTICK_1) || !glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) return;

    for (const auto& key : m_Keys) {
        m_ButtonIntensity[key.name] = CalculateIntensity(key, state);
    }

    if (m_IsEditing) {
        BindAction();
    }

    m_LX = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
    m_LY = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
    m_RX = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
    m_RY = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
}

float GamepadMapper::GetActionValue(const std::string& actionName) {
    std::lock_guard<std::mutex> lock(m_ModeMutex);
    if (m_ActiveModeIndex < 0 || m_ActiveModeIndex >= (int)m_Modes.size()) return 0.0f;
    const auto& mappings = m_Modes[m_ActiveModeIndex].mappings;
    for (const auto& mapping : mappings) {
        if (mapping.action_name == actionName && mapping.is_bound) {
            if (m_ButtonIntensity.count(mapping.gamepad_key)) {
                return m_ButtonIntensity.at(mapping.gamepad_key);
            }
        }
    }
    return 0.0f;
}

// ================= UI 绘制 =================

void GamepadMapper::DrawGamepadMapper() {
    if (!m_IsEditing) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Not in edit mode. Call BeginEdit() first.");
        return;
    }

    GamepadMode& mode = m_EditingMode;
    ImGui::InputText("Mode Name", mode.name, sizeof(mode.name));

    RefreshBoundActions();
    auto& mappings = mode.mappings;

    if (!m_GamepadImage) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load gamepad image!");
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float imgW = (float)m_GamepadImage->GetWidth();
    float imgH = (float)m_GamepadImage->GetHeight();

    // 数字动作区域
    ImGui::TextDisabled("DIGITAL ACTIONS");
    ImGui::BeginChild("##Digital", ImVec2(0, avail.y * 0.22f), true);
    int count = 0;
    for (auto& m : mappings) {
        if (m.is_analog) continue;
        bool selected = (m_SelectedAction == m.action_name);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 0.5f, 0, 1));
        else if (m.is_bound) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0.5f, 1));

        if (ImGui::Button(m.action_name.c_str(), ImVec2(180, 45))) m_SelectedAction = m.action_name;

        if (selected || m.is_bound) ImGui::PopStyleColor();
        if (++count % 6 != 0) ImGui::SameLine();
    }
    ImGui::EndChild();

    // 手柄画布
    ImGui::BeginChild("##Canvas", ImVec2(0, avail.y * 0.55f), false);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasTL = ImGui::GetCursorScreenPos();
    ImVec2 canvasSz = ImGui::GetContentRegionAvail();

    float scale = std::min(canvasSz.x / imgW, canvasSz.y / imgH);
    ImVec2 renderSz = { imgW * scale, imgH * scale };
    ImVec2 offset = { (canvasSz.x - renderSz.x) * 0.5f, (canvasSz.y - renderSz.y) * 0.5f };
    ImVec2 imgOrigin = { canvasTL.x + offset.x, canvasTL.y + offset.y };

    ImGui::SetCursorScreenPos(imgOrigin);
    ImGui::Image(m_GamepadImage->GetDescriptorSet(), renderSz);

    // 按键强度反馈
    for (const auto& key : m_Keys) {
        float val = m_ButtonIntensity[key.name];
        if (val > 0) {
            ImVec2 p = { imgOrigin.x + key.pos.x * scale, imgOrigin.y + key.pos.y * scale };
            dl->AddCircleFilled(p, key.radius * scale * (1 + val * 0.15f), IM_COL32(255, 120, 0, (int)(30 + val * 180)));
        }
    }

    // 绑定标签（保持不变）
    float btnHeight = 26.0f * scale;
    float btnSpacing = 5.0f * scale;
    float horizontalPadding = 10.0f * scale;

    for (const auto& key : m_Keys) {
        const auto& boundList = m_KeyBoundActions[key.name];
        if (boundList.empty()) continue;

        bool isVertical = (key.name == "Button_View" || key.name == "Button_Menu");
        ImVec2 basePos = { imgOrigin.x + key.textPos.x * scale, imgOrigin.y + key.textPos.y * scale };

        if (isVertical) {
            float maxWidth = 0.0f;
            for (const auto& action : boundList) {
                float w = ImGui::CalcTextSize(action.c_str()).x + (horizontalPadding * 2.0f);
                if (w > maxWidth) maxWidth = w;
            }
            float totalHeight = (btnHeight * boundList.size()) + (btnSpacing * (boundList.size() - 1));
            ImVec2 currentPos = { basePos.x - (maxWidth * 0.5f), basePos.y - (totalHeight * 0.5f) };
            for (const auto& actionName : boundList) {
                ImGui::SetCursorScreenPos(currentPos);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 1, 0.5f, 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * scale);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                std::string label = actionName + "##" + key.name;
                if (ImGui::Button(label.c_str(), ImVec2(maxWidth, btnHeight))) {
                    UnbindAction(actionName);
                }
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
                currentPos.y += btnHeight + btnSpacing;
            }
        }
        else {
            float totalWidth = 0.0f;
            std::vector<float> widths;
            for (const auto& action : boundList) {
                float w = ImGui::CalcTextSize(action.c_str()).x + (horizontalPadding * 2.0f);
                widths.push_back(w);
                totalWidth += w;
            }
            totalWidth += btnSpacing * (boundList.size() - 1);
            ImVec2 startPos = { basePos.x - (totalWidth * 0.5f), basePos.y - (btnHeight * 0.5f) };
            ImGui::SetCursorScreenPos(startPos);
            for (size_t i = 0; i < boundList.size(); ++i) {
                if (i > 0) ImGui::SameLine(0, btnSpacing);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 1, 0.5f, 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * scale);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                std::string label = boundList[i] + "##" + key.name;
                if (ImGui::Button(label.c_str(), ImVec2(widths[i], btnHeight))) {
                    UnbindAction(boundList[i]);
                }
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
            }
        }
    }
    ImGui::EndChild();

    // 模拟动作区域
    ImGui::TextDisabled("ANALOG ACTIONS");
    ImGui::BeginChild("##Analog", ImVec2(0, 0), true);
    count = 0;
    for (auto& m : mappings) {
        if (!m.is_analog) continue;
        bool selected = (m_SelectedAction == m.action_name);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0.6f, 1, 1));
        else if (m.is_bound) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0.5f, 1));

        if (ImGui::Button(m.action_name.c_str(), ImVec2(180, 45))) m_SelectedAction = m.action_name;

        if (selected || m.is_bound) ImGui::PopStyleColor();
        if (++count % 6 != 0) ImGui::SameLine();
    }
    ImGui::EndChild();
}