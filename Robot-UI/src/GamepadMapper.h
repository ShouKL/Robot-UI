#pragma once
#include "core.h"
#include <GLFW/glfw3.h>
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

    // 显式拷贝/移动：mutex 不可拷贝/移动，需手动实现
    GamepadMapper(const GamepadMapper& other);
    GamepadMapper& operator=(const GamepadMapper& other);
    GamepadMapper(GamepadMapper&& other);
    GamepadMapper& operator=(GamepadMapper&& other);

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

    // ---- Canvas drawing data accessors (used by GamepadMapperManager) ----
    const std::shared_ptr<Walnut::Image>& GetGamepadImage() const { return m_GamepadImage; }
    std::shared_ptr<Walnut::Image>& GetGamepadImageRef()          { return m_GamepadImage; }
    const std::vector<KeyInfo>& GetXboxPhysicalKeys() const       { return m_Keys; }
    const std::vector<KeyInfo>& GetCustomPhysicalKeys() const     { return m_CustomPhysicalKeys; }
    float GetRawKeyValue(const std::string& name) const;
    const std::map<std::string, std::vector<std::string>>& GetKeyBoundActions() const { return m_KeyBoundActions; }

    // ---- Expose for canvas drawing ----
    void UnbindKey(const std::string&);
    float CalcActivation(const KeyInfo& key, float rawVal) const;
    void RebuildCustomKeys();
    void RefreshBoundKeys();

    // ---- UI state accessors (used by GamepadMapperManager for key-grid drawing) ----
    const std::string& GetSelectedKey() const             { return m_SelectedKey; }
    void SetSelectedKey(const std::string& k)              { m_SelectedKey = k; }
    void ClearSelectedKey()                                { m_SelectedKey.clear(); }
    int  GetPendingDeleteKeyId() const                     { return m_PendingDeleteKeyId; }
    void SetPendingDeleteKeyId(int id)                     { m_PendingDeleteKeyId = id; }
    void ProcessPendingDeletes();
    // Inline rename state
    bool IsRenaming(int keyId) const                       { return m_Renaming && m_RenamingKeyId == keyId; }
    char* GetRenameBuffer()                                { return m_RenameBuffer; }
    void BeginRename(int keyId, const std::string& keyName);
    void EndRename(bool confirmed);

private:
    const std::vector<KeyInfo>& GetActivePhysicalKeys() const;
    void UpdateRawJoystickState();
    void UpdateAllKeyValues();
    void BindKey();

    float CalcRawValue(const KeyInfo&, const GLFWgamepadstate&);

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