#include "NodeLibrary.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace ed = ax::NodeEditor;

// ============================================================================
// GetIconColor
// ============================================================================
ImColor GetIconColor(PinType type)
{
    switch (type)
    {
    default:
    case PinType::Float: return ImColor(147, 226,  74);
    }
}

// ============================================================================
// GetCategoryName / GetNodeCategory / GetCategoryColor
// ============================================================================
const char* GetCategoryName(NodeCategory cat)
{
    switch (cat)
    {
    case NodeCategory::Math:    return "Math";
    case NodeCategory::Shaping: return "Shaping";
    case NodeCategory::Logic:   return "Logic";
    case NodeCategory::Memory:  return "Memory";
    case NodeCategory::Control: return "Control";
    default:                    return "???";
    }
}

NodeCategory GetNodeCategory(NodeType type)
{
    switch (type)
    {
    // Math
    case NodeType::AddSubMulDiv:
    case NodeType::ScaleBias:
    case NodeType::Lerp:
    case NodeType::Clamp:
    case NodeType::Remap:
    case NodeType::Compare:
    case NodeType::InRange:
    case NodeType::Select:
    case NodeType::BoolSelect:
    case NodeType::Atan2:
    case NodeType::Sin:
    case NodeType::Cos:
        return NodeCategory::Math;

    // Shaping
    case NodeType::DeadZone:
    case NodeType::Curve:
    case NodeType::Quantizer:
    case NodeType::Hysteresis:
        return NodeCategory::Shaping;

    // Logic
    case NodeType::And:
    case NodeType::Or:
    case NodeType::Xor:
    case NodeType::Not:
    case NodeType::RisingEdge:
    case NodeType::FallingEdge:
    case NodeType::Toggle:
    case NodeType::SRLatch:
    case NodeType::Hold:
    case NodeType::DelayOn:
    case NodeType::DelayOff:
    case NodeType::Timer:
    case NodeType::Pulse:
        return NodeCategory::Logic;

    // Memory
    case NodeType::UnitDelay:
    case NodeType::SampleHold:
    case NodeType::Accumulator:
    case NodeType::Integrator:
    case NodeType::Differentiator:
    case NodeType::Counter:
    case NodeType::RateLimiter:
    case NodeType::LowPass:
    case NodeType::MovingAverage:
        return NodeCategory::Memory;

    // Control
    case NodeType::ErrorCalculator:
    case NodeType::PID:
    case NodeType::Feedforward:
    case NodeType::DeadbandComparator:
        return NodeCategory::Control;

    // Legacy / special
    case NodeType::KeySource:    return NodeCategory::Math;
    case NodeType::ConstValue:   return NodeCategory::Math;
    case NodeType::CustomOutput: return NodeCategory::Control;
    }
    return NodeCategory::Math;
}

ImColor GetCategoryColor(NodeCategory cat)
{
    switch (cat)
    {
    case NodeCategory::Math:    return ImColor(180, 120,  66);
    case NodeCategory::Shaping: return ImColor(120,  66, 180);
    case NodeCategory::Logic:   return ImColor(100, 140, 200);
    case NodeCategory::Memory:  return ImColor( 66, 150,  66);
    case NodeCategory::Control: return ImColor( 66, 180, 120);
    default:                    return ImColor(128, 128, 128);
    }
}

// ============================================================================
// GetNodeHeaderColor — per-type color, falls back to category color
// ============================================================================
ImColor GetNodeHeaderColor(NodeType type)
{
    auto cat = GetCategoryColor(GetNodeCategory(type));
    switch (type)
    {
    default: return cat;
    }
}

// ============================================================================
// GetNodeTitle
// ============================================================================
const char* GetNodeTitle(NodeType type)
{
    switch (type)
    {
    // Math
    case NodeType::AddSubMulDiv:  return "Arith";
    case NodeType::ScaleBias:     return "Scale&Bias";
    case NodeType::Lerp:          return "Lerp";
    case NodeType::Clamp:         return "Clamp";
    case NodeType::Remap:         return "Remap";
    case NodeType::Compare:       return "Compare";
    case NodeType::InRange:       return "In Range";
    case NodeType::Select:        return "Select";
    case NodeType::BoolSelect:    return "Bool Select";
    case NodeType::Atan2:         return "Atan2";
    case NodeType::Sin:           return "Sin";
    case NodeType::Cos:           return "Cos";

    // Shaping
    case NodeType::DeadZone:      return "Dead Zone";
    case NodeType::Curve:         return "Curve";
    case NodeType::Quantizer:     return "Quantizer";
    case NodeType::Hysteresis:    return "Hysteresis";

    // Logic
    case NodeType::And:           return "AND";
    case NodeType::Or:            return "OR";
    case NodeType::Xor:           return "XOR";
    case NodeType::Not:           return "NOT";
    case NodeType::RisingEdge:    return "Rising Edge";
    case NodeType::FallingEdge:   return "Falling Edge";
    case NodeType::Toggle:        return "Toggle";
    case NodeType::SRLatch:       return "SR Latch";
    case NodeType::Hold:          return "Hold";
    case NodeType::DelayOn:       return "Delay On";
    case NodeType::DelayOff:      return "Delay Off";
    case NodeType::Timer:         return "Timer";
    case NodeType::Pulse:         return "Pulse";

    // Memory
    case NodeType::UnitDelay:     return "Unit Delay";
    case NodeType::SampleHold:    return "S & H";
    case NodeType::Accumulator:   return "Accum";
    case NodeType::Integrator:    return "Integrator";
    case NodeType::Differentiator:return "Derivative";
    case NodeType::Counter:       return "Counter";
    case NodeType::RateLimiter:   return "Rate Limiter";
    case NodeType::LowPass:       return "Low Pass";
    case NodeType::MovingAverage: return "Mov Avg";

    // Control
    case NodeType::ErrorCalculator:    return "Error";
    case NodeType::PID:                return "PID";
    case NodeType::Feedforward:        return "Feedfwd";
    case NodeType::DeadbandComparator: return "Deadband";

    // Legacy / special
    case NodeType::KeySource:    return "Key Source";
    case NodeType::ConstValue:   return "Const";
    case NodeType::CustomOutput: return "Output";

    default: return "???";
    }
}

// ============================================================================
// DrawPinIcon
// ============================================================================
void DrawPinIcon(const EditorPin& pin, bool connected, int alpha)
{
    ImColor color = GetIconColor(pin.Type);
    color.Value.w = alpha / 255.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float iconSize = 24.0f;
    float half = iconSize * 0.5f;
    ImVec2 center(pos.x + half, pos.y + half);

    ImU32 col = ImColor(color);
    ImU32 bg  = ImColor(32, 32, 32, alpha);

    if (connected)
    {
        drawList->AddCircleFilled(center, half * 0.5f, col, 12);
        drawList->AddCircle(center, half * 0.5f, col, 12, 2.0f);
    }
    else
    {
        drawList->AddCircleFilled(center, half * 0.5f, bg, 12);
        drawList->AddCircle(center, half * 0.5f, col, 12, 1.5f);
    }

    ImGui::Dummy(ImVec2(iconSize, iconSize));
}

// ============================================================================
// ComputeNodeOutput — evaluate a single node (now non-const, modifies state)
// ============================================================================
static float GetPinVal(const std::unordered_map<int, float>& pv, const EditorPin& pin) {
    auto it = pv.find((int)pin.ID.Get());
    return (it != pv.end()) ? it->second : 0.0f;
}
static float GetPinByIndex(const EditorNode& node, const std::unordered_map<int, float>& pv, int idx) {
    if (idx < (int)node.Inputs.size()) return GetPinVal(pv, node.Inputs[idx]);
    return 0.0f;
}
static bool AsBool(float v) { return v >= 0.5f; }
static float FromBool(bool b) { return b ? 1.0f : 0.0f; }

float ComputeNodeOutput(EditorNode& node,
    const std::map<std::string, float>& keyValues,
    const std::unordered_map<int, float>& pinVals,
    float dt)
{
    switch (node.Type)
    {
    // ==================== MATH ====================
    case NodeType::AddSubMulDiv:
    {
        float a = GetPinByIndex(node, pinVals, 0);
        float b = GetPinByIndex(node, pinVals, 1);
        // OpMode: 0=Add, 1=Sub, 2=Mul, 3=Div
        switch (node.OpMode) {
        case 0: return a + b;
        case 1: return a - b;
        case 2: return a * b;
        case 3: return (b != 0.0f) ? a / b : 0.0f;
        default: return a + b;
        }
    }
    case NodeType::ScaleBias:
    {
        float in = GetPinByIndex(node, pinVals, 0);
        // Param[0]=scale, Param[1]=bias (default: 1, 0)
        float s = node.Param[0] != 0.0f ? node.Param[0] : 1.0f;
        float o = node.Param[1];
        return in * s + o;
    }
    case NodeType::Lerp:
    {
        float a = GetPinByIndex(node, pinVals, 0);
        float b = GetPinByIndex(node, pinVals, 1);
        float t = GetPinByIndex(node, pinVals, 2);
        t = std::clamp(t, 0.0f, 1.0f);
        return a + (b - a) * t;
    }
    case NodeType::Clamp:
    {
        float v = GetPinByIndex(node, pinVals, 0);
        float mn = GetPinByIndex(node, pinVals, 1);
        float mx = GetPinByIndex(node, pinVals, 2);
        if (mn > mx) std::swap(mn, mx);
        return std::clamp(v, mn, mx);
    }
    case NodeType::Remap:
    {
        float v  = GetPinByIndex(node, pinVals, 0);
        float imn = GetPinByIndex(node, pinVals, 1);
        float imx = GetPinByIndex(node, pinVals, 2);
        float omn = GetPinByIndex(node, pinVals, 3);
        float omx = GetPinByIndex(node, pinVals, 4);
        if (imx == imn) return omn;
        float t = (v - imn) / (imx - imn);
        return omn + t * (omx - omn);
    }
    case NodeType::Compare:
    {
        float a = GetPinByIndex(node, pinVals, 0);
        float b = GetPinByIndex(node, pinVals, 1);
        switch (node.OpMode) {
        case 0: return FromBool(a > b);
        case 1: return FromBool(a >= b);
        case 2: return FromBool(a <= b);
        case 3: return FromBool(a < b);
        case 4: return FromBool(a == b);
        case 5: return FromBool(a != b);
        default: return FromBool(a > b);
        }
    }
    case NodeType::InRange:
    {
        float v  = GetPinByIndex(node, pinVals, 0);
        float mn = GetPinByIndex(node, pinVals, 1);
        float mx = GetPinByIndex(node, pinVals, 2);
        return FromBool(v >= mn && v <= mx);
    }
    case NodeType::Select:
    {
        float cond = GetPinByIndex(node, pinVals, 0);
        float a    = GetPinByIndex(node, pinVals, 1);
        float b    = GetPinByIndex(node, pinVals, 2);
        return AsBool(cond) ? a : b;
    }
    case NodeType::BoolSelect:
    {
        bool cond = AsBool(GetPinByIndex(node, pinVals, 0));
        bool a    = AsBool(GetPinByIndex(node, pinVals, 1));
        bool b    = AsBool(GetPinByIndex(node, pinVals, 2));
        return FromBool(cond ? a : b);
    }
    case NodeType::Atan2:
    {
        float y = GetPinByIndex(node, pinVals, 0);
        float x = GetPinByIndex(node, pinVals, 1);
        return std::atan2(y, x);
    }
    case NodeType::Sin:
    {
        float rad = GetPinByIndex(node, pinVals, 0);
        return std::sin(rad);
    }
    case NodeType::Cos:
    {
        float rad = GetPinByIndex(node, pinVals, 0);
        return std::cos(rad);
    }

    // ==================== SHAPING ====================
    case NodeType::DeadZone:
    {
        float in = GetPinByIndex(node, pinVals, 0);
        float dz = node.Param[0]; // dead zone half-width (default 0.1)
        if (dz <= 0.0f) dz = 0.1f;
        if (std::abs(in) < dz) return 0.0f;
        float sign = (in > 0) ? 1.0f : -1.0f;
        return sign * (std::abs(in) - dz) / (1.0f - dz);
    }
    case NodeType::Curve:
    {
        float in = GetPinByIndex(node, pinVals, 0);
        // Param[0..7] = 4 control point pairs (x,y) for piecewise-linear
        // Simple: treat as raw passthrough for now; curve data stored in Param
        return in; // TODO: implement piecewise-linear evaluation from Param
    }
    case NodeType::Quantizer:
    {
        float in = GetPinByIndex(node, pinVals, 0);
        float step = node.Param[0] != 0.0f ? node.Param[0] : 1.0f;
        return std::round(in / step) * step;
    }
    case NodeType::Hysteresis:
    {
        float in  = GetPinByIndex(node, pinVals, 0);
        float thr = node.Param[0] != 0.0f ? node.Param[0] : 0.5f;
        float hys = node.Param[1] != 0.0f ? node.Param[1] : 0.1f;
        // StateB[0] = previous output
        if (in > thr + hys)       node.StateB[0] = true;
        else if (in < thr - hys)  node.StateB[0] = false;
        return FromBool(node.StateB[0]);
    }

    // ==================== LOGIC ====================
    case NodeType::And:
    {
        bool a = AsBool(GetPinByIndex(node, pinVals, 0));
        bool b = AsBool(GetPinByIndex(node, pinVals, 1));
        return FromBool(a && b);
    }
    case NodeType::Or:
    {
        bool a = AsBool(GetPinByIndex(node, pinVals, 0));
        bool b = AsBool(GetPinByIndex(node, pinVals, 1));
        return FromBool(a || b);
    }
    case NodeType::Xor:
    {
        bool a = AsBool(GetPinByIndex(node, pinVals, 0));
        bool b = AsBool(GetPinByIndex(node, pinVals, 1));
        return FromBool(a != b);
    }
    case NodeType::Not:
    {
        bool a = AsBool(GetPinByIndex(node, pinVals, 0));
        return FromBool(!a);
    }
    case NodeType::RisingEdge:
    {
        bool cur = AsBool(GetPinByIndex(node, pinVals, 0));
        bool prev = node.StateB[0];
        node.StateB[0] = cur;
        return FromBool(cur && !prev);
    }
    case NodeType::FallingEdge:
    {
        bool cur  = AsBool(GetPinByIndex(node, pinVals, 0));
        bool prev = node.StateB[0];
        node.StateB[0] = cur;
        return FromBool(!cur && prev);
    }
    case NodeType::Toggle:
    {
        bool cur  = AsBool(GetPinByIndex(node, pinVals, 0));
        bool prev = node.StateB[1]; // previous clock
        node.StateB[1] = cur;
        if (cur && !prev) node.StateB[0] = !node.StateB[0]; // toggle on rising edge
        return FromBool(node.StateB[0]);
    }
    case NodeType::SRLatch:
    {
        bool s = AsBool(GetPinByIndex(node, pinVals, 0));
        bool r = AsBool(GetPinByIndex(node, pinVals, 1));
        if (s)       node.StateB[0] = true;
        else if (r)  node.StateB[0] = false;
        return FromBool(node.StateB[0]);
    }
    case NodeType::Hold:
    {
        bool cur = AsBool(GetPinByIndex(node, pinVals, 0));
        float holdTime = node.Param[0] != 0.0f ? node.Param[0] : 0.5f;
        if (cur) node.StateDt += dt;
        else     node.StateDt = 0.0f;
        return FromBool(cur && node.StateDt >= holdTime);
    }
    case NodeType::DelayOn:
    {
        bool cur = AsBool(GetPinByIndex(node, pinVals, 0));
        float delay = node.Param[0] != 0.0f ? node.Param[0] : 0.5f;
        if (cur) node.StateDt += dt;
        else     node.StateDt = 0.0f;
        return FromBool(cur && node.StateDt >= delay);
    }
    case NodeType::DelayOff:
    {
        bool cur = AsBool(GetPinByIndex(node, pinVals, 0));
        float delay = node.Param[0] != 0.0f ? node.Param[0] : 0.5f;
        if (!cur) node.StateDt += dt;
        else      node.StateDt = 0.0f;
        if (cur) return 1.0f;
        return FromBool(node.StateDt < delay);
    }
    case NodeType::Timer:
    {
        bool trig = AsBool(GetPinByIndex(node, pinVals, 0));
        // StateB[1] = previous trigger, StateB[0] = output active
        if (trig && !node.StateB[1]) {
            node.StateB[0] = true;
            node.StateDt = 0.0f;
        }
        node.StateB[1] = trig;
        if (node.StateB[0]) {
            float dur = node.Param[0] != 0.0f ? node.Param[0] : 0.5f;
            node.StateDt += dt;
            if (node.StateDt >= dur) node.StateB[0] = false;
        }
        return FromBool(node.StateB[0]);
    }
    case NodeType::Pulse:
    {
        bool cur  = AsBool(GetPinByIndex(node, pinVals, 0));
        bool prev = node.StateB[1];
        node.StateB[1] = cur;
        if (cur != prev) {
            node.StateB[0] = true;
            node.StateDt = 0.0f;
        }
        if (node.StateB[0]) {
            float width = node.Param[0] != 0.0f ? node.Param[0] : 0.1f;
            node.StateDt += dt;
            if (node.StateDt >= width) node.StateB[0] = false;
        }
        return FromBool(node.StateB[0]);
    }

    // ==================== MEMORY ====================
    case NodeType::UnitDelay:
    {
        float in = GetPinByIndex(node, pinVals, 0);
        float out = node.StateF[0];
        node.StateF[0] = in;
        return out;
    }
    case NodeType::SampleHold:
    {
        float in  = GetPinByIndex(node, pinVals, 0);
        bool trig = AsBool(GetPinByIndex(node, pinVals, 1));
        if (trig) node.StateF[0] = in;
        return node.StateF[0];
    }
    case NodeType::Accumulator:
    {
        float in  = GetPinByIndex(node, pinVals, 0);
        bool reset = AsBool(GetPinByIndex(node, pinVals, 1));
        if (reset) node.StateF[0] = 0.0f;
        else       node.StateF[0] += in;
        return node.StateF[0];
    }
    case NodeType::Integrator:
    {
        float in  = GetPinByIndex(node, pinVals, 0);
        bool reset = AsBool(GetPinByIndex(node, pinVals, 1));
        if (reset) node.StateF[0] = 0.0f;
        else       node.StateF[0] += in * dt;
        float lo = node.Param[0], hi = node.Param[1];
        if (lo != 0.0f || hi != 0.0f) {
            if (hi > lo) node.StateF[0] = std::clamp(node.StateF[0], lo, hi);
        }
        return node.StateF[0];
    }
    case NodeType::Differentiator:
    {
        float in = GetPinByIndex(node, pinVals, 0);
        float prev = node.StateF[1]; // previous input
        node.StateF[1] = in;
        if (dt > 0.0f) return (in - prev) / dt;
        return 0.0f;
    }
    case NodeType::Counter:
    {
        bool inc   = AsBool(GetPinByIndex(node, pinVals, 0));
        bool reset = AsBool(GetPinByIndex(node, pinVals, 1));
        // StateB[1] = previous increment
        if (reset) { node.StateF[0] = 0; }
        else if (inc && !node.StateB[1]) {
            int limit = (int)node.ModeLabels.size();
            if (limit <= 0) limit = 10;
            node.StateF[0] = std::fmod(node.StateF[0] + 1.0f, (float)limit);
        }
        node.StateB[1] = inc;
        return node.StateF[0];
    }
    case NodeType::RateLimiter:
    {
        float in = GetPinByIndex(node, pinVals, 0);
        float upRate   = node.Param[0] != 0.0f ? node.Param[0] : 10.0f;
        float downRate = node.Param[1] != 0.0f ? node.Param[1] : 10.0f;
        float prev = node.StateF[0];
        float maxUp   = upRate * dt;
        float maxDown = downRate * dt;
        float delta = in - prev;
        if (delta > maxUp)        delta = maxUp;
        else if (delta < -maxDown) delta = -maxDown;
        node.StateF[0] = prev + delta;
        return node.StateF[0];
    }
    case NodeType::LowPass:
    {
        float in = GetPinByIndex(node, pinVals, 0);
        float cutoff = node.Param[0] != 0.0f ? node.Param[0] : 10.0f;
        float rc = 1.0f / (cutoff * 2.0f * 3.14159265f);
        float alpha = dt / (rc + dt);
        node.StateF[0] = node.StateF[0] + alpha * (in - node.StateF[0]);
        return node.StateF[0];
    }
    case NodeType::MovingAverage:
    {
        float in = GetPinByIndex(node, pinVals, 0);
        int window = (int)node.Param[0];
        if (window <= 0) window = 5;
        // Shift buffer: StateF[0..3] = 4-slot, extend via StateDt
        // Simple: use StateF[0] as sum, StateF[1] as count
        node.StateF[0] += in;
        node.StateF[1] += 1.0f;
        if (node.StateF[1] > (float)window) {
            // Need ring buffer; simplified: exponential moving average fallback
            float alpha = 2.0f / ((float)window + 1.0f);
            node.StateF[2] = node.StateF[2] + alpha * (in - node.StateF[2]);
            return node.StateF[2];
        }
        return node.StateF[0] / node.StateF[1];
    }

    // ==================== CONTROL ====================
    case NodeType::ErrorCalculator:
    {
        float sp = GetPinByIndex(node, pinVals, 0);
        float actual = GetPinByIndex(node, pinVals, 1);
        return sp - actual;
    }
    case NodeType::PID:
    {
        float sp = GetPinByIndex(node, pinVals, 0);
        float pv = GetPinByIndex(node, pinVals, 1);
        float kp = node.Param[0] != 0.0f ? node.Param[0] : 1.0f;
        float ki = node.Param[1];
        float kd = node.Param[2];
        float error = sp - pv;
        // Proportional
        float P = kp * error;
        // Integral (StateF[0])
        node.StateF[0] += error * dt;
        float I = ki * node.StateF[0];
        // Derivative (StateF[1] = prev error)
        float D = (dt > 0.0f) ? kd * (error - node.StateF[1]) / dt : 0.0f;
        node.StateF[1] = error;
        // Reset via StateB[0] (external reset pin)
        if (AsBool(GetPinByIndex(node, pinVals, 2))) {
            node.StateF[0] = 0.0f;
            node.StateF[1] = 0.0f;
        }
        // Output limit
        float out = P + I + D;
        if (node.Param[3] != 0.0f || node.Param[4] != 0.0f) {
            float lo = node.Param[3], hi = node.Param[4];
            if (hi > lo) out = std::clamp(out, lo, hi);
        }
        return out;
    }
    case NodeType::Feedforward:
    {
        float sp = GetPinByIndex(node, pinVals, 0);
        float gain = node.Param[0] != 0.0f ? node.Param[0] : 1.0f;
        return sp * gain;
    }
    case NodeType::DeadbandComparator:
    {
        float err = GetPinByIndex(node, pinVals, 0);
        float band = node.Param[0] != 0.0f ? node.Param[0] : 0.01f;
        return FromBool(std::abs(err) <= band);
    }

    // ==================== LEGACY/SPECIAL ====================
    case NodeType::KeySource:
        return (keyValues.count(node.KeyName) > 0) ? keyValues.at(node.KeyName) : 0.0f;
    case NodeType::ConstValue:
        return node.Value;
    case NodeType::CustomOutput:
        return GetPinByIndex(node, pinVals, 0);
    }
    return 0.0f;
}

// ============================================================================
// CreateEditorNode — factory: produce a fully configured EditorNode
// ============================================================================
EditorNode CreateEditorNode(NodeType type, std::function<int()> nextId)
{
    return CreateEditorNodeByType(type, std::move(nextId));
}

EditorNode CreateEditorNodeByType(NodeType type, std::function<int()> nextId)
{
    auto N = [&](const char* name) { return EditorNode(nextId(), name, type, GetNodeHeaderColor(type)); };
    auto I = [&](const char* label) { return EditorPin(nextId(), label, PinType::Float); };
    auto O = [&](const char* label) { return EditorPin(nextId(), label, PinType::Float); };

    switch (type)
    {
    // ==================== MATH ====================
    case NodeType::AddSubMulDiv: {
        auto n = N("Arith"); n.Inputs = {I("A"), I("B")}; n.Outputs = {O("Out")}; return n; }
    case NodeType::ScaleBias: {
        auto n = N("Scale&Bias"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 1.0f; return n; }
    case NodeType::Lerp: {
        auto n = N("Lerp"); n.Inputs = {I("A"), I("B"), I("T")}; n.Outputs = {O("Out")}; return n; }
    case NodeType::Clamp: {
        auto n = N("Clamp"); n.Inputs = {I("Value"), I("Min"), I("Max")}; n.Outputs = {O("Out")};
        n.Param[2] = 1.0f; return n; }
    case NodeType::Remap: {
        auto n = N("Remap"); n.Inputs = {I("Value"), I("InMin"), I("InMax"), I("OutMin"), I("OutMax")};
        n.Outputs = {O("Out")}; n.Param[5] = 1.0f; return n; }
    case NodeType::Compare: {
        auto n = N("Compare"); n.Inputs = {I("A"), I("B")}; n.Outputs = {O("Bool")}; return n; }
    case NodeType::InRange: {
        auto n = N("In Range"); n.Inputs = {I("Value"), I("Min"), I("Max")}; n.Outputs = {O("Bool")};
        n.Param[2] = 1.0f; return n; }
    case NodeType::Select: {
        auto n = N("Select"); n.Inputs = {I("Cond"), I("A"), I("B")}; n.Outputs = {O("Out")}; return n; }
    case NodeType::BoolSelect: {
        auto n = N("Bool Select"); n.Inputs = {I("Cond"), I("A"), I("B")}; n.Outputs = {O("Out")}; return n; }
    case NodeType::Atan2: {
        auto n = N("Atan2"); n.Inputs = {I("Y"), I("X")}; n.Outputs = {O("θ")}; return n; }
    case NodeType::Sin: {
        auto n = N("Sin"); n.Inputs = {I("rad")}; n.Outputs = {O("Out")}; return n; }
    case NodeType::Cos: {
        auto n = N("Cos"); n.Inputs = {I("rad")}; n.Outputs = {O("Out")}; return n; }

    // ==================== SHAPING ====================
    case NodeType::DeadZone: {
        auto n = N("Dead Zone"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 0.1f; return n; }
    case NodeType::Curve: {
        auto n = N("Curve"); n.Inputs = {I("In")}; n.Outputs = {O("Out")}; return n; }
    case NodeType::Quantizer: {
        auto n = N("Quantizer"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 1.0f; return n; }
    case NodeType::Hysteresis: {
        auto n = N("Hysteresis"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 0.5f; n.Param[1] = 0.1f; return n; }

    // ==================== LOGIC ====================
    case NodeType::And: {
        auto n = N("AND"); n.Inputs = {I("A"), I("B")}; n.Outputs = {O("A&B")}; return n; }
    case NodeType::Or: {
        auto n = N("OR"); n.Inputs = {I("A"), I("B")}; n.Outputs = {O("A|B")}; return n; }
    case NodeType::Xor: {
        auto n = N("XOR"); n.Inputs = {I("A"), I("B")}; n.Outputs = {O("A^B")}; return n; }
    case NodeType::Not: {
        auto n = N("NOT"); n.Inputs = {I("A")}; n.Outputs = {O("!A")}; return n; }
    case NodeType::RisingEdge: {
        auto n = N("Rising Edge"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.StateB[0] = false; return n; }
    case NodeType::FallingEdge: {
        auto n = N("Falling Edge"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.StateB[0] = false; return n; }
    case NodeType::Toggle: {
        auto n = N("Toggle"); n.Inputs = {I("Clk")}; n.Outputs = {O("Q")};
        n.StateB[0] = false; return n; }
    case NodeType::SRLatch: {
        auto n = N("SR Latch"); n.Inputs = {I("S"), I("R")}; n.Outputs = {O("Q")};
        n.StateB[0] = false; return n; }
    case NodeType::Hold: {
        auto n = N("Hold"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 0.5f; return n; }
    case NodeType::DelayOn: {
        auto n = N("Delay On"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 0.5f; return n; }
    case NodeType::DelayOff: {
        auto n = N("Delay Off"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 0.5f; return n; }
    case NodeType::Timer: {
        auto n = N("Timer"); n.Inputs = {I("Trig")}; n.Outputs = {O("Out")};
        n.Param[0] = 0.5f; n.StateB[0] = false; return n; }
    case NodeType::Pulse: {
        auto n = N("Pulse"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 0.1f; n.StateB[0] = false; return n; }

    // ==================== MEMORY ====================
    case NodeType::UnitDelay: {
        auto n = N("Unit Delay"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.StateF[0] = 0.0f; return n; }
    case NodeType::SampleHold: {
        auto n = N("S & H"); n.Inputs = {I("Value"), I("Trig")}; n.Outputs = {O("Out")};
        n.StateF[0] = 0.0f; return n; }
    case NodeType::Accumulator: {
        auto n = N("Accum"); n.Inputs = {I("In"), I("Reset")}; n.Outputs = {O("Sum")};
        n.StateF[0] = 0.0f; return n; }
    case NodeType::Integrator: {
        auto n = N("Integrator"); n.Inputs = {I("In"), I("Reset")}; n.Outputs = {O("Out")};
        n.StateF[0] = 0.0f; return n; }
    case NodeType::Differentiator: {
        auto n = N("Derivative"); n.Inputs = {I("In")}; n.Outputs = {O("d/dt")};
        n.StateF[1] = 0.0f; return n; }
    case NodeType::Counter: {
        auto n = N("Counter"); n.Inputs = {I("Inc"), I("Reset")}; n.Outputs = {O("Count")};
        n.StateF[0] = 0; return n; }
    case NodeType::RateLimiter: {
        auto n = N("Rate Limiter"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 10.0f; n.Param[1] = 10.0f; n.StateF[0] = 0.0f; return n; }
    case NodeType::LowPass: {
        auto n = N("Low Pass"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 10.0f; n.StateF[0] = 0.0f; return n; }
    case NodeType::MovingAverage: {
        auto n = N("Mov Avg"); n.Inputs = {I("In")}; n.Outputs = {O("Avg")};
        n.Param[0] = 5.0f; return n; }

    // ==================== CONTROL ====================
    case NodeType::ErrorCalculator: {
        auto n = N("Error"); n.Inputs = {I("Setpoint"), I("Actual")}; n.Outputs = {O("Error")}; return n; }
    case NodeType::PID: {
        auto n = N("PID"); n.Inputs = {I("Setpoint"), I("PV"), I("Reset")}; n.Outputs = {O("Out")};
        n.Param[0] = 1.0f; n.StateF[0] = 0.0f; n.StateF[1] = 0.0f; return n; }
    case NodeType::Feedforward: {
        auto n = N("Feedfwd"); n.Inputs = {I("Setpoint")}; n.Outputs = {O("Out")};
        n.Param[0] = 1.0f; return n; }
    case NodeType::DeadbandComparator: {
        auto n = N("Deadband"); n.Inputs = {I("Error")}; n.Outputs = {O("OnTarget")};
        n.Param[0] = 0.01f; return n; }

    // ==================== LEGACY/SPECIAL ====================
    case NodeType::KeySource: {
        auto n = N("Key Source"); n.Outputs = {O("Value")}; return n; }
    case NodeType::ConstValue: {
        auto n = N("Const"); n.Outputs = {O("Value")}; n.Value = 0.0f; return n; }
    case NodeType::CustomOutput: {
        auto n = N("Output"); n.Inputs = {I("Value")}; return n; }
    }
    return EditorNode(0, "???", type);
}

// ============================================================================
// AllNodeTypes — for dynamic menu/context-menu iteration
// ============================================================================
const NodeType AllNodeTypes[] = {
    NodeType::AddSubMulDiv, NodeType::ScaleBias, NodeType::Lerp,
    NodeType::Clamp, NodeType::Remap, NodeType::Compare, NodeType::InRange,
    NodeType::Select, NodeType::BoolSelect, NodeType::Atan2, NodeType::Sin, NodeType::Cos,
    NodeType::DeadZone, NodeType::Curve, NodeType::Quantizer, NodeType::Hysteresis,
    NodeType::And, NodeType::Or, NodeType::Xor, NodeType::Not,
    NodeType::RisingEdge, NodeType::FallingEdge, NodeType::Toggle,
    NodeType::SRLatch, NodeType::Hold, NodeType::DelayOn, NodeType::DelayOff,
    NodeType::Timer, NodeType::Pulse,
    NodeType::UnitDelay, NodeType::SampleHold, NodeType::Accumulator,
    NodeType::Integrator, NodeType::Differentiator, NodeType::Counter,
    NodeType::RateLimiter, NodeType::LowPass, NodeType::MovingAverage,
    NodeType::ErrorCalculator, NodeType::PID, NodeType::Feedforward, NodeType::DeadbandComparator,
    NodeType::KeySource, NodeType::ConstValue, NodeType::CustomOutput,
};
const int AllNodeTypeCount = sizeof(AllNodeTypes) / sizeof(AllNodeTypes[0]);

int GetCategoryNodeCount(NodeCategory cat)
{
    int cnt = 0;
    for (int i = 0; i < AllNodeTypeCount; ++i)
        if (GetNodeCategory(AllNodeTypes[i]) == cat) ++cnt;
    return cnt;
}

NodeType GetCategoryNodeType(NodeCategory cat, int index)
{
    int cnt = 0;
    for (int i = 0; i < AllNodeTypeCount; ++i) {
        if (GetNodeCategory(AllNodeTypes[i]) == cat) {
            if (cnt == index) return AllNodeTypes[i];
            ++cnt;
        }
    }
    return NodeType::ConstValue; // fallback
}

// ============================================================================
// OpMode helpers
// ============================================================================
int GetNodeOpModeCount(NodeType type)
{
    switch (type)
    {
    case NodeType::AddSubMulDiv: return 4;
    case NodeType::Compare:      return 6;
    default: return 0;
    }
}

const char* GetNodeOpModeLabel(NodeType type, int mode)
{
    if (type == NodeType::AddSubMulDiv) {
        static const char* labels[] = {"Add", "Sub", "Mul", "Div"};
        return (mode >= 0 && mode < 4) ? labels[mode] : "?";
    }
    if (type == NodeType::Compare) {
        static const char* labels[] = {"A>B", "A>=B", "A<=B", "A<B", "A==B", "A!=B"};
        return (mode >= 0 && mode < 6) ? labels[mode] : "?";
    }
    return nullptr;
}

// ============================================================================
// ShowBool helper
// ============================================================================
static void ShowBool(float v)
{
    if (v >= 0.5f)
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "True");
    else
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "False");
}

// ============================================================================
// DrawGenericNodeBody — generic node body drawing for all non-special types
// ============================================================================
void DrawGenericNodeBody(EditorNode& node,
                          const std::set<std::string>& analogKeys,
                          const std::vector<std::string>& availableKeys,
                          const std::vector<std::string>& outputNames,
                          const std::vector<std::string>& outputPaths,
                          std::function<bool(int)> isPinLinked,
                          std::function<void()> onModified)
{
    namespace ed = ax::NodeEditor;
    bool showBoolOutput = false;

    // Determine if outputs should show as bool
    switch (node.Type) {
    case NodeType::Compare: case NodeType::InRange:
    case NodeType::BoolSelect:
    case NodeType::And: case NodeType::Or: case NodeType::Xor: case NodeType::Not:
    case NodeType::RisingEdge: case NodeType::FallingEdge:
    case NodeType::Toggle: case NodeType::SRLatch:
    case NodeType::Hold: case NodeType::DelayOn: case NodeType::DelayOff:
    case NodeType::Timer: case NodeType::Pulse:
    case NodeType::Hysteresis:
    case NodeType::DeadbandComparator:
        showBoolOutput = true;
        break;
    default: break;
    }

    // -- Draw OpMode combo if applicable --
    int opCount = GetNodeOpModeCount(node.Type);
    if (opCount > 0) {
        NodeType capturedType = node.Type;
        ImGui::PushID((int)node.ID.Get());

        ImGui::SetNextItemWidth(110);
        std::string preview = GetNodeOpModeLabel(capturedType, node.OpMode);
        // 修正 imgui-node-editor 内 BeginCombo 弹出窗口定位
        ImVec2 popupPos = ImGui::GetCursorScreenPos();
        popupPos.y += ImGui::GetFrameHeightWithSpacing();
        ImGui::SetNextWindowPos(popupPos);
        if (ImGui::BeginCombo("##OpMode", preview.c_str())) {
            for (int i = 0; i < opCount; ++i) {
                bool sel = (node.OpMode == i);
                if (ImGui::Selectable(GetNodeOpModeLabel(capturedType, i), sel)) {
                    node.OpMode = i;
                    onModified();
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }

    // -- Draw input pins --
    for (auto& pin : node.Inputs) {
        ed::BeginPin(pin.ID, ed::PinKind::Input);
        DrawPinIcon(pin, isPinLinked((int)pin.ID.Get()), 255);
        ImGui::SameLine();
        ImGui::TextUnformatted(pin.Name.c_str());
        ed::EndPin();
    }

    // -- Draw node-specific Param inputs --
    {
        ImGui::PushID((int)node.ID.Get());
        auto F = [&](int idx, const char* label) {
            ImGui::SetNextItemWidth(100);
            if (ImGui::InputFloat(label, &node.Param[idx], 0.0f, 0.0f, "%.4f"))
                onModified();
        };

        switch (node.Type) {
        // --- Math ---
        case NodeType::ScaleBias:
            F(0, "Scale"); F(1, "Bias"); break;
        // --- Shaping ---
        case NodeType::DeadZone:
            F(0, "Dead Zone"); break;
        case NodeType::Quantizer:
            F(0, "Step"); break;
        case NodeType::Hysteresis:
            F(0, "Threshold"); F(1, "Hysteresis"); break;
        // --- Logic ---
        case NodeType::Hold:
        case NodeType::DelayOn:
        case NodeType::DelayOff:
            F(0, "Delay (s)"); break;
        case NodeType::Timer:
            F(0, "Duration (s)"); break;
        case NodeType::Pulse:
            F(0, "Width (s)"); break;
        // --- Memory ---
        case NodeType::Integrator:
            F(0, "Min"); F(1, "Max"); break;
        case NodeType::RateLimiter:
            F(0, "Up Rate"); F(1, "Down Rate"); break;
        case NodeType::LowPass:
            F(0, "Cutoff (Hz)"); break;
        case NodeType::MovingAverage:
            F(0, "Window"); break;
        // --- Control ---
        case NodeType::PID:
            F(0, "Kp"); F(1, "Ki"); F(2, "Kd");
            F(3, "Out Min"); F(4, "Out Max"); break;
        case NodeType::Feedforward:
            F(0, "Gain"); break;
        case NodeType::DeadbandComparator:
            F(0, "Band"); break;
        default: break;
        }
        ImGui::PopID();
    }

    // -- Draw separator --
    if (!node.Inputs.empty() && !node.Outputs.empty()) {
        float sepWidth = 140.0f;
        ImGui::Dummy(ImVec2(sepWidth, 2));
        auto* dl = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(cursor.x, cursor.y),
                    ImVec2(cursor.x + sepWidth, cursor.y),
                    IM_COL32(100, 100, 100, 255), 1.0f);
        ImGui::Dummy(ImVec2(sepWidth, 2));
    }

    // -- Draw output pins --
    for (auto& pin : node.Outputs) {
        ed::BeginPin(pin.ID, ed::PinKind::Output);
        DrawPinIcon(pin, isPinLinked((int)pin.ID.Get()), 255);
        ImGui::SameLine();
        ImGui::TextUnformatted(pin.Name.c_str());
        ImGui::SameLine(0, 20);
        if (showBoolOutput) ShowBool(node.Value);
        else                ImGui::TextDisabled("%.3f", node.Value);
        ed::EndPin();
    }
}
