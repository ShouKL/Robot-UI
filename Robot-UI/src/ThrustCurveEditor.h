#pragma once
#include "Robot_API/robot_api.h"
#include "implot.h"
#include <vector>
#include <algorithm>

// X=Thrust, Y=PWM Duty — internal storage (Thrust, PWM)
// Raw points stored as (Thrust, PWM), displayed as-is.
// Model: PWM = f(Thrust) with 5+1 control points:
//   (nt_end, PWM-) → (nt_mid, np_mid) → (0, np_ini) → (pt_mid, pp_mid) → (pt_end, PWM+)
//                                          ↕ origin not used

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
inline double EvalModel(double thrust, const double* p) {
    double pwmm = p[8], pwmp = p[9];
    if (thrust <= p[0]) return pwmm;
    if (thrust >= p[3]) return pwmp;
    if (thrust <= p[1]) { double t = (thrust-p[0])/(p[1]-p[0]); return pwmm + t*(p[5]-pwmm); }
    if (thrust <= 0.0)  { double t = (thrust-p[1])/(0.0-p[1]);   return p[5] + t*(p[4]-p[5]); }
    if (thrust <= p[2]) { double t = (thrust-0.0)/(p[2]-0.0);    return p[6] + t*(p[7]-p[6]); }
    double t = (thrust-p[2])/(p[3]-p[2]); return p[7] + t*(pwmp-p[7]);
}

class ThrustCurveEditor {
public:
    ThrustCurveEditor() = default;
    void Open(const char* motorName, ThrustCurve& curve);
    void OpenStandalone(const char* motorName);
    void Close() { m_Open = false; }
    bool IsOpen() const { return m_Open; }
    void Draw();
    // Get the fitted curve as a formatted string for copy-paste
    const char* GetOutputString() const { return m_OutputStr; }
private:
    void DrawPlot();
    void DrawPopupEditor();
    void SortRawPoints();
    void SortFitPoints();
    void LoadFromCurve();
    void FitAndApply();

    bool m_Open=false; char m_MotorName[128]={};
    ThrustCurve* m_Curve=nullptr;
    std::vector<ImVec2> m_RawPoints, m_FitPoints; // (Thrust, PWM)
    int m_SelectedIdx=-1;
    bool m_EditPopupOpen=false, m_PendingAdd=false;
    bool m_Standalone=false;
    float m_PwmMin=0.0f, m_PwmMax=0.0f;
    float m_AnchorNP_INI=0.0f,m_AnchorNP_MID=0.0f,m_AnchorPP_INI=0.0f,m_AnchorPP_MID=0.0f;
    ThrustCurve m_OwnCurve;
    char m_OutputStr[512]={};
};
