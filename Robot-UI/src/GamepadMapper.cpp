#include "GamepadMapper.h"
#include "FileManager.h"

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
    strncpy_s(name, other.name, sizeof(name) - 1);
    strncpy_s(m_RenameBuffer, other.m_RenameBuffer, sizeof(m_RenameBuffer) - 1);
}

GamepadMapper& GamepadMapper::operator=(const GamepadMapper& other)
{
    if (this == &other)
        return *this;

    id = other.id;
    isSelected = other.isSelected;
    strncpy_s(name, other.name, sizeof(name) - 1);
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
    strncpy_s(m_RenameBuffer, other.m_RenameBuffer, sizeof(m_RenameBuffer) - 1);
    m_PendingDeleteKeyId = other.m_PendingDeleteKeyId;
    m_Keys = other.m_Keys;
    m_RawKeyValues = other.m_RawKeyValues;
    m_LastState = other.m_LastState;

    return *this;
}

// 显式移动：因 std::mutex 不可移动，实现为拷贝 + 默认构造 mutex
GamepadMapper::GamepadMapper(GamepadMapper&& other)
    : GamepadMapper()
{
    *this = static_cast<const GamepadMapper&>(other);
}

GamepadMapper& GamepadMapper::operator=(GamepadMapper&& other)
{
    if (this == &other) return *this;
    return operator=(static_cast<const GamepadMapper&>(other));
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

void GamepadMapper::BeginRename(int keyId, const std::string& keyName)
{
    m_Renaming = true;
    m_RenamingKeyId = keyId;
    strncpy_s(m_RenameBuffer, keyName.c_str(), sizeof(m_RenameBuffer) - 1);
}

void GamepadMapper::EndRename(bool confirmed)
{
    if (confirmed && m_RenameBuffer[0] != '\0')
        RenameKey(m_RenamingKeyId, m_RenameBuffer);
    m_Renaming = false;
    m_RenamingKeyId = 0;
    m_RenameBuffer[0] = '\0';
}

void GamepadMapper::ProcessPendingDeletes()
{
    if (m_PendingDeleteKeyId != 0)
    {
        RemoveKey(m_PendingDeleteKeyId);
        m_PendingDeleteKeyId = 0;
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

float GamepadMapper::GetRawKeyValue(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_RawKeyValuesMutex);
    auto it = m_RawKeyValues.find(name);
    return (it != m_RawKeyValues.end()) ? it->second : 0.0f;
}
