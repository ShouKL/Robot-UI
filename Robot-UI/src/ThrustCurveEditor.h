#pragma once
#include "Robot_API/robot_api.h"
#include "implot.h"
#include <vector>
#include <algorithm>

// X=Thrust, Y=PWM Duty — internal storage (Thrust, PWM)
// Raw points stored as (Thrust, PWM), displayed as-is.
// Fitted model: 6 control points connected as polyline:
//   (nt_end,PwmMin)→(nt_mid,np_mid)→(0,np_ini)→(0,pp_ini)→(pt_mid,pp_mid)→(pt_end,PwmMax)

inline double SamplePWM(const std::vector<ImVec2>& pts, double thrust) {
    if (pts.empty()) return 0.0;
    if (thrust <= pts.front().x) return (double)pts.front().y;
    if (thrust >= pts.back().x)  return (double)pts.back().y;
    for (size_t i = 0; i + 1 < pts.size(); ++i)
        if (thrust >= pts[i].x && thrust <= pts[i+1].x) {
            float dx = pts[i+1].x - pts[i].x; if (dx <= 0.0f) return (double)pts[i].y;
            float t = (float)((thrust - pts[i].x) / dx);
            return (double)(pts[i].y + t * (pts[i+1].y - pts[i].y));
        }
    return 0.0;
}

// Model: Thrust → PWM. p[0..3]=Thrust anchors, p[4..7]=PWM anchors (see FitAndApply)

class ThrustCurveEditor {
public:
    ThrustCurveEditor() = default;
    void Open(const char* motorName, ThrustCurve& curve);
    void Open();
    void Close() { m_Open = false; }
    bool IsOpen() const { return m_Open; }
    void Draw();
    ThrustCurve& GetCurve() { return m_OwnCurve; }
    const char* GetOutputString() const { return m_OutputStr; }
private:
    void DrawPlot();
    void DrawPointTable();
    void SortRawPoints();
    void SortFitPoints();
    void LoadFromCurve();
    void Fit();
    void SaveRawPointsToCurve();

    void Save();   // commit changes to curve
    void Revert();  // undo all changes, restore snapshot

    bool m_Open=false; char m_MotorName[128]={};
    ThrustCurve* m_Curve=nullptr;
    std::vector<ImVec2> m_RawPoints, m_FitPoints; // (Thrust, PWM)
    int m_SelectedIdx=-1;
    bool m_EditPopupOpen=false, m_PendingAdd=false;
    float m_PwmMin=0.0f, m_PwmMax=0.0f, m_DefaultPwm=0.0f;
    float m_AnchorNP_INI=0.0f,m_AnchorNP_MID=0.0f,m_AnchorPP_INI=0.0f,m_AnchorPP_MID=0.0f;
    ThrustCurve m_OwnCurve;
    ThrustCurve m_Snapshot;   // backup on Open, restored on Revert
    char m_OutputStr[512]={};
};
