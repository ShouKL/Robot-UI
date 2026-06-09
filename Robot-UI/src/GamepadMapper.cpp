#include "GamepadMapper.h"
#include "FileManager.h"
#include "Walnut/Core/Log.h"
#include <cmath>

// ============================================================================
// 构造 / 析构 / 拷贝
// ============================================================================

GamepadMapper::GamepadMapper()
{
    m_Keys = std::vector<KeyInfo>{
        {ImVec2(938, 340), ImVec2(1250, 351), 17, "Button_A",      "A",    GLFW_GAMEPAD_BUTTON_A,              false},
        {ImVec2(987, 300), ImVec2(1250, 283), 17, "Button_B",      "B",    GLFW_GAMEPAD_BUTTON_B,              false},
        {ImVec2(898, 310), ImVec2(1250, 418), 17, "Button_X",      "X",    GLFW_GAMEPAD_BUTTON_X,              false},
        {ImVec2(945, 265), ImVec2(1250, 218), 17, "Button_Y",      "Y",    GLFW_GAMEPAD_BUTTON_Y,              false},
        {ImVec2(550, 205), ImVec2(312,  154), 17, "LB",            "LB",   GLFW_GAMEPAD_BUTTON_LEFT_BUMPER,   false},
        {ImVec2(995, 205), ImVec2(1250, 154), 17, "RB",            "RB",   GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER,  false},
        {ImVec2(595, 327), ImVec2(312,  314), 15, "L_Stick_Click", "L3",   GLFW_GAMEPAD_BUTTON_LEFT_THUMB,     false},
        {ImVec2(857, 416), ImVec2(1250, 556), 15, "R_Stick_Click", "R3",   GLFW_GAMEPAD_BUTTON_RIGHT_THUMB,    false},
        {ImVec2(723, 309), ImVec2(655,  548), 15, "Button_View",   "View", GLFW_GAMEPAD_BUTTON_BACK,            false},
        {ImVec2(818, 309), ImVec2(879,  548), 15, "Button_Menu",   "Menu", GLFW_GAMEPAD_BUTTON_START,           false},
        {ImVec2(685, 370), ImVec2(312,  350), 13, "DPad_Up",       "U",    GLFW_GAMEPAD_BUTTON_DPAD_UP,         false},
        {ImVec2(685, 420), ImVec2(312,  485), 13, "DPad_Down",     "D",    GLFW_GAMEPAD_BUTTON_DPAD_DOWN,       false},
        {ImVec2(655, 395), ImVec2(312,  439), 13, "DPad_Left",     "L",    GLFW_GAMEPAD_BUTTON_DPAD_LEFT,       false},
        {ImVec2(715, 395), ImVec2(312,  396), 13, "DPad_Right",    "R",    GLFW_GAMEPAD_BUTTON_DPAD_RIGHT,      false},
        {ImVec2(575, 160), ImVec2(312,   80), 20, "LT",            "LT",   GLFW_GAMEPAD_AXIS_LEFT_TRIGGER,      true},
        {ImVec2(960, 160), ImVec2(1250,  80), 20, "RT",            "RT",   GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER,     true},
        {ImVec2(595, 327), ImVec2(312,  205), 30, "L_Stick_X",     "LS X", GLFW_GAMEPAD_AXIS_LEFT_X,           true},
        {ImVec2(595, 327), ImVec2(312,  265), 30, "L_Stick_Y",     "LS Y", GLFW_GAMEPAD_AXIS_LEFT_Y,           true},
        {ImVec2(857, 416), ImVec2(1250, 455), 30, "R_Stick_X",     "RS X", GLFW_GAMEPAD_AXIS_RIGHT_X,          true},
        {ImVec2(857, 416), ImVec2(1250, 515), 30, "R_Stick_Y",     "RS Y", GLFW_GAMEPAD_AXIS_RIGHT_Y,          true}
    };
    memset(&m_LastState, 0, sizeof(GLFWgamepadstate));
}

GamepadMapper::~GamepadMapper() {}

GamepadMapper::GamepadMapper(const GamepadMapper& other)
    : id(other.id)
    , isSelected(other.isSelected)
    , gamepad_type(other.gamepad_type)
    , keys(other.keys)
    , mappings(other.mappings)
    , m_GamepadImage(other.m_GamepadImage)
    , m_KeyValues(other.m_KeyValues)
    , m_KeyBoundActions(other.m_KeyBoundActions)
    , m_SelectedKey(other.m_SelectedKey)
    , m_NextKeyID(other.m_NextKeyID)
    , m_CustomPresent(other.m_CustomPresent)
    , m_CustomButtonCount(other.m_CustomButtonCount)
    , m_CustomAxisCount(other.m_CustomAxisCount)
    , m_RawButtons(other.m_RawButtons)
    , m_RawAxes(other.m_RawAxes)
    , m_CustomPhysicalKeys(other.m_CustomPhysicalKeys)
    , m_Renaming(other.m_Renaming)
    , m_RenamingKeyId(other.m_RenamingKeyId)
    , m_PendingDeleteKeyId(other.m_PendingDeleteKeyId)
    , m_Keys(other.m_Keys)
    , m_RawKeyValues(other.m_RawKeyValues)
    , m_LastState(other.m_LastState)
{
    strncpy(name, other.name, sizeof(name));
    strncpy(m_RenameBuffer, other.m_RenameBuffer, sizeof(m_RenameBuffer));
}

GamepadMapper& GamepadMapper::operator=(const GamepadMapper& other)
{
    if (this == &other)
        return *this;

    id = other.id;
    isSelected = other.isSelected;
    strncpy(name, other.name, sizeof(name));
    gamepad_type = other.gamepad_type;

    keys = other.keys;
    mappings = other.mappings;

    m_GamepadImage = other.m_GamepadImage;
    m_KeyValues = other.m_KeyValues;
    m_KeyBoundActions = other.m_KeyBoundActions;
    m_SelectedKey = other.m_SelectedKey;
    m_NextKeyID = other.m_NextKeyID;
    m_CustomPresent = other.m_CustomPresent;
    m_CustomButtonCount = other.m_CustomButtonCount;
    m_CustomAxisCount = other.m_CustomAxisCount;
    m_RawButtons = other.m_RawButtons;
    m_RawAxes = other.m_RawAxes;
    m_CustomPhysicalKeys = other.m_CustomPhysicalKeys;
    m_Renaming = other.m_Renaming;
    m_RenamingKeyId = other.m_RenamingKeyId;
    strncpy(m_RenameBuffer, other.m_RenameBuffer, sizeof(m_RenameBuffer));
    m_PendingDeleteKeyId = other.m_PendingDeleteKeyId;
    m_Keys = other.m_Keys;
    m_RawKeyValues = other.m_RawKeyValues;
    m_LastState = other.m_LastState;

    return *this;
}

// ============================================================================
// 按键管理
// ============================================================================

int GamepadMapper::AddKey(const std::string& keyName, bool isAnalog)
{
    GamepadKey key;
    key.id = m_NextKeyID++;
    key.name = keyName;
    key.is_analog = isAnalog;
    keys.push_back(key);

    KeyMapping km;
    km.key_id = key.id;
    km.key_name = keyName;
    km.is_bound = false;
    km.is_analog = isAnalog;
    mappings.push_back(km);

    return key.id;
}

void GamepadMapper::RemoveKey(int keyId)
{
    keys.erase(
        std::remove_if(keys.begin(), keys.end(),
            [keyId](const GamepadKey& k) { return k.id == keyId; }),
        keys.end());

    mappings.erase(
        std::remove_if(mappings.begin(), mappings.end(),
            [keyId](const KeyMapping& m) { return m.key_id == keyId; }),
        mappings.end());
}

void GamepadMapper::RenameKey(int keyId, const std::string& newName)
{
    for (auto& k : keys)
    {
        if (k.id == keyId) { k.name = newName; break; }
    }
    for (auto& m : mappings)
    {
        if (m.key_id == keyId) { m.key_name = newName; break; }
    }
}

const std::vector<GamepadKey>& GamepadMapper::GetKeys() const
{
    return keys;
}

void GamepadMapper::UpdateNextKeyID()
{
    int maxId = 0;
    for (auto& k : keys)
        if (k.id > maxId) maxId = k.id;
    m_NextKeyID = maxId + 1;
}

// ============================================================================
// 按键绑定 / 状态查询
// ============================================================================

void GamepadMapper::BindKey()
{
    if (m_SelectedKey.empty())
        return;

    const auto& activeKeys = GetActivePhysicalKeys();

    for (auto& mapping : mappings)
    {
        if (mapping.key_name != m_SelectedKey)
            continue;

        for (const auto& key : activeKeys)
        {
            if (mapping.is_analog != key.is_axis)
                continue;

            float rawVal = m_RawKeyValues[key.name];
            float val = CalcActivation(key, rawVal);
            if (val < k_BindThreshold)
                continue;

            auto& boundList = m_KeyBoundActions[key.name];
            if (boundList.size() >= 2)
                break;

            UnbindKey(m_SelectedKey);

            mapping.is_bound = true;
            mapping.gamepad_key = key.name;
            mapping.key_pos = key.pos;
            boundList.push_back(m_SelectedKey);

            WL_INFO_TAG("GAMEPAD", "Key bound: {} -> {}", m_SelectedKey, key.name);

            m_SelectedKey.clear();
            break;
        }

        if (m_SelectedKey.empty())
            break;
    }

    RefreshBoundKeys();
}

void GamepadMapper::UnbindKey(const std::string& keyName)
{
    for (auto& mapping : mappings)
    {
        if (mapping.key_name == keyName && mapping.is_bound)
        {
            if (!mapping.gamepad_key.empty())
            {
                auto& bl = m_KeyBoundActions[mapping.gamepad_key];
                auto it = std::find(bl.begin(), bl.end(), keyName);
                if (it != bl.end())
                    bl.erase(it);
            }

            mapping.is_bound = false;
            mapping.gamepad_key.clear();
            mapping.key_pos = ImVec2();
            break;
        }
    }
}

void GamepadMapper::RefreshBoundKeys()
{
    m_KeyBoundActions.clear();
    for (auto& m : mappings)
    {
        if (m.is_bound && !m.gamepad_key.empty())
            m_KeyBoundActions[m.gamepad_key].push_back(m.key_name);
    }
}

float GamepadMapper::CalcRawValue(const KeyInfo& key, const GLFWgamepadstate& state)
{
    if (!key.is_axis)
        return (state.buttons[key.glfw_id] != GLFW_RELEASE) ? 1.0f : 0.0f;
    return state.axes[key.glfw_id];
}

float GamepadMapper::CalcActivation(const KeyInfo& key, float rawVal) const
{
    if (!key.is_axis)
        return rawVal;

    if (key.glfw_id == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER ||
        key.glfw_id == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER)
        return (rawVal + 1.0f) * 0.5f;

    return std::abs(rawVal);
}

void GamepadMapper::UpdateGamepadState()
{
    UpdateRawJoystickState();
    UpdateAllKeyValues();
    BindKey();
}

float GamepadMapper::GetKeyValue(const std::string& keyName)
{
    std::lock_guard<std::mutex> lock(m_RawKeyValuesMutex);

    for (auto& m : mappings)
    {
        if (m.key_name == keyName && m.is_bound)
        {
            if (m_RawKeyValues.count(m.gamepad_key))
                return m_RawKeyValues.at(m.gamepad_key);
        }
    }

    if (m_RawKeyValues.count(keyName))
        return m_RawKeyValues.at(keyName);

    // 别名映射
    if (keyName == "LX")  return m_RawKeyValues["L_Stick_X"];
    if (keyName == "LY")  return m_RawKeyValues["L_Stick_Y"];
    if (keyName == "RX")  return m_RawKeyValues["R_Stick_X"];
    if (keyName == "RY")  return m_RawKeyValues["R_Stick_Y"];

    return 0.0f;
}

std::vector<std::string> GamepadMapper::GetActiveModeBoundKeyNames() const
{
    std::vector<std::string> names;
    for (auto& m : mappings)
        if (m.is_bound)
            names.push_back(m.key_name);
    return names;
}

const std::vector<KeyInfo>& GamepadMapper::GetActivePhysicalKeys() const
{
    if (gamepad_type == GamepadType::Custom)
        return m_CustomPhysicalKeys;
    return m_Keys;
}

// ============================================================================
// 原始摇杆状态 / 自定义按键
// ============================================================================

void GamepadMapper::UpdateRawJoystickState()
{
    m_CustomPresent = false;

    int count;
    const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &count);
    if (!buttons)
        return;

    m_CustomPresent = true;
    m_CustomButtonCount = count;
    m_RawButtons.assign(buttons, buttons + count);

    const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count);
    if (axes)
    {
        m_CustomAxisCount = count;
        m_RawAxes.assign(axes, axes + count);
    }
    else
    {
        m_CustomAxisCount = 0;
        m_RawAxes.clear();
    }
}

void GamepadMapper::RebuildCustomKeys()
{
    m_CustomPhysicalKeys.clear();
    if (!m_CustomPresent)
        return;

    float y = 50;
    for (int i = 0; i < m_CustomButtonCount; ++i)
    {
        KeyInfo k;
        k.name    = "Btn_" + std::to_string(i);
        k.label   = "B" + std::to_string(i);
        k.glfw_id = i;
        k.is_axis = false;
        k.pos     = ImVec2(40, y);
        k.textPos = ImVec2(200, y);
        k.radius  = 8;
        m_CustomPhysicalKeys.push_back(k);
        y += 22;
    }

    y = 50;
    for (int i = 0; i < m_CustomAxisCount; ++i)
    {
        KeyInfo k;
        k.name    = "Axis_" + std::to_string(i);
        k.label   = "A" + std::to_string(i);
        k.glfw_id = i;
        k.is_axis = true;
        k.pos     = ImVec2(400, y);
        k.textPos = ImVec2(560, y);
        k.radius  = 10;
        m_CustomPhysicalKeys.push_back(k);
        y += 22;
    }
}

void GamepadMapper::UpdateAllKeyValues()
{
    std::lock_guard<std::mutex> lock(m_RawKeyValuesMutex);
    m_RawKeyValues.clear();

    GLFWgamepadstate state;
    if (glfwJoystickPresent(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
    {
        for (auto& key : m_Keys)
            m_RawKeyValues[key.name] = CalcRawValue(key, state);
    }

    if (m_CustomPresent)
    {
        for (int i = 0; i < m_CustomButtonCount; ++i)
            m_RawKeyValues["Btn_" + std::to_string(i)] = (m_RawButtons[i] != 0) ? 1.0f : 0.0f;

        for (int i = 0; i < m_CustomAxisCount; ++i)
            m_RawKeyValues["Axis_" + std::to_string(i)] = m_RawAxes[i];
    }
}


// ============================================================================
// Xbox 手柄画布
// ============================================================================

void GamepadMapper::DrawXboxCanvas()
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float imgW = (float)m_GamepadImage->GetWidth();
    float imgH = (float)m_GamepadImage->GetHeight();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasTL = ImGui::GetCursorScreenPos();
    ImVec2 canvasSz = avail;

    float scale = std::min(canvasSz.x / imgW, canvasSz.y / imgH);
    ImVec2 renderSz = { imgW * scale, imgH * scale };
    ImVec2 offset = {
        (canvasSz.x - renderSz.x) * 0.5f,
        (canvasSz.y - renderSz.y) * 0.5f
    };
    ImVec2 imgOrigin = { canvasTL.x + offset.x, canvasTL.y + offset.y };

    ImGui::SetCursorScreenPos(imgOrigin);
    ImGui::Image(m_GamepadImage->GetDescriptorSet(), renderSz);

    // --- 绘制按键高亮 ---
    for (auto& key : m_Keys)
    {
        // 跳过摇杆轴（单独绘制）
        if (key.name == "L_Stick_X" || key.name == "L_Stick_Y" ||
            key.name == "R_Stick_X" || key.name == "R_Stick_Y")
            continue;

        float rawVal = m_RawKeyValues[key.name];
        float val = CalcActivation(key, rawVal);

        if (val > 0.01f)
        {
            ImVec2 p = {
                imgOrigin.x + key.pos.x * scale,
                imgOrigin.y + key.pos.y * scale
            };
            dl->AddCircleFilled(p,
                key.radius * scale * (1.0f + val * 0.15f),
                IM_COL32(255, 120, 0, (int)(30 + val * 180)));
        }
    }

    // --- 绘制摇杆 ---
    {
        float lx = m_RawKeyValues["L_Stick_X"];
        float ly = m_RawKeyValues["L_Stick_Y"];
        float rx = m_RawKeyValues["R_Stick_X"];
        float ry = m_RawKeyValues["R_Stick_Y"];
        float lMag = std::sqrt(lx * lx + ly * ly);
        float rMag = std::sqrt(rx * rx + ry * ry);
        float stickTravel = 30;

        // 左摇杆
        {
            ImVec2 center = { imgOrigin.x + 595 * scale, imgOrigin.y + 327 * scale };
            if (lMag > 0.01f)
            {
                dl->AddCircleFilled(center,
                    30 * scale * (1.0f + lMag * 0.15f),
                    IM_COL32(255, 120, 0, (int)(30 + lMag * 180)));
            }
            ImVec2 dot = {
                center.x + lx * stickTravel * scale,
                center.y + ly * stickTravel * scale
            };
            dl->AddCircleFilled(dot, 4 * scale, IM_COL32(255, 60, 40, 220));
        }

        // 右摇杆
        {
            ImVec2 center = { imgOrigin.x + 857 * scale, imgOrigin.y + 416 * scale };
            if (rMag > 0.01f)
            {
                dl->AddCircleFilled(center,
                    30 * scale * (1.0f + rMag * 0.15f),
                    IM_COL32(255, 120, 0, (int)(30 + rMag * 180)));
            }
            ImVec2 dot = {
                center.x + rx * stickTravel * scale,
                center.y + ry * stickTravel * scale
            };
            dl->AddCircleFilled(dot, 4 * scale, IM_COL32(255, 60, 40, 220));
        }
    }

    // --- 绘制已绑定的按键标签 ---
    float btnHeight = 26 * scale;
    float btnSpacing = 5 * scale;
    float horizontalPadding = 10 * scale;

    for (auto& key : m_Keys)
    {
        auto& boundList = m_KeyBoundActions[key.name];
        if (boundList.empty())
            continue;

        bool isVertical = (key.name == "Button_View" ||
                           key.name == "Button_Menu" ||
                           key.name == "L_Stick_X" ||
                           key.name == "L_Stick_Y" ||
                           key.name == "R_Stick_X" ||
                           key.name == "R_Stick_Y");

        ImVec2 basePos = {
            imgOrigin.x + key.textPos.x * scale,
            imgOrigin.y + key.textPos.y * scale
        };

        if (isVertical)
        {
            float maxWidth = 0;
            for (auto& a : boundList)
            {
                float w = ImGui::CalcTextSize(a.c_str()).x + (horizontalPadding * 2);
                if (w > maxWidth) maxWidth = w;
            }

            float totalHeight = (btnHeight * boundList.size()) +
                                (btnSpacing * (boundList.size() - 1));
            ImVec2 currentPos = {
                basePos.x - (maxWidth * 0.5f),
                basePos.y - (totalHeight * 0.5f)
            };

            for (auto& actionName : boundList)
            {
                ImGui::SetCursorScreenPos(currentPos);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 1, 0.5f, 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4 * scale);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

                std::string label = actionName + "##" + key.name;
                if (ImGui::Button(label.c_str(), ImVec2(maxWidth, btnHeight)))
                    UnbindKey(actionName);

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
                currentPos.y += btnHeight + btnSpacing;
            }
        }
        else
        {
            float totalWidth = 0;
            std::vector<float> widths;
            for (auto& a : boundList)
            {
                float w = ImGui::CalcTextSize(a.c_str()).x + (horizontalPadding * 2);
                widths.push_back(w);
                totalWidth += w;
            }
            totalWidth += btnSpacing * (boundList.size() - 1);

            ImVec2 startPos = {
                basePos.x - (totalWidth * 0.5f),
                basePos.y - (btnHeight * 0.5f)
            };
            ImGui::SetCursorScreenPos(startPos);

            for (size_t i = 0; i < boundList.size(); ++i)
            {
                if (i > 0)
                    ImGui::SameLine(0, btnSpacing);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 1, 0.5f, 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4 * scale);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

                std::string label = boundList[i] + "##" + key.name;
                if (ImGui::Button(label.c_str(), ImVec2(widths[i], btnHeight)))
                    UnbindKey(boundList[i]);

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
            }
        }
    }
}

// ============================================================================
// 自定义手柄画布
// ============================================================================

void GamepadMapper::DrawCustomCanvas()
{
    auto& keys = m_CustomPhysicalKeys;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasTL = ImGui::GetCursorScreenPos();
    ImVec2 canvasSz = ImGui::GetContentRegionAvail();

    for (auto& key : keys)
    {
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

        ImVec2 textPos = {
            canvasTL.x + key.textPos.x,
            canvasTL.y + key.textPos.y - 7
        };
        dl->AddText(textPos, IM_COL32(200, 200, 200, 255), key.label.c_str());

        // 轴值进度条
        if (key.is_axis && std::abs(rawVal) > 0.01f)
        {
            float barW = 80;
            float barH = 6;
            ImVec2 barPos = {
                canvasTL.x + key.textPos.x + 60,
                canvasTL.y + key.textPos.y - 3
            };

            dl->AddRectFilled(barPos,
                { barPos.x + barW, barPos.y + barH },
                IM_COL32(50, 50, 50, 200));

            float fillW = std::abs(rawVal) * barW;
            float fillX = (rawVal >= 0)
                ? barPos.x + barW * 0.5f
                : barPos.x + barW * 0.5f - fillW;
            dl->AddRectFilled(
                { fillX, barPos.y },
                { fillX + fillW, barPos.y + barH },
                IM_COL32(255, 140, 40, 220));

            // 中心线
            dl->AddRectFilled(
                { barPos.x + barW * 0.5f - 1, barPos.y },
                { barPos.x + barW * 0.5f + 1, barPos.y + barH },
                IM_COL32(200, 200, 200, 180));

            std::string valStr = std::to_string(rawVal).substr(0, 5);
            dl->AddText(
                { barPos.x + barW + 5, barPos.y - 2 },
                IM_COL32(180, 180, 180, 255),
                valStr.c_str());
        }
    }

    // --- 已绑定的按键标签 ---
    float btnHeight = 22;
    float btnSpacing = 4;

    for (auto& key : keys)
    {
        auto& boundList = m_KeyBoundActions[key.name];
        if (boundList.empty())
            continue;

        ImVec2 basePos = {
            canvasTL.x + key.textPos.x,
            canvasTL.y + key.textPos.y + 16
        };

        for (size_t i = 0; i < boundList.size(); ++i)
        {
            ImGui::SetCursorScreenPos({
                basePos.x,
                basePos.y + i * (btnHeight + btnSpacing)
            });

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 1, 0.5f, 0.8f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

            std::string label = boundList[i] + "##" + key.name;
            float w = ImGui::CalcTextSize(boundList[i].c_str()).x + 20;
            if (ImGui::Button(label.c_str(), ImVec2(w, btnHeight)))
                UnbindKey(boundList[i]);

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }
    }
}


// ============================================================================
// 主绘制入口
// ============================================================================

void GamepadMapper::DrawGamepadMapper()
{
    // --- 手柄类型选择 ---
    int typeIdx = (int)gamepad_type;
    if (ImGui::Combo("Gamepad Type", &typeIdx, "Xbox\0Custom\0\0"))
        gamepad_type = (GamepadType)typeIdx;

    RefreshBoundKeys();
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // --- 数字按键区域 ---
    ImGui::TextDisabled("DIGITAL ACTIONS");
    ImGui::BeginChild("##Digital", ImVec2(0, avail.y * 0.22f), true);

    int count = 0;
    for (auto& m : mappings)
    {
        if (m.is_analog)
            continue;

        ImGui::PushID(m.key_id);

        // --- 内联改名模式 ---
        if (m_Renaming && m_RenamingKeyId == m.key_id)
        {
            ImGui::SetNextItemWidth(180);
            bool confirm = ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer),
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            bool canceled = ImGui::IsItemDeactivatedAfterEdit()
                            && !ImGui::IsKeyDown(ImGuiKey_Enter)
                            && !ImGui::IsKeyDown(ImGuiKey_KeypadEnter);
            if (confirm || canceled)
            {
                if (confirm && m_RenameBuffer[0] != '\0')
                    RenameKey(m_RenamingKeyId, m_RenameBuffer);
                m_Renaming = false;
                m_RenamingKeyId = 0;
                m_RenameBuffer[0] = '\0';
            }
        }
        else
        {
            bool selected = (m_SelectedKey == m.key_name);
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 0.5f, 0, 1));
            else if (m.is_bound)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0.5f, 1));

            if (ImGui::Button(m.key_name.c_str(), ImVec2(180, 45)))
                m_SelectedKey = m.key_name;

            // 双击进入内联改名
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_Renaming = true;
                m_RenamingKeyId = m.key_id;
                strncpy_s(m_RenameBuffer, m.key_name.c_str(), sizeof(m_RenameBuffer) - 1);
            }

            if (selected || m.is_bound)
                ImGui::PopStyleColor();
        }

        // 右键菜单（仅保留删除）
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete"))
                m_PendingDeleteKeyId = m.key_id;
            ImGui::EndPopup();
        }

        ImGui::PopID();

        if (++count % 6 != 0)
            ImGui::SameLine();
    }

    // 右键空白区域添加按键
    if (ImGui::BeginPopupContextWindow("##DigitalCtx",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem("Add Digital Key"))
        {
            int n = 1;
            while (true)
            {
                std::string name = "Key" + std::to_string(n);
                bool dup = false;
                for (auto& m : mappings)
                    if (m.key_name == name) { dup = true; break; }
                if (!dup) { AddKey(name, false); break; }
                ++n;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::EndChild();

    // --- 手柄画布 ---
    ImGui::BeginChild("##Canvas", ImVec2(0, avail.y * 0.55f), false);

    if (gamepad_type == GamepadType::Xbox)
    {
        if (!m_GamepadImage)
            m_GamepadImage = std::make_shared<Walnut::Image>(FileManager::GetExeDir() + "..\\..\\..\\asset\\picture\\gamepadmap.png");

        if (!m_GamepadImage)
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load gamepad image!");
        else
            DrawXboxCanvas();
    }
    else
    {
        RebuildCustomKeys();
        DrawCustomCanvas();
    }

    ImGui::EndChild();

    // --- 模拟按键区域 ---
    ImGui::TextDisabled("ANALOG ACTIONS");
    ImGui::BeginChild("##Analog", ImVec2(0, 0), true);

    count = 0;
    for (auto& m : mappings)
    {
        if (!m.is_analog)
            continue;

        ImGui::PushID(m.key_id);

        // --- 内联改名模式 ---
        if (m_Renaming && m_RenamingKeyId == m.key_id)
        {
            ImGui::SetNextItemWidth(180);
            bool confirm = ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer),
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            bool canceled = ImGui::IsItemDeactivatedAfterEdit()
                            && !ImGui::IsKeyDown(ImGuiKey_Enter)
                            && !ImGui::IsKeyDown(ImGuiKey_KeypadEnter);
            if (confirm || canceled)
            {
                if (confirm && m_RenameBuffer[0] != '\0')
                    RenameKey(m_RenamingKeyId, m_RenameBuffer);
                m_Renaming = false;
                m_RenamingKeyId = 0;
                m_RenameBuffer[0] = '\0';
            }
        }
        else
        {
            bool selected = (m_SelectedKey == m.key_name);
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0.6f, 1, 1));
            else if (m.is_bound)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0.5f, 1));

            if (ImGui::Button(m.key_name.c_str(), ImVec2(180, 45)))
                m_SelectedKey = m.key_name;

            // 双击进入内联改名
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_Renaming = true;
                m_RenamingKeyId = m.key_id;
                strncpy_s(m_RenameBuffer, m.key_name.c_str(), sizeof(m_RenameBuffer) - 1);
            }

            if (selected || m.is_bound)
                ImGui::PopStyleColor();
        }

        // 右键菜单（仅保留删除）
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete"))
                m_PendingDeleteKeyId = m.key_id;
            ImGui::EndPopup();
        }

        ImGui::PopID();

        if (++count % 6 != 0)
            ImGui::SameLine();
    }

    // 右键空白区域添加模拟轴
    if (ImGui::BeginPopupContextWindow("##AnalogCtx",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem("Add Analog Key"))
        {
            int n = 1;
            while (true)
            {
                std::string name = "Axis" + std::to_string(n);
                bool dup = false;
                for (auto& m : mappings)
                    if (m.key_name == name) { dup = true; break; }
                if (!dup) { AddKey(name, true); break; }
                ++n;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::EndChild();

    // --- 延迟删除 ---
    if (m_PendingDeleteKeyId != 0)
    {
        RemoveKey(m_PendingDeleteKeyId);
        m_PendingDeleteKeyId = 0;
    }
}