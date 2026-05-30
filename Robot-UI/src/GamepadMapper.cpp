#include "GamepadMapper.h"
#include "Walnut/Core/Log.h"
#include <cmath>

// ======== 构造/析构 ========
GamepadMapper::GamepadMapper() {
    m_GamepadImage = std::make_shared<Walnut::Image>("../asset/picture/gamepadmap.png");

    m_Keys = std::vector<KeyInfo>{
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
        {ImVec2(595, 327), ImVec2(312, 205), 30.0f, "L_Stick_X", "LS X", GLFW_GAMEPAD_AXIS_LEFT_X, true},
        {ImVec2(595, 327), ImVec2(312, 265), 30.0f, "L_Stick_Y", "LS Y", GLFW_GAMEPAD_AXIS_LEFT_Y, true},
        {ImVec2(857, 416), ImVec2(1250, 455), 30.0f, "R_Stick_X", "RS X", GLFW_GAMEPAD_AXIS_RIGHT_X, true},
        {ImVec2(857, 416), ImVec2(1250, 515), 30.0f, "R_Stick_Y", "RS Y", GLFW_GAMEPAD_AXIS_RIGHT_Y, true}
    };

    GamepadMode defaultMode;
    strncpy(defaultMode.name, "", sizeof(defaultMode.name));
    m_Modes = std::vector<GamepadMode>{};
    m_ActiveModeIndex = 0;
    m_SelectedModeIndex = 0;

    memset(&m_LastState, 0, sizeof(GLFWgamepadstate));
}

GamepadMapper::~GamepadMapper() {}

// ======== 键位管理 ========
int GamepadMapper::AddKey(const std::string& keyName, bool isAnalog)
{
    if (m_Modes.empty() || m_SelectedModeIndex < 0 || m_SelectedModeIndex >= (int)m_Modes.size()) return -1;
    GamepadKey key;
    key.id       = m_NextKeyID++;
    key.name     = keyName;
    key.is_analog = isAnalog;

    GamepadMode& mode = m_Modes[m_SelectedModeIndex];
    mode.keys.push_back(key);

    KeyMapping km;
    km.key_id   = key.id;
    km.key_name = keyName;
    km.is_bound = false;
    km.is_analog = isAnalog;
    mode.mappings.push_back(km);

    return key.id;
}

void GamepadMapper::RemoveKey(int keyId)
{
    if (m_Modes.empty() || m_SelectedModeIndex >= (int)m_Modes.size()) return;
    GamepadMode& mode = m_Modes[m_SelectedModeIndex];
    mode.keys.erase(
        std::remove_if(mode.keys.begin(), mode.keys.end(),
            [keyId](const GamepadKey& k) { return k.id == keyId; }),
        mode.keys.end());

    mode.mappings.erase(
        std::remove_if(mode.mappings.begin(), mode.mappings.end(),
            [keyId](const KeyMapping& m) { return m.key_id == keyId; }),
        mode.mappings.end());
}

void GamepadMapper::RenameKey(int keyId, const std::string& newName)
{
    if (m_Modes.empty() || m_SelectedModeIndex >= (int)m_Modes.size()) return;
    GamepadMode& mode = m_Modes[m_SelectedModeIndex];
    for (auto& k : mode.keys)
    {
        if (k.id == keyId) { k.name = newName; break; }
    }
    for (auto& m : mode.mappings)
    {
        if (m.key_id == keyId) { m.key_name = newName; break; }
    }
}

const std::vector<GamepadKey>& GamepadMapper::GetKeys() const
{
    static const std::vector<GamepadKey> empty;
    if (m_Modes.empty() || m_SelectedModeIndex >= (int)m_Modes.size()) return empty;
    return m_Modes[m_SelectedModeIndex].keys;
}

void GamepadMapper::UpdateNextKeyID()
{
    int maxId = 0;
    for (const auto& mode : m_Modes)
    {
        for (const auto& k : mode.keys)
            if (k.id > maxId) maxId = k.id;
    }
    m_NextKeyID = maxId + 1;
}

// ======== 模式管理 ========
std::vector<std::string> GamepadMapper::GetModeNames() const {
    std::vector<std::string> names;
    for (const auto& mode : m_Modes) {
        names.push_back(mode.name);
    }
    return names;
}

void GamepadMapper::SetActiveMode(const std::string& name) {
    for (int i = 0; i < (int)m_Modes.size(); ++i) {
        if (m_Modes[i].name == name) {
            if (m_ActiveModeIndex != i) {
                WL_INFO_TAG("GAMEPAD", "Active mode set: {} (index {})", name, i);
                m_ActiveModeIndex = i;
            }
            return;
        }
    }
    WL_WARN_TAG("GAMEPAD", "SetActiveMode failed: mode '{}' not found", name);
}

void GamepadMapper::SetActiveModeByIndex(int index) {
    if (index >= 0 && index < (int)m_Modes.size()) {
        m_ActiveModeIndex = index;
    }
}

void GamepadMapper::AddMode() {
    GamepadMode newMode;
    if (!m_Modes.empty()) {
        newMode = m_Modes[m_ActiveModeIndex];
    }
    int idx = 0;
    while (true) {
        snprintf(newMode.name, sizeof(newMode.name), "Mode %d", idx);
        if (!ModeExists(newMode.name)) break;
        ++idx;
    }
    m_Modes.push_back(newMode);
    m_SelectedModeIndex = (int)m_Modes.size() - 1;
}

void GamepadMapper::DeleteMode(int index) {
    if (m_Modes.empty()) return;
    if (index < 0 || index >= (int)m_Modes.size()) return;
    m_Modes.erase(m_Modes.begin() + index);

    if (m_ActiveModeIndex >= (int)m_Modes.size())
        m_ActiveModeIndex = (int)m_Modes.size() - 1;

    if (m_SelectedModeIndex >= (int)m_Modes.size())
        m_SelectedModeIndex = std::max(0, (int)m_Modes.size() - 1);
}

bool GamepadMapper::ModeExists(const std::string& name) const {
    for (const auto& m : m_Modes) {
        if (m.name == name) return true;
    }
    return false;
}

std::vector<GamepadMode>& GamepadMapper::GetModes() {
    return m_Modes;
}

const std::vector<GamepadMode>& GamepadMapper::GetModes() const {
    return m_Modes;
}

GamepadMode& GamepadMapper::GetSelectedMode() {
    if (m_Modes.empty() || m_SelectedModeIndex < 0 || m_SelectedModeIndex >= (int)m_Modes.size()) {
        static GamepadMode s_emptyMode;
        return s_emptyMode;
    }
    return m_Modes[m_SelectedModeIndex];
}

const GamepadMode& GamepadMapper::GetSelectedMode() const {
    if (m_Modes.empty() || m_SelectedModeIndex < 0 || m_SelectedModeIndex >= (int)m_Modes.size()) {
        static const GamepadMode s_emptyMode;
        return s_emptyMode;
    }
    return m_Modes[m_SelectedModeIndex];
}

// ======== 绑定管理 ========
void GamepadMapper::BindKey() {
    if (m_SelectedKey.empty()) return;
    if (m_Modes.empty() || m_SelectedModeIndex >= (int)m_Modes.size()) return;
    auto& mode = m_Modes[m_SelectedModeIndex];
    auto& mappings = mode.mappings;
    const auto& activeKeys = GetActivePhysicalKeys();
    for (auto& mapping : mappings) {
        if (mapping.key_name != m_SelectedKey) continue;
        for (const auto& key : activeKeys) {
            if (mapping.is_analog != key.is_axis) continue;
            float rawVal = m_RawKeyValues[key.name];
            float val = CalcActivation(key, rawVal);
            if (val < k_BindThreshold) continue;

            auto& boundList = m_KeyBoundActions[key.name];
            if (boundList.size() >= 2) break;

            // 安全解绑旧按键
            UnbindKey(m_SelectedKey);

            mapping.is_bound = true;
            mapping.gamepad_key = key.name;
            mapping.key_pos = key.pos;
            boundList.push_back(m_SelectedKey);
            WL_INFO_TAG("GAMEPAD", "Key bound: '{}' -> '{}'", m_SelectedKey, key.name);
            m_SelectedKey.clear();
            break;
        }
        if (m_SelectedKey.empty()) break;
    }
    RefreshBoundKeys();
}

void GamepadMapper::UnbindKey(const std::string& keyName) {
    if (m_Modes.empty() || m_SelectedModeIndex >= (int)m_Modes.size()) return;
    auto& mappings = m_Modes[m_SelectedModeIndex].mappings;
    for (auto& mapping : mappings) {
        if (mapping.key_name == keyName && mapping.is_bound) {
            // 防止空键污染
            if (!mapping.gamepad_key.empty()) {
                auto& boundList = m_KeyBoundActions[mapping.gamepad_key];
                auto it = std::find(boundList.begin(), boundList.end(), keyName);
                if (it != boundList.end()) boundList.erase(it);
            }
            mapping.is_bound = false;
            mapping.gamepad_key.clear();
            mapping.key_pos = ImVec2();
            break;
        }
    }
}

void GamepadMapper::RefreshBoundKeys() {
    m_KeyBoundActions.clear();
    if (m_Modes.empty() || m_SelectedModeIndex >= (int)m_Modes.size()) return;
    const GamepadMode& mode = m_Modes[m_SelectedModeIndex];
    for (const auto& m : mode.mappings) {
        if (m.is_bound && !m.gamepad_key.empty()) {
            m_KeyBoundActions[m.gamepad_key].push_back(m.key_name);
        }
    }
}

float GamepadMapper::CalcRawValue(const KeyInfo& key, const GLFWgamepadstate& state) {
    if (!key.is_axis) {
        return (state.buttons[key.glfw_id] != GLFW_RELEASE) ? 1.0f : 0.0f;
    }
    // 轴直接返回原始值 [-1, 1]
    return state.axes[key.glfw_id];
}

float GamepadMapper::CalcActivation(const KeyInfo& key, float rawVal) const {
    if (!key.is_axis) return rawVal;
    if (key.glfw_id == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER || key.glfw_id == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        return (rawVal + 1.0f) * 0.5f;
    }
    return std::abs(rawVal);
}

// ======== 游戏手柄状态更新 ========

void GamepadMapper::UpdateGamepadState() {
    UpdateRawJoystickState();
    UpdateAllKeyValues();

    BindKey();
}

float GamepadMapper::GetKeyValue(const std::string& keyName) {
    std::lock_guard<std::mutex> lock(m_RawKeyValuesMutex);
    if (m_ActiveModeIndex < 0 || m_ActiveModeIndex >= (int)m_Modes.size()) return 0.0f;
    const auto& mappings = m_Modes[m_ActiveModeIndex].mappings;
    for (const auto& mapping : mappings) {
        if (mapping.key_name == keyName && mapping.is_bound) {
            if (m_RawKeyValues.count(mapping.gamepad_key)) {
                return m_RawKeyValues.at(mapping.gamepad_key);
            }
        }
    }

    // Fallback: allow KeySource nodes to reference physical button/axis names directly
    // (e.g. "L_Stick", "A", "LT", "RT", "LX", "LY", "RX", "RY")
    if (m_RawKeyValues.count(keyName)) {
        return m_RawKeyValues.at(keyName);
    }
    if (keyName == "LX") return m_RawKeyValues["L_Stick_X"];
    if (keyName == "LY") return m_RawKeyValues["L_Stick_Y"];
    if (keyName == "RX") return m_RawKeyValues["R_Stick_X"];
    if (keyName == "RY") return m_RawKeyValues["R_Stick_Y"];

    return 0.0f;
}

std::vector<std::string> GamepadMapper::GetActiveModeBoundKeyNames() const
{
    std::vector<std::string> names;
    if (m_ActiveModeIndex < 0 || m_ActiveModeIndex >= (int)m_Modes.size())
        return names;
    for (const auto& mapping : m_Modes[m_ActiveModeIndex].mappings) {
        if (mapping.is_bound)
            names.push_back(mapping.key_name);
    }
    return names;
}

// ================= 手柄类型 =================

// ======== 手柄类型 ========

void GamepadMapper::SetGamepadType(GamepadType type) {
    if (m_Modes.empty() || m_SelectedModeIndex >= (int)m_Modes.size()) return;
    m_Modes[m_SelectedModeIndex].gamepad_type = type;
}

GamepadType GamepadMapper::GetGamepadType() const {
    if (m_Modes.empty() || m_SelectedModeIndex >= (int)m_Modes.size()) return GamepadType::Xbox;
    return m_Modes[m_SelectedModeIndex].gamepad_type;
}

const std::vector<KeyInfo>& GamepadMapper::GetActivePhysicalKeys() const {
    if (GetSelectedMode().gamepad_type == GamepadType::Custom)
        return m_CustomPhysicalKeys;
    return m_Keys;
}

void GamepadMapper::UpdateRawJoystickState() {
    m_CustomPresent = false;
    int count;
    const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &count);
    if (!buttons) return;

    m_CustomPresent = true;
    m_CustomButtonCount = count;
    m_RawButtons.assign(buttons, buttons + count);

    const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count);
    if (axes) {
        m_CustomAxisCount = count;
        m_RawAxes.assign(axes, axes + count);
    } else {
        m_CustomAxisCount = 0;
        m_RawAxes.clear();
    }
}

void GamepadMapper::RebuildCustomKeys() {
    m_CustomPhysicalKeys.clear();
    if (!m_CustomPresent) return;

    float yStart = 50.0f;
    float yStep = 22.0f;
    float y = yStart;

    // 按钮
    for (int i = 0; i < m_CustomButtonCount; ++i) {
        KeyInfo k;
        k.name = "Btn_" + std::to_string(i);
        k.label = "B" + std::to_string(i);
        k.glfw_id = i;
        k.is_axis = false;
        k.pos = ImVec2(40.0f, y);
        k.textPos = ImVec2(200.0f, y);
        k.radius = 8.0f;
        m_CustomPhysicalKeys.push_back(k);
        y += yStep;
    }

    y = yStart;

    // 轴
    for (int i = 0; i < m_CustomAxisCount; ++i) {
        KeyInfo k;
        k.name = "Axis_" + std::to_string(i);
        k.label = "A" + std::to_string(i);
        k.glfw_id = i;
        k.is_axis = true;
        k.pos = ImVec2(400.0f, y);
        k.textPos = ImVec2(560.0f, y);
        k.radius = 10.0f;
        m_CustomPhysicalKeys.push_back(k);
        y += yStep;
    }
}

void GamepadMapper::UpdateAllKeyValues() {
    std::lock_guard<std::mutex> lock(m_RawKeyValuesMutex);
    m_RawKeyValues.clear();

    // Xbox 按键
    GLFWgamepadstate state;
    if (glfwJoystickPresent(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
        for (const auto& key : m_Keys) {
            m_RawKeyValues[key.name] = CalcRawValue(key, state);
        }
    }

    // 自定义手柄按键
    if (m_CustomPresent) {
        for (int i = 0; i < m_CustomButtonCount; ++i) {
            std::string name = "Btn_" + std::to_string(i);
            m_RawKeyValues[name] = (m_RawButtons[i] != 0) ? 1.0f : 0.0f;
        }
        for (int i = 0; i < m_CustomAxisCount; ++i) {
            std::string name = "Axis_" + std::to_string(i);
            m_RawKeyValues[name] = m_RawAxes[i];
        }
    }
}

// ================= UI 绘制（内部辅助） =================

void GamepadMapper::DrawXboxCanvas() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float imgW = (float)m_GamepadImage->GetWidth();
    float imgH = (float)m_GamepadImage->GetHeight();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasTL = ImGui::GetCursorScreenPos();
    ImVec2 canvasSz = avail;

    float scale = std::min(canvasSz.x / imgW, canvasSz.y / imgH);
    ImVec2 renderSz = { imgW * scale, imgH * scale };
    ImVec2 offset = { (canvasSz.x - renderSz.x) * 0.5f, (canvasSz.y - renderSz.y) * 0.5f };
    ImVec2 imgOrigin = { canvasTL.x + offset.x, canvasTL.y + offset.y };

    ImGui::SetCursorScreenPos(imgOrigin);
    ImGui::Image(m_GamepadImage->GetDescriptorSet(), renderSz);

    // 按键强度反馈（跳过摇杆轴条目，摇杆在下面单独绘制）
    for (const auto& key : m_Keys) {
        if (key.name == "L_Stick_X" || key.name == "L_Stick_Y" ||
            key.name == "R_Stick_X" || key.name == "R_Stick_Y")
            continue;
        float rawVal = m_RawKeyValues[key.name];
        float val = CalcActivation(key, rawVal);
        if (val > 0.01f) {
            ImVec2 p = { imgOrigin.x + key.pos.x * scale, imgOrigin.y + key.pos.y * scale };
            dl->AddCircleFilled(p, key.radius * scale * (1 + val * 0.15f), IM_COL32(255, 120, 0, (int)(30 + val * 180)));
        }
    }

    // 摇杆综合强度 + 小红点
    {
        float lx = m_RawKeyValues["L_Stick_X"], ly = m_RawKeyValues["L_Stick_Y"];
        float rx = m_RawKeyValues["R_Stick_X"], ry = m_RawKeyValues["R_Stick_Y"];
        float lMag = std::sqrt(lx * lx + ly * ly);
        float rMag = std::sqrt(rx * rx + ry * ry);
        float stickTravel = 30.0f;

        // 左摇杆
        {
            ImVec2 center = { imgOrigin.x + 595.0f * scale, imgOrigin.y + 327.0f * scale };
            if (lMag > 0.01f) {
                float r = 30.0f * scale * (1 + lMag * 0.15f);
                dl->AddCircleFilled(center, r, IM_COL32(255, 120, 0, (int)(30 + lMag * 180)));
            }
            ImVec2 dot = { center.x + lx * stickTravel * scale, center.y + ly * stickTravel * scale };
            dl->AddCircleFilled(dot, 4.0f * scale, IM_COL32(255, 60, 40, 220));
        }

        // 右摇杆
        {
            ImVec2 center = { imgOrigin.x + 857.0f * scale, imgOrigin.y + 416.0f * scale };
            if (rMag > 0.01f) {
                float r = 30.0f * scale * (1 + rMag * 0.15f);
                dl->AddCircleFilled(center, r, IM_COL32(255, 120, 0, (int)(30 + rMag * 180)));
            }
            ImVec2 dot = { center.x + rx * stickTravel * scale, center.y + ry * stickTravel * scale };
            dl->AddCircleFilled(dot, 4.0f * scale, IM_COL32(255, 60, 40, 220));
        }
    }

    // 绑定标签
    float btnHeight = 26.0f * scale;
    float btnSpacing = 5.0f * scale;
    float horizontalPadding = 10.0f * scale;

    for (const auto& key : m_Keys) {
        const auto& boundList = m_KeyBoundActions[key.name];
        if (boundList.empty()) continue;

        bool isVertical = (key.name == "Button_View" || key.name == "Button_Menu"
            || key.name == "L_Stick_X" || key.name == "L_Stick_Y"
            || key.name == "R_Stick_X" || key.name == "R_Stick_Y");
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
                    UnbindKey(actionName);
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
                    UnbindKey(boundList[i]);
                }
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
            }
        }
    }
}

void GamepadMapper::DrawCustomCanvas() {
    const auto& keys = m_CustomPhysicalKeys;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasTL = ImGui::GetCursorScreenPos();
    ImVec2 canvasSz = ImGui::GetContentRegionAvail();

    // 绘制每个物理按键的圆 + 标签
    for (const auto& key : keys) {
        ImVec2 p = { canvasTL.x + key.pos.x, canvasTL.y + key.pos.y };
        float rawVal = m_RawKeyValues[key.name];
        float val = CalcActivation(key, rawVal);

        ImU32 color;
        if (val > 0.5f)
            color = IM_COL32(255, 120, 0, 220);
        else if (val > 0.01f)
            color = IM_COL32(255, 120, 0, 80);
        else
            color = IM_COL32(80, 80, 80, 120);

        dl->AddCircleFilled(p, key.radius * (1.0f + val * 0.2f), color);
        dl->AddCircle(p, key.radius, IM_COL32(180, 180, 180, 180), 0, 1.5f);

        // 标签
        ImVec2 textPos = { canvasTL.x + key.textPos.x, canvasTL.y + key.textPos.y - 7.0f };
        dl->AddText(textPos, IM_COL32(200, 200, 200, 255), key.label.c_str());

        // 数值条
        if (key.is_axis && std::abs(rawVal) > 0.01f) {
            float barW = 80.0f;
            float barH = 6.0f;
            ImVec2 barPos = { canvasTL.x + key.textPos.x + 60.0f, canvasTL.y + key.textPos.y - 3.0f };
            dl->AddRectFilled(barPos, { barPos.x + barW, barPos.y + barH }, IM_COL32(50, 50, 50, 200));
            float fillW = std::abs(rawVal) * barW;
            float fillX = (rawVal >= 0) ? barPos.x + barW * 0.5f : barPos.x + barW * 0.5f - fillW;
            dl->AddRectFilled({ fillX, barPos.y }, { fillX + fillW, barPos.y + barH }, IM_COL32(255, 140, 40, 220));
            dl->AddRectFilled({ barPos.x + barW * 0.5f - 1, barPos.y }, { barPos.x + barW * 0.5f + 1, barPos.y + barH }, IM_COL32(200, 200, 200, 180));

            std::string valStr = std::to_string(rawVal).substr(0, 5);
            dl->AddText({ barPos.x + barW + 5, barPos.y - 2 }, IM_COL32(180, 180, 180, 255), valStr.c_str());
        }
    }

    // 绑定标签（竖排在右侧区域）
    float btnHeight = 22.0f;
    float btnSpacing = 4.0f;

    for (const auto& key : keys) {
        const auto& boundList = m_KeyBoundActions[key.name];
        if (boundList.empty()) continue;

        ImVec2 basePos = { canvasTL.x + key.textPos.x, canvasTL.y + key.textPos.y + 16.0f };
        for (size_t i = 0; i < boundList.size(); ++i) {
            ImGui::SetCursorScreenPos({ basePos.x, basePos.y + i * (btnHeight + btnSpacing) });
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 1, 0.5f, 0.8f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            std::string label = boundList[i] + "##" + key.name;
            float w = ImGui::CalcTextSize(boundList[i].c_str()).x + 20.0f;
            if (ImGui::Button(label.c_str(), ImVec2(w, btnHeight))) {
                UnbindKey(boundList[i]);
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }
    }
}

// ======== UI 绘制 ========

void GamepadMapper::DrawGamepadMapper() {
    if (m_Modes.empty() || m_SelectedModeIndex < 0 || m_SelectedModeIndex >= (int)m_Modes.size()) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No mode selected.");
        return;
    }

    GamepadMode& mode = m_Modes[m_SelectedModeIndex];
    ImGui::InputText("Mode Name", mode.name, sizeof(mode.name));

    // 手柄类型选择
    int typeIdx = static_cast<int>(mode.gamepad_type);
    if (ImGui::Combo("Gamepad Type", &typeIdx, "Xbox\0Custom\0\0")) {
        mode.gamepad_type = static_cast<GamepadType>(typeIdx);
    }

    RefreshBoundKeys();
    auto& mappings = mode.mappings;

    ImVec2 avail = ImGui::GetContentRegionAvail();

    // 数字动作区域（两类手柄通用）
    ImGui::TextDisabled("DIGITAL ACTIONS");
    ImGui::BeginChild("##Digital", ImVec2(0, avail.y * 0.22f), true);
    int count = 0;
    for (auto& m : mappings) {
        if (m.is_analog) continue;
        ImGui::PushID(m.key_id);

        bool selected = (m_SelectedKey == m.key_name);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 0.5f, 0, 1));
        else if (m.is_bound) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0.5f, 1));

        if (ImGui::Button(m.key_name.c_str(), ImVec2(180, 45))) m_SelectedKey = m.key_name;

        if (selected || m.is_bound) ImGui::PopStyleColor();

        // 右键菜单
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename")) {
                m_PendingRenameKeyId = m.key_id;
                m_PendingRenameKeyName = m.key_name;
                m_RenamePopupOpen = true;
            }
            if (ImGui::MenuItem("Delete")) {
                m_PendingDeleteKeyId = m.key_id;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        if (++count % 6 != 0) ImGui::SameLine();
    }

    // 添加数字键位（右键空白区域）
    if (ImGui::BeginPopupContextWindow("##DigitalCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Add Digital Key")) {
            int n = 1;
            while (true) {
                std::string name = "Key" + std::to_string(n);
                bool dup = false;
                for (auto& m : mode.mappings)
                    if (m.key_name == name) { dup = true; break; }
                if (!dup) { AddKey(name, false); break; }
                ++n;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::EndChild();

    // 手柄画布（根据类型分支）
    ImGui::BeginChild("##Canvas", ImVec2(0, avail.y * 0.55f), false);
    if (mode.gamepad_type == GamepadType::Xbox) {
        if (!m_GamepadImage) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load gamepad image!");
        } else {
            DrawXboxCanvas();
        }
    } else {
        RebuildCustomKeys();  // 确保显示列表与当前设备同步
        DrawCustomCanvas();
    }
    ImGui::EndChild();

    // 模拟动作区域
    ImGui::TextDisabled("ANALOG ACTIONS");
    ImGui::BeginChild("##Analog", ImVec2(0, 0), true);
    count = 0;
    for (auto& m : mappings) {
        if (!m.is_analog) continue;
        ImGui::PushID(m.key_id);

        bool selected = (m_SelectedKey == m.key_name);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0.6f, 1, 1));
        else if (m.is_bound) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0.5f, 1));

        if (ImGui::Button(m.key_name.c_str(), ImVec2(180, 45))) m_SelectedKey = m.key_name;

        if (selected || m.is_bound) ImGui::PopStyleColor();

        // 右键菜单
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename")) {
                m_PendingRenameKeyId = m.key_id;
                m_PendingRenameKeyName = m.key_name;
                m_RenamePopupOpen = true;
            }
            if (ImGui::MenuItem("Delete")) {
                m_PendingDeleteKeyId = m.key_id;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        if (++count % 6 != 0) ImGui::SameLine();
    }

    // 添加模拟键位（右键空白区域）
    if (ImGui::BeginPopupContextWindow("##AnalogCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Add Analog Key")) {
            int n = 1;
            while (true) {
                std::string name = "Axis" + std::to_string(n);
                bool dup = false;
                for (auto& m : mode.mappings)
                    if (m.key_name == name) { dup = true; break; }
                if (!dup) { AddKey(name, true); break; }
                ++n;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::EndChild();

    // 处理删除
    if (m_PendingDeleteKeyId != 0) {
        RemoveKey(m_PendingDeleteKeyId);
        m_PendingDeleteKeyId = 0;
    }

    // 重命名弹窗
    if (m_RenamePopupOpen) {
        ImGui::OpenPopup("Rename Key");
        m_RenamePopupOpen = false;
    }
    if (ImGui::BeginPopupModal("Rename Key", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        char buf[128];
        strncpy(buf, m_PendingRenameKeyName.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        ImGui::InputText("##RenameInput", buf, sizeof(buf));
        m_PendingRenameKeyName = buf;
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            RenameKey(m_PendingRenameKeyId, m_PendingRenameKeyName);
            m_PendingRenameKeyId = 0;
            m_PendingRenameKeyName.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_PendingRenameKeyId = 0;
            m_PendingRenameKeyName.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}