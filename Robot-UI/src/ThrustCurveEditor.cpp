#include "ThrustCurveEditor.h"
#include "Walnut/Core/Log.h"
#include <cstdio>
#include <cmath>

void ThrustCurveEditor::Open(const char* n, ThrustCurve& c) {
    WL_INFO_TAG("CURVE", "Open(external): motor={}, raw_pts={}", n, c.raw_thrust.size());
    _snprintf_s(m_MotorName,sizeof(m_MotorName),"%s",n); m_Curve=&c;
    m_Snapshot = *m_Curve;  // backup for undo
    LoadFromCurve(); m_SelectedIdx=-1; m_EditPopupOpen=false; m_Open=true;
    BeginEdit();
    if(m_RawPoints.size()>=2){SortRawPoints();Fit();}
}

void ThrustCurveEditor::Open() {
    WL_INFO_TAG("CURVE", "Open(internal): raw_pts={}, pwm_min={:.1f}, pwm_max={:.1f}", m_OwnCurve.raw_thrust.size(), m_OwnCurve.pwm_min, m_OwnCurve.pwm_max);
    _snprintf_s(m_MotorName,sizeof(m_MotorName),"Thrust Curve");
    m_Curve=&m_OwnCurve;
    m_Snapshot = *m_Curve;  // backup for undo
    LoadFromCurve(); m_SelectedIdx=-1; m_EditPopupOpen=false; m_Open=true; m_OutputStr[0]=0;
    BeginEdit();
    WL_INFO_TAG("CURVE", "Open: loaded {} raw pts, pwm=[{:.1f},{:.1f}]", m_RawPoints.size(), m_PwmMin, m_PwmMax);
    if(m_RawPoints.size()>=2){SortRawPoints();Fit();}
}

void ThrustCurveEditor::Save() {
    if(!m_Curve)return;
    SaveRawPointsToCurve();
    // commit limits+mid back to curve
    m_Curve->pwm_min = m_PwmMin;
    m_Curve->pwm_max = m_PwmMax;
    m_Curve->default_pwm = m_DefaultPwm;
    // update snapshot so Close won't revert
    m_Snapshot = *m_Curve;
    ApplyEdit();
}

void ThrustCurveEditor::Revert() {
    if(!m_Curve)return;
    *m_Curve = m_Snapshot;
    LoadFromCurve(); m_SelectedIdx=-1; m_EditPopupOpen=false; m_FitPoints.clear();
    CancelEdit();
    // restore fitted curve from saved params
    double ne=m_Curve->nt_end.value, nm=m_Curve->nt_mid.value, pm=m_Curve->pt_mid.value, pe=m_Curve->pt_end.value;
    if (fabs(ne)+fabs(nm)+fabs(pm)+fabs(pe) >= 0.001) {
        m_FitPoints.push_back(ImVec2((float)ne, m_PwmMin));
        m_FitPoints.push_back(ImVec2((float)nm, m_AnchorNP_MID));
        m_FitPoints.push_back(ImVec2(0, m_AnchorNP_INI));
        m_FitPoints.push_back(ImVec2(0, m_AnchorPP_INI));
        m_FitPoints.push_back(ImVec2((float)pm, m_AnchorPP_MID));
        m_FitPoints.push_back(ImVec2((float)pe, m_PwmMax));
        SortFitPoints();
    }
    m_OutputStr[0]=0;
}

void ThrustCurveEditor::LoadFromCurve() {
    m_RawPoints.clear(); m_FitPoints.clear(); if(!m_Curve)return;
    m_AnchorNP_INI=(float)m_Curve->np_ini.value; m_AnchorNP_MID=(float)m_Curve->np_mid.value;
    m_AnchorPP_INI=(float)m_Curve->pp_ini.value; m_AnchorPP_MID=(float)m_Curve->pp_mid.value;
    m_PwmMin=m_Curve->pwm_min; m_PwmMax=m_Curve->pwm_max; m_DefaultPwm=m_Curve->default_pwm;
    // Load raw user points
    size_t rn = m_Curve->raw_thrust.size();
    if (rn == m_Curve->raw_pwm.size() && rn > 0) {
        for (size_t i = 0; i < rn; ++i)
            m_RawPoints.push_back(ImVec2((float)m_Curve->raw_thrust[i], (float)m_Curve->raw_pwm[i]));
    }
    // Auto-detect PwmMin/PwmMax if both are zero
    if (m_PwmMin == 0.0f && m_PwmMax == 0.0f && !m_RawPoints.empty()) {
        m_PwmMin = m_RawPoints[0].y; m_PwmMax = m_PwmMin;
        for (auto& p : m_RawPoints) {
            float v = p.y;
            if (v < m_PwmMin) m_PwmMin = v;
            if (v > m_PwmMax) m_PwmMax = v;
        }
        float pad = (m_PwmMax - m_PwmMin) * 0.15f;
        if (pad < 50.0f) pad = 50.0f;
        m_PwmMin -= pad; m_PwmMax += pad;
        if (m_Curve) { m_Curve->pwm_min = m_PwmMin; m_Curve->pwm_max = m_PwmMax; }
    }
    // Dead zone: clamp to actual data range at x≈0
    double zLo = 1e30, zHi = -1e30;
    for(auto& pt : m_RawPoints){
        if(fabs((double)pt.x) < 0.001){
            if((double)pt.y < zLo) zLo = (double)pt.y;
            if((double)pt.y > zHi) zHi = (double)pt.y;
        }
    }
    if(zLo > zHi){ zLo = zHi = m_DefaultPwm; } // no point at x=0, use mid
    if(zHi < zLo) zHi = zLo;
    if (m_Curve->np_ini.value == 0.0) m_Curve->np_ini.value = zLo;
    if (m_Curve->pp_ini.value == 0.0) m_Curve->pp_ini.value = zHi;
    m_AnchorNP_INI = (float)m_Curve->np_ini.value;
    m_AnchorPP_INI = (float)m_Curve->pp_ini.value;

    // Restore fitted curve from saved params
    double ne=m_Curve->nt_end.value, nm=m_Curve->nt_mid.value, pm=m_Curve->pt_mid.value, pe=m_Curve->pt_end.value;
    if (fabs(ne)+fabs(nm)+fabs(pm)+fabs(pe) >= 0.001) {
        m_FitPoints.push_back(ImVec2((float)ne, m_PwmMin));
        m_FitPoints.push_back(ImVec2((float)nm, m_AnchorNP_MID));
        m_FitPoints.push_back(ImVec2(0, m_AnchorNP_INI));
        m_FitPoints.push_back(ImVec2(0, m_AnchorPP_INI));
        m_FitPoints.push_back(ImVec2((float)pm, m_AnchorPP_MID));
        m_FitPoints.push_back(ImVec2((float)pe, m_PwmMax));
        SortFitPoints();
    }
}

void ThrustCurveEditor::SaveRawPointsToCurve() {
    if (!m_Curve) return;
    m_Curve->raw_thrust.clear(); m_Curve->raw_pwm.clear();
    for (auto& p : m_RawPoints) {
        m_Curve->raw_thrust.push_back((double)p.x);
        m_Curve->raw_pwm.push_back(p.y);
    }
}

void ThrustCurveEditor::Fit() {
    if(!m_Curve||m_RawPoints.size()<2)return; SortRawPoints();

    // ===== Step 1: Determine dead zone from data at x≈0, centered on user mid =====
    float mid = m_DefaultPwm;
    double dataLo = 1e30, dataHi = -1e30;
    for(auto& pt : m_RawPoints){
        if(fabs((double)pt.x) < 0.001){
            if((double)pt.y < dataLo) dataLo = (double)pt.y;
            if((double)pt.y > dataHi) dataHi = (double)pt.y;
        }
    }
    // Half-width: enough to cover all data at x=0 on both sides, minimum 10
    double half = 10.0;
    if(dataLo <= dataHi){
        half = mid - dataLo;
        if(dataHi - mid > half) half = dataHi - mid;
        if(half < 10.0) half = 10.0;
    }
    float np_ini = mid - (float)half;
    float pp_ini = mid + (float)half;

    // ===== Step 2: Split raw points into negative / positive half-axes =====
    std::vector<ImVec2> negPts, posPts;
    for(auto& pt : m_RawPoints){
        if(pt.x <= 0.0f)  negPts.push_back(pt);
        if(pt.x >= 0.0f)  posPts.push_back(pt);
    }

    float pwmMinD = m_PwmMin, pwmMaxD = m_PwmMax;
    double nt_end = m_RawPoints.front().x;
    double pt_end = m_RawPoints.back().x;
    if(nt_end > -0.5) nt_end = -0.5;
    if(pt_end < 0.5)  pt_end = 0.5;

    double nt_mid, np_mid_d, pt_mid, pp_mid_d, neg_end_pwm, pos_end_pwm;

    // Compute data-driven PWM range per half-axis (not from user limits)
    float negDataLo = negPts.empty() ? 0.0f : negPts[0].y, negDataHi = negDataLo;
    for(auto& p : negPts){ float v=p.y; if(v<negDataLo)negDataLo=v; if(v>negDataHi)negDataHi=v; }
    if(negDataLo > np_ini) negDataLo = np_ini;
    if(negDataHi < np_ini) negDataHi = np_ini;
    float posDataLo = posPts.empty() ? 0.0f : posPts[0].y, posDataHi = posDataLo;
    for(auto& p : posPts){ float v=p.y; if(v<posDataLo)posDataLo=v; if(v>posDataHi)posDataHi=v; }
    if(posDataLo > pp_ini) posDataLo = pp_ini;
    if(posDataHi < pp_ini) posDataHi = pp_ini;

    // ===== Step 3: Fit negative side — 3 params: (nt_mid, np_mid, neg_end_pwm) =====
    //   (nt_end, neg_end_pwm) → (nt_mid, np_mid) → (0, np_ini)
    {
        float npIniD = np_ini;
        auto costNeg = [&](double tm, double pm, double endPwm){
            double e = 0;
            for(auto& pt : negPts){
                double th = (double)pt.x, pw = (double)pt.y;
                double pred;
                if(th <= tm) pred = endPwm + (th - nt_end) / (tm - nt_end) * (pm - endPwm);
                else         pred = pm + (th - tm) / (0.0 - tm) * (npIniD - pm);
                double d = pred - pw; e += d*d;
            }
            return e;
        };

        if(negPts.size() >= 2){
            double bestTm = nt_end * 0.5, bestPm = (negDataLo + npIniD) * 0.5, bestEp = negDataLo;
            double bestE = costNeg(bestTm, bestPm, bestEp);
            double margin = (negDataHi - negDataLo) * 0.15;
            for(int i = 0; i < 15; i++){
                double tm = nt_end + 0.01 + (-0.005 - nt_end - 0.01) * i / 14.0;
                for(int j = 0; j < 15; j++){
                    double pm = negDataLo + (npIniD - negDataLo) * (j + 1) / 16.0;
                    for(int k = 0; k < 12; k++){
                        double ep = negDataLo - margin + (npIniD - negDataLo + 2*margin) * k / 11.0;
                        double e = costNeg(tm, pm, ep);
                        if(e < bestE){ bestE = e; bestTm = tm; bestPm = pm; bestEp = ep; }
                    }
                }
            }
            nt_mid = bestTm; np_mid_d = bestPm; neg_end_pwm = bestEp;
        } else {
            nt_mid = nt_end * 0.5;
            np_mid_d = (negDataLo + npIniD) * 0.5;
            neg_end_pwm = negPts.empty() ? 0.0 : (double)negPts[0].y;
        }
    }

    // ===== Step 4: Fit positive side — 3 params: (pt_mid, pp_mid, pos_end_pwm) =====
    //   (0, pp_ini) → (pt_mid, pp_mid) → (pt_end, pos_end_pwm)
    {
        float ppIniD = pp_ini;
        auto costPos = [&](double tm, double pm, double endPwm){
            double e = 0;
            for(auto& pt : posPts){
                double th = (double)pt.x, pw = (double)pt.y;
                double pred;
                if(th <= tm) pred = ppIniD + (th - 0.0) / (tm - 0.0) * (pm - ppIniD);
                else         pred = pm + (th - tm) / (pt_end - tm) * (endPwm - pm);
                double d = pred - pw; e += d*d;
            }
            return e;
        };

        if(posPts.size() >= 2){
            double bestTm = pt_end * 0.5, bestPm = (ppIniD + posDataHi) * 0.5, bestEp = posDataHi;
            double bestE = costPos(bestTm, bestPm, bestEp);
            double margin = (posDataHi - posDataLo) * 0.15;
            for(int i = 0; i < 15; i++){
                double tm = 0.005 + (pt_end - 0.01 - 0.005) * i / 14.0;
                for(int j = 0; j < 15; j++){
                    double pm = ppIniD + (posDataHi - ppIniD) * (j + 1) / 16.0;
                    for(int k = 0; k < 12; k++){
                        double ep = ppIniD - margin + (posDataHi - ppIniD + 2*margin) * k / 11.0;
                        double e = costPos(tm, pm, ep);
                        if(e < bestE){ bestE = e; bestTm = tm; bestPm = pm; bestEp = ep; }
                    }
                }
            }
            pt_mid = bestTm; pp_mid_d = bestPm; pos_end_pwm = bestEp;
        } else {
            pt_mid = pt_end * 0.5;
            pp_mid_d = (ppIniD + posDataHi) * 0.5;
            pos_end_pwm = posPts.empty() ? 0.0 : (double)posPts.back().y;
        }
    }

    // ===== Step 5: Write back to curve & anchors =====
    m_Curve->nt_end.value  = nt_end;
    m_Curve->nt_mid.value  = nt_mid;
    m_Curve->pt_mid.value  = pt_mid;
    m_Curve->pt_end.value  = pt_end;
    m_Curve->np_ini.value  = np_ini;
    m_Curve->np_mid.value  = np_mid_d;
    m_Curve->pp_ini.value  = pp_ini;
    m_Curve->pp_mid.value  = pp_mid_d;
    m_Curve->pwm_min = pwmMinD;
    m_Curve->pwm_max = pwmMaxD;

    m_AnchorNP_INI = np_ini;   m_AnchorNP_MID = (float)np_mid_d;
    m_AnchorPP_INI = pp_ini;   m_AnchorPP_MID = (float)pp_mid_d;

    m_FitPoints.clear();
    m_FitPoints.push_back(ImVec2((float)nt_end, (float)neg_end_pwm));
    m_FitPoints.push_back(ImVec2((float)nt_mid, (float)np_mid_d));
    m_FitPoints.push_back(ImVec2(0, np_ini));
    m_FitPoints.push_back(ImVec2(0, pp_ini));
    m_FitPoints.push_back(ImVec2((float)pt_mid, (float)pp_mid_d));
    m_FitPoints.push_back(ImVec2((float)pt_end, (float)pos_end_pwm));

    SortFitPoints(); m_SelectedIdx = -1; m_EditPopupOpen = false;

    // Generate formatted output string
    _snprintf_s(m_OutputStr, sizeof(m_OutputStr),
        "%.1f, %.1f, %.1f, %.1f,  %.3f, %.3f, %.3f, %.3f,  %.1f, %.1f",
        m_Curve->np_ini.value, m_Curve->np_mid.value,
        m_Curve->pp_ini.value, m_Curve->pp_mid.value,
        m_Curve->nt_end.value, m_Curve->nt_mid.value,
        m_Curve->pt_mid.value, m_Curve->pt_end.value,
        m_Curve->pwm_min, m_Curve->pwm_max);
}

void ThrustCurveEditor::SortRawPoints(){std::stable_sort(m_RawPoints.begin(),m_RawPoints.end(),[](const ImVec2& a,const ImVec2& b){return a.x<b.x;});}
void ThrustCurveEditor::SortFitPoints(){std::sort(m_FitPoints.begin(),m_FitPoints.end(),[](const ImVec2& a,const ImVec2& b){if(a.x!=b.x)return a.x<b.x;return a.y<b.y;});}

void ThrustCurveEditor::DrawPlot() {
    if(!ImPlot::BeginPlot("##CurvePlot",ImVec2(-1,-1),
        ImPlotFlags_NoTitle|ImPlotFlags_NoLegend|ImPlotFlags_NoMenus|ImPlotFlags_NoBoxSelect|ImPlotFlags_Crosshairs)) return;
    ImPlot::SetupAxis(ImAxis_X1,"Thrust (norm.)",0);
    ImPlot::SetupAxis(ImAxis_Y1,"PWM (compare)",0);
    // Auto-fit Y to PwmMin/PwmMax with 10% padding
    double yPad = (m_PwmMax - m_PwmMin) * 0.1;
    if (yPad < 10.0) yPad = 10.0;
    ImPlot::SetupAxisLimits(ImAxis_Y1, m_PwmMin - yPad, m_PwmMax + yPad);

    // Fitted polyline — 6 control points connected in order (sorted by x)
    if(m_FitPoints.size()>=6){
        int nf = (int)m_FitPoints.size();
        std::vector<float> fx(nf), fy(nf);
        for(int i=0;i<nf;++i){fx[i]=m_FitPoints[i].x;fy[i]=m_FitPoints[i].y;}
        ImPlot::SetNextLineStyle(ImVec4(1.0f,0.55f,0.0f,1.0f),3.0f);
        ImPlot::PlotLine("##Fitted",fx.data(),fy.data(),nf);
    }

    // Raw scatter
    int n=(int)m_RawPoints.size();
    if(n>0){
        std::vector<float> rtx(n),rty(n);
        for(int i=0;i<n;++i){rtx[i]=m_RawPoints[i].x;rty[i]=m_RawPoints[i].y;}
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle,5.0f,ImVec4(0.3f,0.6f,1.0f,0.8f));
        ImPlot::PlotScatter("##Raw",rtx.data(),rty.data(),n);
        // Highlight selected point
        if(m_SelectedIdx>=0 && m_SelectedIdx<n){
            float sx=m_RawPoints[m_SelectedIdx].x, sy=m_RawPoints[m_SelectedIdx].y;
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle,9.0f,ImVec4(0.0f,1.0f,0.3f,0.6f));
            ImPlot::PlotScatter("##Sel",&sx,&sy,1);
        }
    }

    // PWM limits & Mid (mid is user-controlled)
    double pmax=m_PwmMax,pmin=m_PwmMin, pmid=m_DefaultPwm;
    ImPlot::DragLineY(0,&pmax,ImVec4(1.0f,0.3f,0.3f,0.8f),2.0f,ImPlotDragToolFlags_Delayed);
    ImPlot::DragLineY(1,&pmin,ImVec4(0.3f,0.5f,1.0f,0.8f),2.0f,ImPlotDragToolFlags_Delayed);
    ImPlot::DragLineY(2,&pmid,ImVec4(0.3f,1.0f,0.3f,0.8f),2.0f,ImPlotDragToolFlags_Delayed);
    m_PwmMax=(float)pmax;m_PwmMin=(float)pmin;m_DefaultPwm=(float)pmid;
    if(m_PwmMin>=m_PwmMax-1.0f)m_PwmMin=m_PwmMax-1.0f;if(m_PwmMax<=m_PwmMin+1.0f)m_PwmMax=m_PwmMin+1.0f;
    ImPlot::TagY(m_DefaultPwm,ImVec4(0.3f,1.0f,0.3f,0.6f),"mid");

    // Dead zone reference points (np_ini/pp_ini — visible on plot, computed by Fit from mid + data)
    ImPlot::TagY(m_AnchorNP_INI,ImVec4(1,0.7f,0.2f,0.5f),"np_ini");
    ImPlot::TagY(m_AnchorPP_INI,ImVec4(1,0.7f,0.2f,0.5f),"pp_ini");
    {
        double dx=0.0, dnp=m_AnchorNP_INI, dpp=m_AnchorPP_INI;
        ImPlot::DragPoint(1001,&dx,&dnp,ImVec4(1.0f,0.55f,0.0f,0.7f),5.0f,ImPlotDragToolFlags_NoInputs);
        dx=0.0;
        ImPlot::DragPoint(1002,&dx,&dpp,ImVec4(1.0f,0.55f,0.0f,0.7f),5.0f,ImPlotDragToolFlags_NoInputs);
    }

    // DragPoint for each raw point
    for(int i=0;i<(int)m_RawPoints.size();++i){
        double dx=(double)m_RawPoints[i].x, dy=(double)m_RawPoints[i].y;
        if(ImPlot::DragPoint(i,&dx,&dy,ImVec4(1,1,0,1),4.0f,ImPlotDragToolFlags_Delayed)){
            m_RawPoints[i].x=(float)dx; m_RawPoints[i].y=(float)dy; m_SelectedIdx=i; SaveRawPointsToCurve();
        }
    }

    // Left-click on point to select it (for table sync)
    if(ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyCtrl){
        ImPlotPoint mp=ImPlot::GetPlotMousePos();
        // Use relative distance (5% of visible range)
        ImPlotRect lim = ImPlot::GetPlotLimits();
        double xRange = lim.X.Max - lim.X.Min, yRange = lim.Y.Max - lim.Y.Min;
        double xThresh = xRange * 0.03, yThresh = yRange * 0.03;
        int best=-1; double bd=1e30;
        for(int i=0;i<(int)m_RawPoints.size();++i){
            double dx=(m_RawPoints[i].x-(float)mp.x)/xThresh;
            double dy=(m_RawPoints[i].y-(float)mp.y)/yThresh;
            double d2=dx*dx+dy*dy; if(d2<bd){bd=d2; best=i;}
        }
        if(best>=0 && bd<1.0) m_SelectedIdx=best;
    }

    ImPlot::EndPlot();
}

void ThrustCurveEditor::DrawPointTable(){
    ImGui::BeginChild("##PtTable",ImVec2(0,0),true);

    // PwmMin / PwmMax / Mid  (constraints use live members to avoid stale local copies)
    ImGui::PushItemWidth(80);
    float pmin=m_PwmMin, pmax=m_PwmMax, pmid=m_DefaultPwm;
    if(ImGui::DragFloat("Min##PWM",&pmin,1.0f,0,0,"%.1f")){
        if(pmin<m_PwmMax-1.0f) m_PwmMin=pmin;
    }
    ImGui::SameLine();
    if(ImGui::DragFloat("Max##PWM",&pmax,1.0f,0,0,"%.1f")){
        if(pmax>m_PwmMin+1.0f) m_PwmMax=pmax;
    }
    ImGui::SameLine();
    ImGui::DragFloat("Mid##PWM",&pmid,1.0f,0,0,"%.1f");
    m_DefaultPwm=pmid;
    ImGui::PopItemWidth();
    ImGui::Separator();

    ImGui::Text("%zu data points",m_RawPoints.size());
    ImGui::Separator();

    if(ImGui::BeginTable("##PtTbl",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,ImVec2(0,ImGui::GetContentRegionAvail().y-28))){
        ImGui::TableSetupColumn("#",ImGuiTableColumnFlags_WidthFixed,24);
        ImGui::TableSetupColumn("Thrust",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("PWM",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int delIdx=-1;
        for(int i=0;i<(int)m_RawPoints.size();++i){
            char lblT[32], lblP[32];
            _snprintf_s(lblT,sizeof(lblT),"##T%d",i);
            _snprintf_s(lblP,sizeof(lblP),"##P%d",i);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool isSel = (i == m_SelectedIdx);
            if (isSel) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(60, 120, 60, 100));
            }
            ImGui::Selectable(std::to_string(i).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);
            if (ImGui::IsItemClicked()) m_SelectedIdx = i;
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1);
            float t=m_RawPoints[i].x;
            if(ImGui::DragFloat(lblT,&t,0.005f,-100,100,"%.3f")){
                m_RawPoints[i].x=t; SaveRawPointsToCurve();
            }
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-1);
            float d=m_RawPoints[i].y;
            if(ImGui::DragFloat(lblP,&d,1.0f,0,0,"%.1f")){
                m_RawPoints[i].y=d; SaveRawPointsToCurve();
            }
            if(ImGui::IsItemHovered()&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) m_SelectedIdx=i;
        }
        ImGui::EndTable();
    }

    if(ImGui::Button("Add Point")){
        m_RawPoints.push_back(ImVec2(0,m_DefaultPwm)); SortRawPoints(); m_SelectedIdx=-1; SaveRawPointsToCurve();
    }
    ImGui::SameLine();
    if(ImGui::Button("Del Selected") && m_SelectedIdx>=0 && m_SelectedIdx<(int)m_RawPoints.size()){
        m_RawPoints.erase(m_RawPoints.begin()+m_SelectedIdx); m_SelectedIdx=-1; SaveRawPointsToCurve();
    }
    ImGui::SameLine();
    if(ImGui::Button("Clear All")){m_RawPoints.clear(); m_FitPoints.clear(); m_SelectedIdx=-1; m_EditPopupOpen=false; SaveRawPointsToCurve();}

    ImGui::EndChild();
}

void ThrustCurveEditor::Draw(){
    if(!m_Open)return;
    WL_TRACE_TAG("CURVE", "Draw: begin ({} raw pts, {} fit pts)", m_RawPoints.size(), m_FitPoints.size());
    ImGui::SetNextWindowSize(ImVec2(900,550),ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Thrust Curve Editor",&m_Open)){ImGui::End();return;}
    char t[192];_snprintf_s(t,sizeof(t),"Motor: %s  |  %zu pts",m_MotorName,m_RawPoints.size());
    ImGui::TextUnformatted(t);
    ImGui::SameLine(); if(ImGui::SmallButton("Fit")){SortRawPoints();Fit();m_EditPopupOpen=false;}
    ImGui::SameLine(); if(ImGui::SmallButton("Reset")){m_RawPoints.clear();m_FitPoints.clear();m_SelectedIdx=-1;m_EditPopupOpen=false;SaveRawPointsToCurve();}
    ImGui::SameLine(); if(ImGui::SmallButton("Save")){Save();}
    ImGui::SameLine(); if(ImGui::SmallButton("Close")){Revert();m_Open=false;}

    // Show formatted output string with copy button
    if(m_OutputStr[0]){
        ImGui::SameLine();
        ImGui::TextDisabled("| Output:");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_FrameBg,IM_COL32(30,30,35,255));
        ImGui::InputText("##Out",m_OutputStr,sizeof(m_OutputStr),ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if(ImGui::SmallButton("Copy")) ImGui::SetClipboardText(m_OutputStr);
    }

    ImGui::Separator();

    WL_TRACE_TAG("CURVE", "Draw: entering plot");

    // Draggable horizontal splitter between plot and table
    static float s_SplitPos = -1.0f;
    const float kSplitterWidth = 6.0f;
    const float kMinPaneW = 120.0f;
    float availW = ImGui::GetContentRegionAvail().x;
    if (s_SplitPos < 0) s_SplitPos = availW * 0.65f;
    // Clamp
    if (s_SplitPos < kMinPaneW) s_SplitPos = kMinPaneW;
    if (s_SplitPos > availW - kMinPaneW - kSplitterWidth) s_SplitPos = availW - kMinPaneW - kSplitterWidth;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    // Left: plot pane
    WL_TRACE_TAG("CURVE", "Draw: entering DrawPlot");
    ImGui::BeginChild("##PlotPane", ImVec2(s_SplitPos, 0), true);
    DrawPlot();
    ImGui::EndChild();
    WL_TRACE_TAG("CURVE", "Draw: DrawPlot OK");

    ImGui::SameLine(0.0f, 0.0f);

    // Splitter handle
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.35f, 0.38f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.50f, 0.55f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.55f, 0.60f, 0.9f));
    ImGui::Button("##Splitter", ImVec2(kSplitterWidth, -1));
    if (ImGui::IsItemActive()) {
        s_SplitPos += ImGui::GetIO().MouseDelta.x;
        if (s_SplitPos < kMinPaneW) s_SplitPos = kMinPaneW;
        if (s_SplitPos > availW - kMinPaneW - kSplitterWidth) s_SplitPos = availW - kMinPaneW - kSplitterWidth;
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, 0.0f);

    // Right: table pane
    float tableW = availW - s_SplitPos - kSplitterWidth;
    WL_TRACE_TAG("CURVE", "Draw: entering DrawPointTable");
    ImGui::BeginChild("##TablePane", ImVec2(tableW, 0), true);
    DrawPointTable();
    ImGui::EndChild();
    WL_TRACE_TAG("CURVE", "Draw: DrawPointTable OK");

    ImGui::PopStyleVar();

    ImGui::End();
}
