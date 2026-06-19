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
    case PinType::Bool:  return ImColor(226, 147,  74);
    case PinType::Int:   return ImColor( 74, 180, 226);
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
    case NodeType::MathFunc:
        return NodeCategory::Math;

    // Shaping
    case NodeType::DeadZone:
    case NodeType::Curve:
    case NodeType::Quantizer:
    case NodeType::Hysteresis:
        return NodeCategory::Shaping;

    // Logic
    case NodeType::LogicOp:
    case NodeType::RisingEdge:
    case NodeType::Toggle:
    case NodeType::SRLatch:
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
    case NodeType::GlobalRead:
    case NodeType::GlobalWrite:
        return NodeCategory::Memory;

    // Control
    case NodeType::PID:
    case NodeType::DeadbandComparator:
        return NodeCategory::Control;

    // Legacy / special
    case NodeType::KeySource:    return NodeCategory::Math;
    case NodeType::ConstValue:   return NodeCategory::Math;
    case NodeType::LookupTable:  return NodeCategory::Math;
    case NodeType::CustomOutput: return NodeCategory::Control;
    case NodeType::ShortcutTrigger: return NodeCategory::Control;
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
    case NodeType::ShortcutTrigger: return ImColor(200, 160,  60);
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
    case NodeType::AddSubMulDiv:  return "Arithmetic";
    case NodeType::ScaleBias:     return "Scale & Bias";
    case NodeType::Lerp:          return "Lerp";
    case NodeType::Clamp:         return "Clamp";
    case NodeType::Remap:         return "Remap";
    case NodeType::Compare:       return "Compare";
    case NodeType::InRange:       return "In Range";
    case NodeType::Select:        return "Select";
    case NodeType::BoolSelect:    return "Select";
    case NodeType::MathFunc:      return "Math Func";

    // Shaping
    case NodeType::DeadZone:      return "Dead Zone";
    case NodeType::Curve:         return "Curve";
    case NodeType::Quantizer:     return "Quantizer";
    case NodeType::Hysteresis:    return "Hysteresis";

    // Logic
    case NodeType::LogicOp:       return "Logic Op";
    case NodeType::RisingEdge:    return "Edge Detect";
    case NodeType::Toggle:        return "Toggle";
    case NodeType::SRLatch:       return "SR Latch";
    case NodeType::DelayOn:       return "Delay";
    case NodeType::DelayOff:      return "Delay Off";
    case NodeType::Timer:         return "Timer";
    case NodeType::Pulse:         return "Pulse";

    // Memory
    case NodeType::UnitDelay:     return "Unit Delay";
    case NodeType::SampleHold:    return "Sample Hold";
    case NodeType::Accumulator:   return "Accumulator";
    case NodeType::Integrator:    return "Integrator";
    case NodeType::Differentiator:return "Differentiator";
    case NodeType::Counter:       return "Counter";
    case NodeType::RateLimiter:   return "Rate Limiter";
    case NodeType::LowPass:       return "Low Pass";
    case NodeType::MovingAverage: return "Moving Average";
    case NodeType::GlobalRead:   return "Global Read";
    case NodeType::GlobalWrite:  return "Global Write";

    // Control
    case NodeType::PID:                return "PID";
    case NodeType::DeadbandComparator: return "Deadband Comparator";

    // Legacy / special
    case NodeType::KeySource:    return "Key Source";
    case NodeType::ConstValue:   return "Const Value";
    case NodeType::LookupTable:  return "Lookup Table";
    case NodeType::CustomOutput: return "Output";
    case NodeType::ShortcutTrigger: return "Shortcut";

    default: return "???";
    }
}

// ============================================================================
// DrawPinIcon
// ============================================================================
#include <unordered_map>

// Thread-local pin screen rect map for click-to-connect hit-testing.
// Written by DrawPinIcon, read by NodeGraph::Draw.
static std::unordered_map<int, ImRect> s_PinScreenRects;

const std::unordered_map<int, ImRect>& GetPinScreenRects() { return s_PinScreenRects; }
void ClearPinScreenRects() { s_PinScreenRects.clear(); }

void DrawPinIcon(const EditorPin& pin, bool connected, int alpha)
{
    ImColor color = GetIconColor(pin.Type);
    color.Value.w = alpha / 255.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float iconSize = 24.0f;
    float half = iconSize * 0.5f;
    ImVec2 center(pos.x + half, pos.y + half);

    // Record screen rect for hit-test
    s_PinScreenRects[(int)pin.ID.Get()] = ImRect(pos.x, pos.y, pos.x + iconSize, pos.y + iconSize);

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
    float dt,
    float* globals,
    int globalsCount)
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
    case NodeType::MathFunc:
    {
        float a = GetPinByIndex(node, pinVals, 0);
        float b = GetPinByIndex(node, pinVals, 1);
        switch (node.OpMode) {
        case 0: return std::sin(a);
        case 1: return std::cos(a);
        case 2: return std::atan2(a, b);
        default: return std::sin(a);
        }
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
        // Param[0..7] = 4 control point pairs (x0,y0, x1,y1, x2,y2, x3,y3) for piecewise-linear
        // Build sorted control points (skip duplicates with same x)
        struct Pt { float x, y; };
        Pt pts[4] = {};
        int nPts = 0;
        for (int i = 0; i < 4; ++i) {
            float x = node.Param[i * 2];
            float y = node.Param[i * 2 + 1];
            if (x == 0.0f && y == 0.0f && i > 0) continue; // skip unset points beyond first
            bool duplicate = false;
            for (int j = 0; j < nPts; ++j)
                if (pts[j].x == x) { duplicate = true; break; }
            if (!duplicate) pts[nPts++] = {x, y};
        }
        // Sort by x
        for (int i = 0; i < nPts - 1; ++i)
            for (int j = i + 1; j < nPts; ++j)
                if (pts[j].x < pts[i].x)
                    std::swap(pts[i], pts[j]);

        if (nPts == 0) return in;
        if (nPts == 1) return pts[0].y;

        // Clamp / extrapolate
        if (in <= pts[0].x) return pts[0].y;
        if (in >= pts[nPts - 1].x) return pts[nPts - 1].y;

        // Linear interpolation between nearest points
        for (int i = 0; i < nPts - 1; ++i) {
            if (in >= pts[i].x && in <= pts[i + 1].x) {
                float t = (pts[i + 1].x - pts[i].x != 0.0f)
                    ? (in - pts[i].x) / (pts[i + 1].x - pts[i].x) : 0.0f;
                return pts[i].y + t * (pts[i + 1].y - pts[i].y);
            }
        }
        return in;
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
    case NodeType::LogicOp:
    {
        bool a = AsBool(GetPinByIndex(node, pinVals, 0));
        bool b = AsBool(GetPinByIndex(node, pinVals, 1));
        switch (node.OpMode) {
        case 0: return FromBool(a && b);
        case 1: return FromBool(a || b);
        case 2: return FromBool(a != b);
        case 3: return FromBool(!a);
        default: return FromBool(a && b);
        }
    }
    case NodeType::RisingEdge:
    {
        bool cur = AsBool(GetPinByIndex(node, pinVals, 0));
        bool prev = node.StateB[0];
        node.StateB[0] = cur;
        if (node.OpMode == 1) return FromBool(!cur && prev);  // Falling
        return FromBool(cur && !prev);                           // Rising
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
        bool prevS = (node.StateF[1] >= 0.5f);
        bool prevR = (node.StateF[2] >= 0.5f);
        // S 0→1: set latch
        if (s && !prevS) node.StateF[0] = 1.0f;
        // R 0→1: reset latch (S wins if both fire same frame)
        if (r && !prevR) node.StateF[0] = 0.0f;
        node.StateF[1] = s ? 1.0f : 0.0f;
        node.StateF[2] = r ? 1.0f : 0.0f;
        return node.StateF[0];
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
        float in   = GetPinByIndex(node, pinVals, 0);
        bool trig  = AsBool(GetPinByIndex(node, pinVals, 1));
        bool reset = AsBool(GetPinByIndex(node, pinVals, 2));

        bool latchMode = (node.Param[1] != 0.0f);  // Param[1] != 0 → latch enabled

        // Reset clears the latch flag
        if (reset)
            node.StateB[0] = false;

        // Param[0] = 0 → manual (Trig pin controls sampling)
        // Param[0] != 0 → auto: sample whenever input changes
        if (node.Param[0] != 0.0f) {
            // Auto mode
            if (std::abs(in - node.StateF[1]) > 1e-6f) {
                if (!latchMode || !node.StateB[0]) {
                    node.StateF[0] = in;
                    node.StateF[1] = in;
                    if (latchMode) node.StateB[0] = true;  // lock
                }
            }
        } else if (trig) {
            // Manual mode: Trig rising edge
            bool prevTrig = node.StateB[1];
            node.StateB[1] = trig;
            if (trig && !prevTrig) {
                if (!latchMode || !node.StateB[0]) {
                    node.StateF[0] = in;
                    if (latchMode) node.StateB[0] = true;  // lock
                }
            }
        } else {
            node.StateB[1] = false;
        }
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
        float rawInc = GetPinByIndex(node, pinVals, 0);
        float prevRaw = node.StateF[1];
        bool inc = (prevRaw < 0.5f && rawInc >= 0.5f);
        node.StateF[1] = rawInc;

        bool reset = AsBool(GetPinByIndex(node, pinVals, 1));
        if (reset && !node.StateB[2]) {
            node.StateF[0] = node.Param[0];  // Reset Value
        } else if (inc)
        {
            int limit = (int)node.Param[1];
            if (limit <= 0) limit = 5;
            float startVal = node.Param[2];  // Start Value (wrap target)
            node.StateF[0] += 1.0f;
            if (node.StateF[0] > (float)limit) node.StateF[0] = startVal;
        }
        node.StateB[2] = reset;
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
    case NodeType::GlobalRead:
    {
        int idx = node.GlobalVarId;
        if (idx >= 0 && globals && idx < globalsCount) return globals[idx];
        return 0.0f;
    }
    case NodeType::GlobalWrite:
    {
        float in  = GetPinByIndex(node, pinVals, 0);
        bool trig = AsBool(GetPinByIndex(node, pinVals, 1));
        int idx = node.GlobalVarId;
        bool prevTrig = (node.StateF[1] >= 0.5f);
        if (trig && !prevTrig && idx >= 0 && globals && idx < globalsCount)
            globals[idx] = in;
        node.StateF[1] = trig ? 1.0f : 0.0f;
        if (idx >= 0 && globals && idx < globalsCount) return globals[idx];
        return 0.0f;
    }

    // ==================== LOOKUP ====================
    case NodeType::LookupTable:
    {
        float idx = GetPinByIndex(node, pinVals, 0);
        int cnt = (int)node.ModeLabels.size();
        if (cnt <= 0) return 0.0f;
        int i = std::max(0, std::min(cnt - 1, (int)std::floor(idx)));
        float v = 0.0f;
        try { v = std::stof(node.ModeLabels[i]); } catch (...) {}
        return v;
    }

    // ==================== CONTROL ====================
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
        // Integral (StateF[0]) with clamping anti-windup
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
        // Output limit with anti-windup (clamping integrator)
        float out = P + I + D;
        bool hasLimit = (node.Param[3] != 0.0f || node.Param[4] != 0.0f);
        if (hasLimit) {
            float lo = node.Param[3], hi = node.Param[4];
            if (hi > lo) {
                float clamped = std::clamp(out, lo, hi);
                // Anti-windup: if output saturated, prevent integrator from winding up further
                if (clamped != out && ki != 0.0f) {
                    // Back-calculate integral term to match clamped output
                    node.StateF[0] = (clamped - P - D) / ki;
                }
                out = clamped;
            }
        }
        return out;
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
    case NodeType::ShortcutTrigger:
    {
        // Input Trigger → output passthrough.
        // Rising edge detection is handled in NodeGraph::EvaluateForDisplay().
        // Old compat: if KeyName is set, use gamepad key as trigger source.
        if (!node.KeyName.empty()) {
            auto it = keyValues.find(node.KeyName);
            return (it != keyValues.end() && it->second >= 0.5f) ? 1.0f : 0.0f;
        }
        return GetPinByIndex(node, pinVals, 0);
    }
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
    auto OB = [&](const char* label) { return EditorPin(nextId(), label, PinType::Bool); };
    auto OI = [&](const char* label) { return EditorPin(nextId(), label, PinType::Int); };

    switch (type)
    {
    // ==================== MATH ====================
    case NodeType::AddSubMulDiv: {
        auto n = N("Arithmetic"); n.Inputs = {I("A"), I("B")}; n.Outputs = {O("Out")}; return n; }
    case NodeType::ScaleBias: {
        auto n = N("Scale & Bias"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 1.0f; return n; }
    case NodeType::Lerp: {
        auto n = N("Lerp"); n.Inputs = {I("A"), I("B"), I("T")}; n.Outputs = {O("Out")}; return n; }
    case NodeType::Clamp: {
        auto n = N("Clamp"); n.Inputs = {I("Value"), I("Min"), I("Max")}; n.Outputs = {O("Out")};
        n.Param[2] = 1.0f; return n; }
    case NodeType::Remap: {
        auto n = N("Remap"); n.Inputs = {I("Value"), I("InMin"), I("InMax"), I("OutMin"), I("OutMax")};
        n.Outputs = {O("Out")}; n.Param[1] = 1.0f; return n; }
    case NodeType::Compare: {
        auto n = N("Compare"); n.Inputs = {I("A"), I("B")}; n.Outputs = {OB("Bool")}; return n; }
    case NodeType::InRange: {
        auto n = N("In Range"); n.Inputs = {I("Value"), I("Min"), I("Max")}; n.Outputs = {OB("Bool")};
        n.Param[2] = 1.0f; return n; }
    case NodeType::Select:
    case NodeType::BoolSelect: {  // deprecated, loads as Select
        auto n = N("Select"); n.Inputs = {I("Cond"), I("A"), I("B")}; n.Outputs = {O("Out")}; return n; }
    case NodeType::MathFunc: {
        auto n = N("Math Func"); n.Inputs = {I("A"), I("B")}; n.Outputs = {O("Out")}; return n; }

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
        auto n = N("Hysteresis"); n.Inputs = {I("In")}; n.Outputs = {OB("Out")};
        n.Param[0] = 0.5f; n.Param[1] = 0.1f; return n; }

    // ==================== LOGIC ====================
    case NodeType::LogicOp: {
        auto n = N("Logic Op"); n.Inputs = {I("A"), I("B")}; n.Outputs = {OB("Out")}; return n; }
    case NodeType::RisingEdge: {
        auto n = N("Edge Detect"); n.Inputs = {I("In")}; n.Outputs = {OB("Out")};
        n.StateB[0] = false; return n; }
    case NodeType::Toggle: {
        auto n = N("Toggle"); n.Inputs = {I("Clk")}; n.Outputs = {OB("Q")};
        n.StateB[0] = false; return n; }
    case NodeType::SRLatch: {
        auto n = N("SR Latch"); n.Inputs = {I("S"), I("R")}; n.Outputs = {OB("Q")};
        n.StateB[0] = false; return n; }
    case NodeType::DelayOn: {
        auto n = N("Delay"); n.Inputs = {I("In")}; n.Outputs = {OB("Out")};
        n.Param[0] = 0.5f; return n; }
    case NodeType::DelayOff: {
        auto n = N("Delay Off"); n.Inputs = {I("In")}; n.Outputs = {OB("Out")};
        n.Param[0] = 0.5f; return n; }
    case NodeType::Timer: {
        auto n = N("Timer"); n.Inputs = {I("Trig")}; n.Outputs = {OB("Out")};
        n.Param[0] = 0.5f; n.StateB[0] = false; return n; }
    case NodeType::Pulse: {
        auto n = N("Pulse"); n.Inputs = {I("In")}; n.Outputs = {OB("Out")};
        n.Param[0] = 0.1f; n.StateB[0] = false; return n; }

    // ==================== MEMORY ====================
    case NodeType::UnitDelay: {
        auto n = N("Unit Delay"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.StateF[0] = 0.0f; return n; }
    case NodeType::SampleHold: {
        auto n = N("Sample Hold"); n.Inputs = {I("Value"), I("Trig"), I("Reset")}; n.Outputs = {O("Out")};
        n.StateF[0] = 0.0f; n.StateF[1] = 0.0f; n.StateB[0] = false; n.Param[1] = 0.0f; return n; }
    case NodeType::Accumulator: {
        auto n = N("Accumulator"); n.Inputs = {I("In"), I("Reset")}; n.Outputs = {O("Sum")};
        n.StateF[0] = 0.0f; return n; }
    case NodeType::Integrator: {
        auto n = N("Integrator"); n.Inputs = {I("In"), I("Reset")}; n.Outputs = {O("Out")};
        n.StateF[0] = 0.0f; n.Param[2] = 0.0f; return n; }
    case NodeType::Differentiator: {
        auto n = N("Differentiator"); n.Inputs = {I("In")}; n.Outputs = {O("d/dt")};
        n.StateF[1] = 0.0f; return n; }
    case NodeType::Counter: {
        auto n = N("Counter"); n.Inputs = {I("Inc"), I("Reset")}; n.Outputs = {OI("Count")};
        // Param[0]=Reset Value, Param[1]=Cycle Limit, Param[2]=Start Value
        n.Param[0] = 0.0f; n.Param[1] = 5.0f; n.Param[2] = 0.0f; n.StateF[0] = 0.0f; return n; }
    case NodeType::RateLimiter: {
        auto n = N("Rate Limiter"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 10.0f; n.Param[1] = 10.0f; n.StateF[0] = 0.0f; return n; }
    case NodeType::LowPass: {
        auto n = N("Low Pass"); n.Inputs = {I("In")}; n.Outputs = {O("Out")};
        n.Param[0] = 10.0f; n.StateF[0] = 0.0f; return n; }
    case NodeType::MovingAverage: {
        auto n = N("Moving Average"); n.Inputs = {I("In")}; n.Outputs = {O("Avg")};
        n.Param[0] = 5.0f; return n; }
    case NodeType::GlobalRead: {
        auto n = N("Global Read"); n.Outputs = {O("Value")};
        return n; }
    case NodeType::GlobalWrite: {
        auto n = N("Global Write"); n.Inputs = {I("Value"), I("Trig")}; n.Outputs = {O("Out")};
        return n; }

    // ==================== CONTROL ====================
    case NodeType::PID: {
        auto n = N("PID"); n.Inputs = {I("Setpoint"), I("PV"), I("Reset")}; n.Outputs = {O("Out")};
        n.Param[0] = 1.0f; n.StateF[0] = 0.0f; n.StateF[1] = 0.0f; return n; }
    case NodeType::DeadbandComparator: {
        auto n = N("Deadband Comparator"); n.Inputs = {I("Error")}; n.Outputs = {OB("OnTarget")};
        n.Param[0] = 0.01f; return n; }

    // ==================== LEGACY/SPECIAL ====================
    case NodeType::KeySource: {
        auto n = N("Key Source"); n.Outputs = {O("Value")}; return n; }
    case NodeType::ConstValue: {
        auto n = N("Const Value"); n.Outputs = {O("Value")}; n.Value = 0.0f; return n; }
    case NodeType::CustomOutput: {
        auto n = N("Output"); n.Inputs = {I("Value")}; return n; }
    case NodeType::ShortcutTrigger: {
        auto n = N("Shortcut"); n.Inputs = {I("Trigger")}; n.Outputs = {O("Value")};
        n.ShortcutActionIndex = -1; n.ShortcutSendIndex = -1; n.ShortcutSendMode = 0; return n; }
    case NodeType::LookupTable: {
        auto n = N("Lookup Table"); n.Inputs = {OI("Index")}; n.Outputs = {O("Value")};
        n.ModeLabels = {"0", "1", "2", "3", "4", "5", "6", "7"};
        return n; }
    }
    return EditorNode(0, "???", type);
}

// ============================================================================
// AllNodeTypes — for dynamic menu/context-menu iteration
// ============================================================================
const NodeType AllNodeTypes[] = {
    NodeType::AddSubMulDiv, NodeType::ScaleBias, NodeType::Lerp,
    NodeType::Clamp, NodeType::Remap, NodeType::Compare, NodeType::InRange,
    NodeType::Select, NodeType::MathFunc,
    NodeType::DeadZone, NodeType::Curve, NodeType::Quantizer, NodeType::Hysteresis,
    NodeType::LogicOp, NodeType::RisingEdge, NodeType::Toggle,
    NodeType::SRLatch, NodeType::DelayOn, NodeType::DelayOff,
    NodeType::Timer, NodeType::Pulse,
    NodeType::UnitDelay, NodeType::SampleHold, NodeType::Accumulator,
    NodeType::Integrator, NodeType::Differentiator, NodeType::Counter,
    NodeType::RateLimiter, NodeType::LowPass, NodeType::MovingAverage,
    NodeType::GlobalRead, NodeType::GlobalWrite,
    NodeType::PID, NodeType::DeadbandComparator,
    NodeType::KeySource, NodeType::ConstValue, NodeType::LookupTable, NodeType::CustomOutput,
    NodeType::ShortcutTrigger,
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
    case NodeType::LogicOp:      return 4;
    case NodeType::RisingEdge:   return 2;
    case NodeType::MathFunc:     return 3;
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
    if (type == NodeType::LogicOp) {
        static const char* labels[] = {"AND", "OR", "XOR", "NOT"};
        return (mode >= 0 && mode < 4) ? labels[mode] : "?";
    }
    if (type == NodeType::RisingEdge) {
        static const char* labels[] = {"Rising", "Falling"};
        return (mode >= 0 && mode < 2) ? labels[mode] : "?";
    }
    if (type == NodeType::MathFunc) {
        static const char* labels[] = {"Sin", "Cos", "Atan2"};
        return (mode >= 0 && mode < 3) ? labels[mode] : "?";
    }
    return nullptr;
}

// ============================================================================
// DrawPinTypeSelector — single button that cycles Float → Bool → Int → Float.
// Uses a regular Button (not ArrowButton) because ArrowButton behaves
// unreliably inside imgui-node-editor canvas.
// When insidePin=true (called inside BeginPin/EndPin), uses a non-Button
// (Text + IsItemClicked) so that clicks on the type area still belong
// to the pin for link-dragging purposes.
// ============================================================================
void DrawPinTypeSelector(EditorPin* pin, std::function<void()> onModified)
{
    static const char* labels[] = {"Float", "Bool", "Int"};
    static const PinType types[] = {PinType::Float, PinType::Bool, PinType::Int};
    static const ImU32 colors[] = {IM_COL32(147,226,74,255), IM_COL32(226,147,74,255), IM_COL32(74,180,226,255)};
    int cur = 0;
    for (int i = 0; i < 3; ++i) if (pin->Type == types[i]) { cur = i; break; }

    ImGui::PushID((int)pin->ID.Get());
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));
    ImVec4 col = ImGui::ColorConvertU32ToFloat4(colors[cur]);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button(labels[cur], ImVec2(54, 0)))
    {
        pin->Type = types[(cur + 1) % 3];
        if (onModified) onModified();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    ImGui::PopID();
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

    // Helper to display a pin value according to its type
    auto ShowPinValue = [](PinType type, float v) {
        switch (type) {
        case PinType::Bool: ShowBool(v); break;
        case PinType::Int:  ImGui::TextDisabled("%d", (int)std::round(v)); break;
        default:            ImGui::TextDisabled("%.3f", v); break;
        }
    };

    // -- Draw OpMode combo as a cycle button (no popup, avoids
    //    positioning issues inside imgui-node-editor canvas) --
    int opCount = GetNodeOpModeCount(node.Type);
    if (opCount > 0) {
        NodeType capturedType = node.Type;
        ImGui::PushID((int)node.ID.Get());

        ImGui::SetNextItemWidth(110);
        if (ImGui::ArrowButton("##Prev", ImGuiDir_Left)) {
            node.OpMode = (node.OpMode - 1 + opCount) % opCount;
            onModified();
        }
        ImGui::SameLine(0, 2);
        ImGui::Text("%s", GetNodeOpModeLabel(capturedType, node.OpMode));
        ImGui::SameLine(0, 2);
        if (ImGui::ArrowButton("##Next", ImGuiDir_Right)) {
            node.OpMode = (node.OpMode + 1) % opCount;
            onModified();
        }
        ImGui::PopID();
    }

    // -- Draw input pins --
    // Keep only icon+name inside BeginPin/EndPin so the editor's link-creation
    // zone doesn't intercept clicks on the type button and value display.
    for (size_t i = 0; i < node.Inputs.size(); ++i) {
        auto& pin = node.Inputs[i];
        ed::BeginPin(pin.ID, ed::PinKind::Input);
        DrawPinIcon(pin, isPinLinked((int)pin.ID.Get()), 255);
        ImGui::SameLine();
        ImGui::TextUnformatted(pin.Name.c_str());
        ed::EndPin();
        ImGui::SameLine(0, 6);
        DrawPinTypeSelector(&pin, onModified);
        ImGui::SameLine(0, 8);
        float v = (i < 4) ? node.InputValues[i] : 0.0f;
        ShowPinValue(pin.Type, v);
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
        case NodeType::DelayOn:
        case NodeType::DelayOff:
            F(0, "Delay (s)"); break;
        case NodeType::Timer:
            F(0, "Duration (s)"); break;
        case NodeType::Pulse:
            F(0, "Width (s)"); break;
        // --- Memory ---
        case NodeType::SampleHold: {
            bool autoTrig = (node.Param[0] != 0.0f);
            if (ImGui::Checkbox("Auto", &autoTrig)) {
                node.Param[0] = autoTrig ? 1.0f : 0.0f;
                onModified();
            }
            bool latch = (node.Param[1] != 0.0f);
            if (ImGui::Checkbox("Latch", &latch)) {
                node.Param[1] = latch ? 1.0f : 0.0f;
                if (!latch) node.StateB[0] = false;  // clear latch flag when disabled
                onModified();
            }
            break;
        }
        case NodeType::Counter:
            F(0, "Reset Value");
            F(1, "Cycle Limit");
            F(2, "Start Value"); break;
        case NodeType::Integrator:
            F(0, "Min"); F(1, "Max"); break;
        case NodeType::RateLimiter:
            F(0, "Up Rate"); F(1, "Down Rate"); break;
        case NodeType::LowPass:
            F(0, "Cutoff (Hz)"); break;
        case NodeType::MovingAverage:
            F(0, "Window"); break;
        case NodeType::GlobalRead:
        case NodeType::GlobalWrite:
            // Variable name is shown in DrawNodeContents (like KeySource)
            break;
        // --- Control ---
        case NodeType::PID:
            F(0, "Kp"); F(1, "Ki"); F(2, "Kd");
            F(3, "Out Min"); F(4, "Out Max"); break;
        case NodeType::DeadbandComparator:
            F(0, "Band"); break;
        // --- Lookup Table ---
        case NodeType::LookupTable: {
            auto& labels = node.ModeLabels;
            if (labels.empty()) labels.push_back("0");
            int count = (int)labels.size();

            if (ImGui::Button("+ Slot")) {
                labels.push_back("0");
                onModified();
            }
            ImGui::SameLine();
            if (ImGui::Button("- Slot") && count > 1) {
                labels.pop_back();
                onModified();
            }

            for (int i = 0; i < (int)labels.size(); ++i) {
                ImGui::PushID(i);

                // Move up / down buttons
                if (i > 0 && ImGui::ArrowButton("##up", ImGuiDir_Up)) {
                    std::swap(labels[i], labels[i - 1]);
                    onModified();
                }
                ImGui::SameLine();
                if (i < (int)labels.size() - 1 && ImGui::ArrowButton("##dn", ImGuiDir_Down)) {
                    std::swap(labels[i], labels[i + 1]);
                    onModified();
                }
                ImGui::SameLine();

                // Index label
                char idxLabel[16];
                snprintf(idxLabel, sizeof(idxLabel), "[%d]", i);
                ImGui::TextUnformatted(idxLabel);
                ImGui::SameLine();

                // Value — use InputText with char buffer for reliable editing
                char buf[32];
                snprintf(buf, sizeof(buf), "%s", labels[i].c_str());
                ImGui::SetNextItemWidth(100);
                if (ImGui::InputText("##val", buf, sizeof(buf), ImGuiInputTextFlags_CharsDecimal)) {
                    labels[i] = buf;
                    onModified();
                }

                ImGui::PopID();
            }
            break;
        }
        default: break;
        }
        ImGui::PopID();
    }

    // -- Draw separator --
    if (!node.Inputs.empty() && !node.Outputs.empty()) {
        float sepWidth = 140.0f;
        float padX = ImGui::GetStyle().FramePadding.x;
        ImGui::Dummy(ImVec2(sepWidth, 2));
        auto* dl = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(cursor.x - padX, cursor.y),
                    ImVec2(cursor.x + sepWidth + padX, cursor.y),
                    IM_COL32(100, 100, 100, 255), 1.0f);
        ImGui::Dummy(ImVec2(sepWidth, 2));
    }

    // -- Draw output pins --
    for (auto& pin : node.Outputs) {
        ed::BeginPin(pin.ID, ed::PinKind::Output);
        DrawPinIcon(pin, isPinLinked((int)pin.ID.Get()), 255);
        ImGui::SameLine();
        ImGui::TextUnformatted(pin.Name.c_str());
        ed::EndPin();
        ImGui::SameLine(0, 6);
        DrawPinTypeSelector(&pin, onModified);
        ImGui::SameLine(0, 8);
        ShowPinValue(pin.Type, node.Value);
    }
}
