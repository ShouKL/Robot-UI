#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>
#include "Walnut/Image.h"

enum class GamepadType { Xbox, Custom };

struct GamepadKey
{
    int id = 0;
    std::string name;
    bool is_analog = false;
};

struct KeyMapping
{
    int key_id = 0;
    std::string key_name;
    std::string gamepad_key;
    ImVec2 key_pos;
    bool is_bound = false;
    bool is_analog = false;
};

struct KeyInfo
{
    ImVec2 pos;
    ImVec2 textPos;
    float radius;
    std::string name;
    std::string label;
    int glfw_id;
    bool is_axis;
    bool axis_positive = true;
};

class GamepadMapper
{
public:
    int id = 0;
    bool isSelected = false;
    char name[64] = "Default";
    GamepadType gamepad_type = GamepadType::Xbox;
    std::vector<GamepadKey> keys;
    std::vector<KeyMapping> mappings;

    GamepadMapper();
    ~GamepadMapper();

    // 显式拷贝：mutex 不可拷贝，需手动实现
    GamepadMapper(const GamepadMapper& other);
    GamepadMapper& operator=(const GamepadMapper& other);

    void UpdateGamepadState();
    float GetKeyValue(const std::string& keyName);
    std::vector<std::string> GetActiveModeBoundKeyNames() const;
    bool IsCustomConnected() const { return m_CustomPresent; }

    int AddKey(const std::string&, bool);
    void RemoveKey(int);
    void RenameKey(int, const std::string&);

    const std::vector<GamepadKey>& GetKeys() const;
    void UpdateNextKeyID();

    int GetSelectedIndex() const { return 0; }
    void SetSelectedIndex(int) {}

    void DrawGamepadMapper();

private:
    void DrawXboxCanvas();
    void DrawCustomCanvas();

    const std::vector<KeyInfo>& GetActivePhysicalKeys() const;
    void UpdateRawJoystickState();
    void RebuildCustomKeys();
    void UpdateAllKeyValues();
    void BindKey();
    void UnbindKey(const std::string&);
    void RefreshBoundKeys();

    float CalcRawValue(const KeyInfo&, const GLFWgamepadstate&);
    float CalcActivation(const KeyInfo&, float) const;

    std::shared_ptr<Walnut::Image> m_GamepadImage;
    std::map<std::string, float> m_KeyValues;
    std::map<std::string, std::vector<std::string>> m_KeyBoundActions;

    std::string m_SelectedKey;
    int m_NextKeyID = 1;

    static constexpr float k_VisualDeadzone = 0.15f;
    static constexpr float k_BindThreshold = 0.5f;

    bool m_CustomPresent = false;
    int m_CustomButtonCount = 0;
    int m_CustomAxisCount = 0;

    std::vector<unsigned char> m_RawButtons;
    std::vector<float> m_RawAxes;
    std::vector<KeyInfo> m_CustomPhysicalKeys;

    bool m_Renaming = false;
    int m_RenamingKeyId = 0;
    char m_RenameBuffer[128] = {};
    int m_PendingDeleteKeyId = 0;

    std::vector<KeyInfo> m_Keys;
    std::map<std::string, float> m_RawKeyValues;
    mutable std::mutex m_RawKeyValuesMutex;

    GLFWgamepadstate m_LastState;
};

// ============================================================================
// GamepadNode — 纯数据（不含手柄状态/mutex/纹理），用于序列化/传递
// ============================================================================
struct GamepadNode
{
    int id = 0;
    bool isSelected = false;
    char name[64] = "Default";
    GamepadType gamepad_type = GamepadType::Xbox;
    std::vector<GamepadKey> keys;
    std::vector<KeyMapping> mappings;
};