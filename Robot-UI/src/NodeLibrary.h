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
    AddSubMulDiv,   // 四则运算（OpMode 切换）
    ScaleBias,      // Scale & Bias: Out = In * a + b
    Lerp,           // 线性插值: A + (B-A)*T
    Clamp,          // 钳制: clamp(Value, Min, Max)
    Remap,          // 区间映射
    Compare,        // 比较: A op B → bool
    InRange,        // 区间判断: Value ∈ [Min,Max]
    Select,         // 选择: Cond ? A : B（浮点）
    BoolSelect,     // 布尔选择: Cond ? A : B（布尔）
    Atan2,          // atan2(Y, X)
    Sin,            // sin(rad)
    Cos,            // cos(rad)

    // -- 2. Shaping（信号塑形） --
    DeadZone,       // 死区
    Curve,          // 曲线映射（分段线性）
    Quantizer,      // 量化器
    Hysteresis,     // 滞回比较

    // -- 3. Logic（逻辑与时序） --
    And,            // AND
    Or,             // OR
    Xor,            // XOR
    Not,            // NOT
    RisingEdge,     // 上升沿检测
    FallingEdge,    // 下降沿检测
    Toggle,         // 翻转锁存
    SRLatch,        // SR 锁存器
    Hold,           // 长按检测
    DelayOn,        // 接通延时
    DelayOff,       // 关断延时
    Timer,          // 可重触发单稳态
    Pulse,          // 边沿脉冲

    // -- 4. Memory（状态与记忆） --
    UnitDelay,      // 单位延迟 z⁻¹
    SampleHold,     // 采样保持
    Accumulator,    // 累加器
    Integrator,     // 积分器
    Differentiator, // 微分器
    Counter,        // 循环计数器
    RateLimiter,    // 速率限制
    LowPass,        // 低通滤波
    MovingAverage,  // 滑动平均

    // -- 5. Control（反馈与控制） --
    ErrorCalculator,    // 误差计算
    PID,                // PID 控制器
    Feedforward,        // 前馈
    DeadbandComparator, // 死区比较器

    // -- Legacy / special --
    KeySource,      // 按键源（游戏手柄输入）
    ConstValue,     // 常量值
    CustomOutput,   // 输出到执行器
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
    std::vector<std::string> ModeLabels;    // Counter: mode label list

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
                        float dt = 0.0f);

// -- Factory --
// Create a fully configured EditorNode with proper pins for the given NodeType.
// nextId is called to assign IDs (node ID first, then each pin ID).
EditorNode CreateEditorNode(NodeType type, std::function<int()> nextId);
EditorNode CreateEditorNodeByType(NodeType type, std::function<int()> nextId);
