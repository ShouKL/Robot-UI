#include "GamepadMapper.h"
#include <algorithm>

GamepadMapper::GamepadMapper() {
    m_GamepadImage = std::make_shared<Walnut::Image>("../asset/picture/gamepadmap.png");

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
    m_KeyBoundActions.clear();
    m_ActiveKeyBoundActions.clear();
    memset(&m_LastState, 0, sizeof(GLFWgamepadstate));
}

GamepadMapper::~GamepadMapper() {}

void GamepadMapper::ApplyMappings() {
    m_ActiveMappings = m_Mappings;
    m_ActiveKeyBoundActions = m_KeyBoundActions;
}

void GamepadMapper::RevertMappings() {
    m_Mappings = m_ActiveMappings;
    m_KeyBoundActions = m_ActiveKeyBoundActions;
    m_SelectedAction.clear();
}

void GamepadMapper::SetMappings(const std::vector<KeyMapping>& mappings) {
    m_Mappings = mappings;
    m_ActiveMappings = m_Mappings;
    m_KeyBoundActions.clear();

    m_ActiveKeyBoundActions = m_KeyBoundActions;
}

void GamepadMapper::BindAction() {
    if (!m_SelectedAction.empty()) {
        for (auto& mapping : m_Mappings) {
            if (mapping.action_name != m_SelectedAction) continue;
            for (const auto& key : m_Keys) {
                if (mapping.is_analog != key.is_axis) continue;
                if (m_ButtonIntensity[key.name] < k_BindThreshold) continue;

                auto& boundList = m_KeyBoundActions[key.name];
                if (boundList.size() >= 2) break;

                UnbindAction(m_SelectedAction);
                mapping.is_bound = true;
                mapping.gamepad_key = key.name;
                mapping.key_pos = key.pos;
                boundList.push_back(m_SelectedAction);
                m_SelectedAction.clear();
                break;
            }
        }
    }
}

void GamepadMapper::UnbindAction(const std::string& actionName) {
    for (auto& mapping : m_Mappings) {
        if (mapping.action_name == actionName && mapping.is_bound) {
            auto& boundList = m_KeyBoundActions[mapping.gamepad_key];
            auto it = std::find(boundList.begin(), boundList.end(), actionName);
            if (it != boundList.end()) boundList.erase(it);

            mapping.is_bound = false;
            mapping.gamepad_key.clear();
            mapping.key_pos = {};
            break;
        }
    }
}

float GamepadMapper::CalculateIntensity(const KeyInfo& key, const GLFWgamepadstate& state){
    float intensity = 0.0f;

    if (!key.is_axis) {
        return (state.buttons[key.glfw_id] != GLFW_RELEASE) ? 1.0f : 0.0f;
    }
    if (key.glfw_id == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER || key.glfw_id == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        intensity = (state.axes[key.glfw_id] + 1.0f) * 0.5f;
    }
    else if (key.name == "L_Stick") {
        intensity = std::max(std::abs(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X]),
            std::abs(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]));
    }
    else if (key.name == "R_Stick") {
        intensity = std::max(std::abs(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]),
            std::abs(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]));
    }
    return (intensity < k_VisualDeadzone) ? 0.0f : intensity;
}

void GamepadMapper::UpdateGamepadState() {
    GLFWgamepadstate state;
    if (!glfwJoystickPresent(GLFW_JOYSTICK_1) || !glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) return;

    for (const auto& key : m_Keys) {
        m_ButtonIntensity[key.name] = CalculateIntensity(key, state);
    }

    BindAction();

    m_LX = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
    m_LY = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
    m_RX = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
    m_RY = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
}

void GamepadMapper::DrawGamepadMapper() {
    if (!m_GamepadImage) return;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float imgW = (float)m_GamepadImage->GetWidth();
    float imgH = (float)m_GamepadImage->GetHeight();

    if (imgW <= 0 || imgH <= 0) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load gamepad image! Check path: ../asset/picture/gamepadmap.png");
        imgW = 800;
        imgH = 600;
    }

    ImGui::TextDisabled("DIGITAL ACTIONS");
    ImGui::BeginChild("##Digital", ImVec2(0, avail.y * 0.22f), true);
    int count = 0;
    for (auto& m : m_Mappings) {
        if (m.is_analog) continue;
        bool selected = (m_SelectedAction == m.action_name);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 0.5f, 0, 1));
        else if (m.is_bound) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0.5f, 1));

        if (ImGui::Button(m.action_name.c_str(), ImVec2(180, 45))) m_SelectedAction = m.action_name;

        if (selected || m.is_bound) ImGui::PopStyleColor();
        if (++count % 6 != 0) ImGui::SameLine();
    }
    ImGui::EndChild();

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

    for (const auto& key : m_Keys) {
        float val = m_ButtonIntensity[key.name];
        if (val > 0) {
            ImVec2 p = { imgOrigin.x + key.pos.x * scale, imgOrigin.y + key.pos.y * scale };
            dl->AddCircleFilled(p, key.radius * scale * (1 + val * 0.15f), IM_COL32(255, 120, 0, (int)(30 + val * 180)));
        }
    }

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

                if (ImGui::Button((actionName + "##" + key.name).c_str(), ImVec2(maxWidth, btnHeight))) {
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

                if (ImGui::Button((boundList[i] + "##" + key.name).c_str(), ImVec2(widths[i], btnHeight))) {
                    UnbindAction(boundList[i]);
                }

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
            }
        }
    }
    ImGui::EndChild();

    ImGui::TextDisabled("ANALOG ACTIONS");
    ImGui::BeginChild("##Analog", ImVec2(0, 0), true);
    count = 0;
    for (auto& m : m_Mappings) {
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

float GamepadMapper::GetActionValue(const std::string& actionName){
    for (const auto& mapping : m_ActiveMappings) {
        if (mapping.action_name == actionName && mapping.is_bound) {
            if (m_ButtonIntensity.count(mapping.gamepad_key)) {
                return m_ButtonIntensity.at(mapping.gamepad_key);
            }
        }
    }
    return 0.0f;
}