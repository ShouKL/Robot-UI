#pragma once

// ============================================================================
// NodeLibrary — pure data types for the node graph system.
// Contains: PinType, EditorPin, NodeCategory, NodeType, EditorNode,
// and associated free functions (rendering, compute, factories).
// No dependency on NodeGraph.
// ============================================================================

#include <imgui.h>
#include <imgui_node_editor.h>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <unordered_map>
#include <functional>

// ============================================================================
// PinType
// ============================================================================
enum class PinType
{
    Float,
    Bool,
    Int,
};

// ============================================================================
// Forward declaration
// ============================================================================
struct EditorNode;

// ============================================================================
// EditorPin — one input or output connector on a node
// ============================================================================
struct EditorPin
{
    ax::NodeEditor::PinId   ID;
    EditorNode*             Node = nullptr;
    std::string             Name;
    PinType                 Type = PinType::Float;

    EditorPin(int id, const char* name, PinType type = PinType::Float)
        : ID(id), Name(name), Type(type) {}
};

// ============================================================================
// NodeCategory — 五大节点分类
// ============================================================================
enum class NodeCategory
{
    Math,       // 数学运算
    Shaping,    // 信号塑形
    Logic,      // 逻辑与时序
    Memory,     // 状态与记忆
    Control,    // 反馈与控制
};

// ============================================================================
// NodeType
// ============================================================================
enum class NodeType
{
    // -- 1. Math（数学运算） --
    AddSubMulDiv = 0,   // 四则运算（OpMode 切换）
    ScaleBias = 1,      // Scale & Bias: Out = In * a + b
    Lerp = 2,           // 线性插值: A + (B-A)*T
    Clamp = 3,          // 钳制: clamp(Value, Min, Max)
    Remap = 4,          // 区间映射
    Compare = 5,        // 比较: A op B → bool
    InRange = 6,        // 区间判断: Value ∈ [Min,Max]
    Select = 7,         // 选择: Cond ? A : B
    BoolSelect = 8,     // [已废弃] 被 Select 取代
    MathFunc = 9,       // 数学函数: 0=Sin,1=Cos,2=Atan2 (复用旧Atan2=9)

    // slots 10,11 were Sin,Cos — see LoadGraphData compat

    // -- 2. Shaping（信号塑形） --
    DeadZone = 12,
    Curve = 13,
    Quantizer = 14,
    Hysteresis = 15,

    // -- 3. Logic（逻辑与时序） --
    LogicOp = 16,       // 逻辑运算: 0=AND,1=OR,2=XOR,3=NOT (复用旧And=16)

    // slots 17,18,19 were Or,Xor,Not — see LoadGraphData compat

    RisingEdge = 20,    // 边缘检测: 0=上升沿, 1=下降沿

    // slot 21 was FallingEdge — see LoadGraphData compat

    Toggle = 22,
    SRLatch = 23,

    // slot 24 was Hold — see LoadGraphData compat

    DelayOn = 25,       // 延时（Hold 已合并至此）
    DelayOff = 26,
    Timer = 27,
    Pulse = 28,

    // -- 4. Memory（状态与记忆） --
    UnitDelay = 29,
    SampleHold = 30,
    Accumulator = 31,
    Integrator = 32,
    Differentiator = 33,
    Counter = 34,
    RateLimiter = 35,
    LowPass = 36,
    MovingAverage = 37,
    GlobalRead = 38,
    GlobalWrite = 39,

    // slot 40 was ErrorCalculator — see LoadGraphData compat

    // -- 5. Control（反馈与控制） --
    PID = 41,

    // slot 42 was Feedforward — see LoadGraphData compat

    DeadbandComparator = 43,

    // -- Legacy / special --
    KeySource = 44,
    ConstValue = 45,
    CustomOutput = 46,
    LookupTable = 47,  // Mode → Value mapping
    ShortcutTrigger = 48,  // Keyboard shortcut → float (1.0 when held)
};

// ============================================================================
// EditorNode — one node in the graph
// ============================================================================
struct EditorNode
{
    ax::NodeEditor::NodeId  ID;
    std::string             Name;
    NodeType                Type     = NodeType::ConstValue;
    NodeCategory            Category = NodeCategory::Math;
    std::vector<EditorPin>  Inputs;
    std::vector<EditorPin>  Outputs;
    ImColor                 Color = ImColor(255, 255, 255);

    // ---- Display values (updated each frame by evaluation) ----
    float Value          = 0.0f;  // Primary output value
    float InputValues[4] = {};    // Display: input pin values {in0, in1, in2, in3}
    bool  InputBools[4]  = {};    // Display: boolean interpretation of inputs

    // ---- Configurable parameters ----
    float Param[8] = {};          // Generic float params (scale, bias, min, max, kp, ki, kd, ...)
    int   OpMode   = 0;           // Operation mode selector

    // ---- Identifier ----
    std::string              KeyName;       // KeySource: bound gamepad key
    std::string              OutputTarget;  // CustomOutput: actuator field path
    int                      CommIndex   = 0;  // CustomOutput: target RobotComm index (0=default)
    int                      GlobalVarId = -1; // GlobalRead/Write: stable variable ID
    std::vector<std::string> ModeLabels;    // Counter: mode label list

    // ---- ShortcutTrigger: action selection ----
    int  ShortcutActionIndex = -1;  // -1=none; 0..N = ShortcutManager::Action (panel toggle)
    int  ShortcutSendIndex   = -1;  // -1=none; 0..N = flat send frame index (from RobotComm)
    int  ShortcutSendMode    = 0;   // 0=Toggle, 1=One-Shot

    // ---- Internal runtime state (for memory/logic nodes) ----
    float StateF[4]   = {};      // Float state
    bool  StateB[4]   = {};      // Bool state
    float StateDt     = 0.0f;    // Accumulated delta time

    EditorNode(int id, const char* name, NodeType type, ImColor color = ImColor(255, 255, 255))
        : ID(id), Name(name), Type(type), Color(color) {}
};

// ============================================================================
// Free function declarations
// ============================================================================

// -- Category helpers --
NodeCategory GetNodeCategory(NodeType type);
const char*  GetCategoryName(NodeCategory cat);

// -- Visual helpers --
const char* GetNodeTitle(NodeType type);
ImColor     GetNodeHeaderColor(NodeType type);
ImColor     GetCategoryColor(NodeCategory cat);
ImColor     GetIconColor(PinType type);
void        DrawPinIcon(const EditorPin& pin, bool connected, int alpha);
void        DrawPinTypeSelector(EditorPin* pin, std::function<void()> onModified);

// -- Pin type compatibility (Float ↔ Bool ↔ Int are interchangeable) --
inline bool PinTypesCompatible(PinType a, PinType b) {
    if (a == b) return true;
    // All pin types are internally float, cross-type connections allowed
    return true;
}

// -- Node type iteration (for building menus dynamically) --
extern const NodeType AllNodeTypes[];
extern const int      AllNodeTypeCount;
// Get count of types belonging to a category (for menu sub-headers)
int  GetCategoryNodeCount(NodeCategory cat);
// Get the i-th node type in a category (0-indexed)
NodeType GetCategoryNodeType(NodeCategory cat, int index);

// -- OpMode support (for nodes like AddSubMulDiv, Compare, etc.) --
int         GetNodeOpModeCount(NodeType type);
const char* GetNodeOpModeLabel(NodeType type, int mode);

// -- Generic node body drawing (called from NodeGraph::DrawNodeContents) --
// Draws pins, OpMode combo, and Param sliders for the given node.
// isPinLinked callback: bool(int pinId). onModified callback: called when params change.
void DrawGenericNodeBody(EditorNode& node,
                         const std::set<std::string>& analogKeys,
                         const std::vector<std::string>& availableKeys,
                         const std::vector<std::string>& outputNames,
                         const std::vector<std::string>& outputPaths,
                         std::function<bool(int)> isPinLinked,
                         std::function<void()> onModified);

// -- Compute --
// Evaluate a single node's output. Modifies node.State* for stateful nodes.
// dt is the frame delta time in seconds (0 if not applicable).
float ComputeNodeOutput(EditorNode& node,
                        const std::map<std::string, float>& keyValues,
                        const std::unordered_map<int, float>& pinVals,
                        float dt = 0.0f,
                        float* globals = nullptr,
                        int globalsCount = 0);

// -- Factory --
// Create a fully configured EditorNode with proper pins for the given NodeType.
// nextId is called to assign IDs (node ID first, then each pin ID).
EditorNode CreateEditorNode(NodeType type, std::function<int()> nextId);
EditorNode CreateEditorNodeByType(NodeType type, std::function<int()> nextId);
