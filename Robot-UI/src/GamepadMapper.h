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
struct KeyInfo {
    ImVec2 pos;              // 在画布上的绘制位置
    ImVec2 textPos;          // 绑定标签显示位置
    float radius;            // 绘制半径
    std::string name;        // 内部唯一名 (如 "Button_A", "L_Stick_X", "Axis_0+")
    std::string label;       // UI 显示标签 (如 "A", "LSX", "+0")
    int glfw_id;             // GLFW 按键/轴索引
    bool is_axis;            // true=轴, false=按钮
    bool axis_positive = true; // 仅轴使用：正半轴/负半轴
};

// ============================================================
// 自定义键位（用户可命名、增删）
// ============================================================
struct GamepadKey {
    int         id = 0;
    std::string name;           // 用户自定义键位名，如 "前进"、"左转"
    bool        is_analog = false;
};

// ============================================================
// 自定义键位 → 物理按键的绑定关系
// ============================================================
struct KeyMapping {
    int         key_id = 0;       // 对应 GamepadKey::id
    std::string key_name;         // 冗余存储，方便 UI 查找
    std::string gamepad_key;      // 绑定的物理按键名（KeyInfo::name）
    ImVec2      key_pos;          // 物理按键在画布上的位置
    bool        is_bound = false;
    bool        is_analog = false;
};

// ============================================================
// 手柄映射模式（一组自定义键位 + 绑定）
// ============================================================
struct GamepadMode {
    char name[64] = "Default";
    GamepadType gamepad_type = GamepadType::Xbox;  // 每个模式独立的手柄类型
    std::vector<GamepadKey>  keys;                 // 该模式下的所有自定义键位
    std::vector<KeyMapping>  mappings;             // 键位→物理按键绑定
};

// ============================================================
// GamepadMapper — 三层架构
//   1) Xbox 画布    — 负责画手柄图
//   2) 通用物理按键 — 统一管理所有物理按键、状态更新、值查询
//   3) 自定义键位   — 用户键位增删改、绑定/解绑、模式管理
// ============================================================
class GamepadMapper {
public:
    GamepadMapper();
    ~GamepadMapper();

    void DrawGamepadMapper();
    void UpdateGamepadState();

    // ========== 自定义键位层 ==========
    int  AddKey(const std::string& keyName, bool isAnalog);
    void RemoveKey(int keyId);
    void RenameKey(int keyId, const std::string& newName);
    const std::vector<GamepadKey>& GetKeys() const;
    void UpdateNextKeyID();

    // 通过键位名获取当前手柄值
    float GetKeyValue(const std::string& keyName);

    // 线程安全地获取当前活动模式下所有已绑定键位名列表（供 GamepadThread 使用）
    std::vector<std::string> GetActiveModeBoundKeyNames() const;

    // ========== 手柄类型（操作当前编辑/活动模式） ==========
    void SetGamepadType(GamepadType type);
    GamepadType GetGamepadType() const;
    bool IsCustomConnected() const { return m_CustomPresent; }

    // ========== 模式管理 ==========
    std::vector<std::string> GetModeNames() const;
    int  GetActiveModeIndex() const { return m_ActiveModeIndex; }

    void SetActiveMode(const std::string& name);
    void SetActiveModeByIndex(int index);
    std::string GetActiveModeName() const {
        return m_Modes.empty() ? std::string() : std::string(m_Modes[m_ActiveModeIndex].name);
    }

    void AddMode();
    void DeleteMode(int index);
    bool ModeExists(const std::string& name) const;

    // ========== 草稿机制 ==========
    void BeginEdit(int modeIndex);
    void SwitchEditMode(int modeIndex);
    void ApplyEdit();
    void CancelEdit();
    bool IsEditing() const { return m_IsEditing; }

    const std::vector<GamepadMode>& GetModes() const;
    std::vector<GamepadMode>& GetModes();
    int& GetEditModeIndexRef() { return m_EditingModeIndex; }

    const GamepadMode& GetDisplayMode() const;
    GamepadMode& GetDisplayMode();

private:
    // ================================================================
    // 1) Xbox 画布
    // ================================================================
    std::shared_ptr<Walnut::Image> m_GamepadImage;
    std::vector<KeyInfo> m_XboxKeys;               // Xbox 物理按键定义（带画布坐标）

    void DrawXboxCanvas();
    void DrawCustomCanvas();

    // ================================================================
    // 2) 通用物理按键层（所有手柄类型统一）
    // ================================================================
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

    // 计算一个物理按键的当前强度
    float CalcXboxKeyValue(const KeyInfo& key, const GLFWgamepadstate& state);
    float CalcCustomKeyValue(const KeyInfo& key) const;

    void UpdateAllKeyValues();            // 更新 m_KeyValues（每帧调用）
    void RebuildCustomKeys();             // 重建自定义手柄物理按键
    void UpdateRawJoystickState();        // 读取原始 joystick 状态

    // ================================================================
    // 3) 自定义键位绑定层
    // ================================================================
    std::vector<GamepadMode> m_Modes;
    int m_ActiveModeIndex = 0;
    int m_SelectedModeIndex = 0;    // UI 中当前选中的模式索引

    GamepadMode m_EditingMode;                          // 当前编辑中的草稿模式
    int         m_EditingModeIndex = -1;                // 草稿对应的 m_Modes 索引
    bool        m_IsEditing = false;

    mutable std::mutex m_ModeMutex;

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

    // ================================================================
    // Xbox 画布专用的旧成员（保留兼容）
    // ================================================================
    std::vector<KeyInfo> m_Keys;                       // Xbox 物理按键定义（带画布坐标）
    std::map<std::string, float> m_RawKeyValues;       // 物理按键原始值（未处理，供 GetKeyValue/画布/绑定使用）

    GLFWgamepadstate m_LastState;
    float m_LX = 0, m_LY = 0, m_RX = 0, m_RY = 0;

    float CalcRawValue(const KeyInfo& key, const GLFWgamepadstate& state);
    float CalcActivation(const KeyInfo& key, float rawVal) const;  // 原始值 → 激活程度 [0,1]
};