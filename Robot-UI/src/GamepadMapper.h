#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>
#include "Walnut/Image.h"

enum class GamepadType { Xbox, Custom };
struct KeyInfo {
    ImVec2 pos;              // 在画布上的绘制位置
    ImVec2 textPos;          // 绑定标签显示位置
    float radius;            // 绘制半径
    std::string name;        // 内部唯一名
    std::string label;       // UI 显示标签
    int glfw_id;             // GLFW 按键/轴索引
    bool is_axis;            // true=轴, false=按钮
    bool axis_positive = true; // 仅轴使用：正半轴/负半轴
};

struct GamepadKey {
    int         id = 0;
    std::string name;           // 用户自定义键位名，如 "前进"、"左转"
    bool        is_analog = false;
};

struct KeyMapping {
    int         key_id = 0;       // 对应 GamepadKey::id
    std::string key_name;         // 冗余存储，方便 UI 查找
    std::string gamepad_key;      // 绑定的物理按键名（KeyInfo::name）
    ImVec2      key_pos;          // 物理按键在画布上的位置
    bool        is_bound = false;
    bool        is_analog = false;
};

struct GamepadMode {
    char name[64] = "Default";
    GamepadType gamepad_type = GamepadType::Xbox;  // 每个模式独立的手柄类型
    std::vector<GamepadKey>  keys;                 // 该模式下的所有自定义键位
    std::vector<KeyMapping>  mappings;             // 键位→物理按键绑定
};

class GamepadMapper {
public:
    GamepadMapper();
    ~GamepadMapper();

    void DrawGamepadMapper();
    void UpdateGamepadState();

    int  AddKey(const std::string& keyName, bool isAnalog);
    void RemoveKey(int keyId);
    void RenameKey(int keyId, const std::string& newName);
    const std::vector<GamepadKey>& GetKeys() const;
    void UpdateNextKeyID();

    float GetKeyValue(const std::string& keyName);

    std::vector<std::string> GetActiveModeBoundKeyNames() const;

    void SetGamepadType(GamepadType type);
    GamepadType GetGamepadType() const;
    bool IsCustomConnected() const { return m_CustomPresent; }

    std::vector<std::string> GetModeNames() const;
    int  GetActiveModeIndex() const { return m_ActiveModeIndex; }

    void SetActiveMode(const std::string& name);
    void SetActiveModeByIndex(int index);

    void AddMode();
    void DeleteMode(int index);
    bool ModeExists(const std::string& name) const;

    const std::vector<GamepadMode>& GetModes() const;
    std::vector<GamepadMode>& GetModes();
    int GetSelectedModeIndex() const { return m_SelectedModeIndex; }
    void SetSelectedModeIndex(int idx) { if (idx >= 0 && idx < (int)m_Modes.size()) m_SelectedModeIndex = idx; }

private:
    std::shared_ptr<Walnut::Image> m_GamepadImage;

    void DrawXboxCanvas();
    void DrawCustomCanvas();

    std::map<std::string, float> m_KeyValues;      // 物理按键名 → 当前值（统一强度表）

    // 自定义手柄原始状态
    bool                    m_CustomPresent = false;
    int                     m_CustomButtonCount = 0;
    int                     m_CustomAxisCount = 0;
    std::vector<unsigned char> m_RawButtons;
    std::vector<float>      m_RawAxes;
    std::vector<KeyInfo>    m_CustomPhysicalKeys;   // 自定义手柄物理按键（动态生成）

    static constexpr float k_VisualDeadzone = 0.15f;
    static constexpr float k_BindThreshold  = 0.5f;

    // 返回当前手柄类型对应的物理按键列表
    const std::vector<KeyInfo>& GetActivePhysicalKeys() const;

    void UpdateAllKeyValues();            // 更新 m_KeyValues（每帧调用）
    void RebuildCustomKeys();             // 重建自定义手柄物理按键
    void UpdateRawJoystickState();        // 读取原始 joystick 状态

    std::vector<GamepadMode> m_Modes;
    int m_ActiveModeIndex = 0;
    int m_SelectedModeIndex = 0;    // UI 中当前选中的模式索引

    GamepadMode& GetSelectedMode();
    const GamepadMode& GetSelectedMode() const;

    std::map<std::string, std::vector<std::string>> m_KeyBoundActions; // 物理键 → 绑定的自定义键名列表
    std::string m_SelectedKey;

    int m_NextKeyID = 1;

    // Rename popup state
    bool        m_RenamePopupOpen = false;
    int         m_PendingRenameKeyId = 0;
    std::string m_PendingRenameKeyName;
    int         m_PendingDeleteKeyId = 0;

    void BindKey();
    void UnbindKey(const std::string& keyName);
    void RefreshBoundKeys();

    std::vector<KeyInfo> m_Keys;                       // Xbox 物理按键定义（带画布坐标）
    std::map<std::string, float> m_RawKeyValues;       // 物理按键原始值（未处理，供 GetKeyValue/画布/绑定使用）

    GLFWgamepadstate m_LastState;

    float CalcRawValue(const KeyInfo& key, const GLFWgamepadstate& state);
    float CalcActivation(const KeyInfo& key, float rawVal) const;  // 原始值 → 激活程度 [0,1]
};