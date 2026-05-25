#include "ThrustCurveEditor.h"
#include <cstdio>
#include <cmath>

void ThrustCurveEditor::Open(const char* n, ThrustCurve& c) {
    _snprintf_s(m_MotorName,sizeof(m_MotorName),"%s",n); m_Curve=&c; m_Standalone=false;
    LoadFromCurve(); m_SelectedIdx=-1; m_EditPopupOpen=false; m_Open=true;
}

void ThrustCurveEditor::OpenStandalone(const char* n) {
    _snprintf_s(m_MotorName,sizeof(m_MotorName),"%s",n);
    m_OwnCurve=ThrustCurve(); m_Curve=&m_OwnCurve; m_Standalone=true;
    LoadFromCurve(); m_SelectedIdx=-1; m_EditPopupOpen=false; m_Open=true; m_OutputStr[0]=0;
}

void ThrustCurveEditor::LoadFromCurve() {
    m_RawPoints.clear(); m_FitPoints.clear(); if(!m_Curve)return;
    m_AnchorNP_INI=(float)m_Curve->np_ini.value; m_AnchorNP_MID=(float)m_Curve->np_mid.value;
    m_AnchorPP_INI=(float)m_Curve->pp_ini.value; m_AnchorPP_MID=(float)m_Curve->pp_mid.value;
    double ne=m_Curve->nt_end.value, nm=m_Curve->nt_mid.value, pm=m_Curve->pt_mid.value, pe=m_Curve->pt_end.value;
    // Don't load all-zero default curve
    if (fabs(ne)+fabs(nm)+fabs(pm)+fabs(pe) < 0.001) return;
    m_FitPoints.push_back(ImVec2((float)ne, m_AnchorNP_INI));
    m_FitPoints.push_back(ImVec2((float)nm, m_AnchorNP_MID));
    m_FitPoints.push_back(ImVec2(0, (float)m_Curve->np_ini.value));
    m_FitPoints.push_back(ImVec2((float)pm, m_AnchorPP_INI));
    m_FitPoints.push_back(ImVec2((float)pe, m_AnchorPP_MID));
    SortFitPoints();
}

void ThrustCurveEditor::FitAndApply() {
    if(!m_Curve||m_RawPoints.size()<2)return; SortRawPoints();
    // Sample raw points at 200 Thrust positions
    double mn=m_RawPoints.front().x, mx=m_RawPoints.back().x;
    if(mn>-0.5)mn=-0.5; if(mx<0.5)mx=0.5;
    std::vector<ImVec2> smp(200);
    for(int i=0;i<200;++i){double th=mn+(mx-mn)*i/199.0;smp[i]=ImVec2((float)th,(float)SamplePWM(m_RawPoints,th));}

    double p[10];
    p[0]=mn; p[1]=-0.1; p[2]=0.1; p[3]=mx; // nt_end, nt_mid, pt_mid, pt_end
    p[4]=(double)SamplePWM(m_RawPoints,0.0); // np_ini
    p[5]=(double)SamplePWM(m_RawPoints,-0.5); // np_mid
    p[6]=(double)SamplePWM(m_RawPoints,0.0);  // pp_ini
    p[7]=(double)SamplePWM(m_RawPoints,0.5);  // pp_mid
    p[8]=(double)m_PwmMin; p[9]=(double)m_PwmMax;

    auto Cost=[&](const double* q){double e=0;for(auto& a:smp)e+=(EvalModel(a.x,q)-a.y)*(EvalModel(a.x,q)-a.y);return e;};
    double be=Cost(p);
    double st[8]={0.015,0.015,0.015,0.015,0.04,0.04,0.04,0.04};

    for(int iter=0;iter<120;++iter){
        double decay=1.0-(double)iter/120.0; if(decay<0.04)decay=0.04;
        bool imp=false;
        for(int j=0;j<8;++j){
            double o=p[j],ss=st[j]*decay; if(ss<0.0005)ss=0.0005;
            p[j]=o+ss;
            if(j==0&&p[0]>p[1]-0.005)p[0]=p[1]-0.005;
            if(j==1){if(p[1]<p[0]+0.005)p[1]=p[0]+0.005;if(p[1]>-0.005)p[1]=-0.005;}
            if(j==2){if(p[2]<0.005)p[2]=0.005;if(p[2]>p[3]-0.005)p[2]=p[3]-0.005;}
            if(j==3&&p[3]<p[2]+0.005)p[3]=p[2]+0.005;
            if((j==4||j==5)&&p[j]>-0.005)p[j]=-0.005;
            if((j==6||j==7)&&p[j]<0.005)p[j]=0.005;
            double ep=Cost(p);
            p[j]=o-ss;
            if(j==0&&p[0]>p[1]-0.005)p[0]=p[1]-0.005;
            if(j==1){if(p[1]<p[0]+0.005)p[1]=p[0]+0.005;if(p[1]>-0.005)p[1]=-0.005;}
            if(j==2){if(p[2]<0.005)p[2]=0.005;if(p[2]>p[3]-0.005)p[2]=p[3]-0.005;}
            if(j==3&&p[3]<p[2]+0.005)p[3]=p[2]+0.005;
            if((j==4||j==5)&&p[j]>-0.005)p[j]=-0.005;
            if((j==6||j==7)&&p[j]<0.005)p[j]=0.005;
            double em=Cost(p);
            if(ep<be&&ep<=em){p[j]=o+ss;be=ep;imp=true;}
            else if(em<be&&em<=ep){p[j]=o-ss;be=em;imp=true;}
            else p[j]=o;
        }
        if(!imp)break;
    }

    m_Curve->nt_end.value=p[0];m_Curve->nt_mid.value=p[1];
    m_Curve->pt_mid.value=p[2];m_Curve->pt_end.value=p[3];
    m_Curve->np_ini.value=p[4];m_Curve->np_mid.value=p[5];
    m_Curve->pp_ini.value=p[6];m_Curve->pp_mid.value=p[7];

    m_FitPoints.clear();
    m_FitPoints.push_back(ImVec2((float)p[0],(float)p[8]));
    m_FitPoints.push_back(ImVec2((float)p[1],(float)p[5]));
    m_FitPoints.push_back(ImVec2(0,(float)p[4]));
    m_FitPoints.push_back(ImVec2((float)p[2],(float)p[7]));
    m_FitPoints.push_back(ImVec2((float)p[3],(float)p[9]));

    m_AnchorNP_INI=(float)p[4];m_AnchorNP_MID=(float)p[5];
    m_AnchorPP_INI=(float)p[6];m_AnchorPP_MID=(float)p[7];
    SortFitPoints();m_SelectedIdx=-1;m_EditPopupOpen=false;

    // Generate formatted output string
    _snprintf_s(m_OutputStr,sizeof(m_OutputStr),
        "%.3f, %.3f, %.3f, %.3f,  %.3f, %.3f, %.3f, %.3f",
        m_Curve->np_ini.value,m_Curve->np_mid.value,
        m_Curve->pp_ini.value,m_Curve->pp_mid.value,
        m_Curve->nt_end.value,m_Curve->nt_mid.value,
        m_Curve->pt_mid.value,m_Curve->pt_end.value);
}

void ThrustCurveEditor::SortRawPoints(){std::sort(m_RawPoints.begin(),m_RawPoints.end(),[](const ImVec2& a,const ImVec2& b){return a.x<b.x;});}
void ThrustCurveEditor::SortFitPoints(){std::sort(m_FitPoints.begin(),m_FitPoints.end(),[](const ImVec2& a,const ImVec2& b){return a.x<b.x;});}

void ThrustCurveEditor::DrawPlot() {
    if(!ImPlot::BeginPlot("##CurvePlot",ImVec2(-1,-1),
        ImPlotFlags_NoTitle|ImPlotFlags_NoLegend|ImPlotFlags_NoMenus|ImPlotFlags_NoBoxSelect|ImPlotFlags_Crosshairs)) return;
    ImPlot::SetupAxis(ImAxis_X1,"Thrust (norm.)",0);
    ImPlot::SetupAxis(ImAxis_Y1,"PWM Duty (norm.)",0);
    ImPlot::SetupAxisLimits(ImAxis_Y1,-1.0f,1.0f);
    int n=(int)m_RawPoints.size();

    // Fitted polyline
    if(m_FitPoints.size()>=5){
        double mn=m_FitPoints.front().x,mx=m_FitPoints.back().x;
        if(mn>-0.5)mn=-0.5; if(mx<0.5)mx=0.5;
        double mp[10];
        mp[0]=m_FitPoints[0].x;mp[1]=m_FitPoints[1].x;mp[2]=m_FitPoints[3].x;mp[3]=m_FitPoints[4].x;
        mp[4]=m_FitPoints[2].y;mp[5]=m_FitPoints[1].y;mp[6]=m_FitPoints[2].y;mp[7]=m_FitPoints[3].y;
        mp[8]=m_FitPoints[0].y;mp[9]=m_FitPoints[4].y;
        std::vector<float> tx(300),ty(300);
        for(int i=0;i<300;++i){double th=mn+(mx-mn)*i/299.0;tx[i]=(float)th;ty[i]=(float)EvalModel(th,mp);}
        ImPlot::SetNextLineStyle(ImVec4(1.0f,0.55f,0.0f,1.0f),3.0f);
        ImPlot::PlotLine("##Fitted",tx.data(),ty.data(),300);
    }

    // Raw scatter
    if(n>0){
        std::vector<float> rtx(n),rty(n);
        for(int i=0;i<n;++i){rtx[i]=m_RawPoints[i].x;rty[i]=m_RawPoints[i].y;}
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle,5.0f,ImVec4(0.3f,0.6f,1.0f,0.8f));
        ImPlot::PlotScatter("##Raw",rtx.data(),rty.data(),n);
        for(int i=0;i<n;++i){
            double dx=(double)rtx[i],dy=(double)rty[i];
            if(ImPlot::DragPoint(i,&dx,&dy,ImVec4(1,1,0,1),7.0f,ImPlotDragToolFlags_Delayed)){
                m_RawPoints[i].x=(float)dx;m_RawPoints[i].y=(float)dy;m_SelectedIdx=i;
                SortRawPoints();
                for(int k=0;k<n;++k)if(m_RawPoints[k].x==(float)dx&&m_RawPoints[k].y==(float)dy){m_SelectedIdx=k;break;}
            }
        }
    }

    // PWM limits
    double pmax=(double)m_PwmMax,pmin=(double)m_PwmMin;
    ImPlot::DragLineY(0,&pmax,ImVec4(1.0f,0.3f,0.3f,0.8f),2.0f,ImPlotDragToolFlags_Delayed);
    ImPlot::DragLineY(1,&pmin,ImVec4(0.3f,0.5f,1.0f,0.8f),2.0f,ImPlotDragToolFlags_Delayed);
    m_PwmMax=(float)pmax;m_PwmMin=(float)pmin;
    if(m_PwmMin>=m_PwmMax-0.01f)m_PwmMin=m_PwmMax-0.01f;if(m_PwmMax<=m_PwmMin+0.01f)m_PwmMax=m_PwmMin+0.01f;

    // Anchors
    ImPlot::TagY((double)m_AnchorNP_INI,ImVec4(1,0.7f,0.2f,0.5f),"np_ini");
    ImPlot::TagY((double)m_AnchorNP_MID,ImVec4(1,0.7f,0.2f,0.5f),"np_mid");
    ImPlot::TagY((double)m_AnchorPP_INI,ImVec4(1,0.7f,0.2f,0.5f),"pp_ini");
    ImPlot::TagY((double)m_AnchorPP_MID,ImVec4(1,0.7f,0.2f,0.5f),"pp_mid");

    // Click add
    if(ImPlot::IsPlotHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&
       !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)&&!ImGui::GetIO().KeyCtrl)m_PendingAdd=true;
    if(m_PendingAdd&&ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
        ImPlotPoint pt=ImPlot::GetPlotMousePos();
        bool near=false;for(int i=0;i<n;++i)if(powf(m_RawPoints[i].x-(float)pt.x,2)+powf(m_RawPoints[i].y-(float)pt.y,2)<0.002f)near=true;
        if(!near){m_RawPoints.push_back(ImVec2((float)pt.x,(float)pt.y));SortRawPoints();m_SelectedIdx=-1;m_EditPopupOpen=false;}m_PendingAdd=false;
    }
    // Right delete
    if(ImPlot::IsPlotHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Right)){m_PendingAdd=false;
        ImPlotPoint mp=ImPlot::GetPlotMousePos();int best=-1;float bd=0.008f;
        for(int i=0;i<n;++i){float dx=m_RawPoints[i].x-(float)mp.x,dy=m_RawPoints[i].y-(float)mp.y;if(dx*dx+dy*dy<bd){bd=dx*dx+dy*dy;best=i;}}
        if(best>=0){m_RawPoints.erase(m_RawPoints.begin()+best);if(m_SelectedIdx==best)m_SelectedIdx=-1;else if(m_SelectedIdx>best)m_SelectedIdx--;}
    }
    // Double click
    if(ImPlot::IsPlotHovered()&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){m_PendingAdd=false;
        ImPlotPoint mp=ImPlot::GetPlotMousePos();
        for(int i=0;i<n;++i){float dx=m_RawPoints[i].x-(float)mp.x,dy=m_RawPoints[i].y-(float)mp.y;if(dx*dx+dy*dy<0.008f){m_SelectedIdx=i;m_EditPopupOpen=true;break;}}
    }
    ImPlot::EndPlot();
}

void ThrustCurveEditor::DrawPopupEditor(){
    if(!m_EditPopupOpen||m_SelectedIdx<0||m_SelectedIdx>=(int)m_RawPoints.size())return;
    ImGui::SetNextWindowSize(ImVec2(180,42),ImGuiCond_Always);
    char t[64];_snprintf_s(t,sizeof(t),"Point #%d###PE",m_SelectedIdx);
    if(!ImGui::Begin(t,&m_EditPopupOpen,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize)){ImGui::End();return;}
    float th=m_RawPoints[m_SelectedIdx].x,du=m_RawPoints[m_SelectedIdx].y;
    ImGui::PushItemWidth(68);
    ImGui::DragFloat("T",&th,0.005f,-100,100,"%.3f");ImGui::SameLine();ImGui::DragFloat("Duty",&du,0.005f,-100,100,"%.3f");ImGui::PopItemWidth();
    if(th!=m_RawPoints[m_SelectedIdx].x||du!=m_RawPoints[m_SelectedIdx].y){m_RawPoints[m_SelectedIdx].x=th;m_RawPoints[m_SelectedIdx].y=du;
        SortRawPoints();for(size_t i=0;i<m_RawPoints.size();++i)if(m_RawPoints[i].x==th&&m_RawPoints[i].y==du){m_SelectedIdx=(int)i;break;}}ImGui::End();
}

void ThrustCurveEditor::Draw(){
    if(!m_Open)return;ImGui::SetNextWindowSize(ImVec2(700,550),ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Thrust Curve Editor",&m_Open,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)){ImGui::End();return;}
    char t[192];_snprintf_s(t,sizeof(t),"Motor: %s  |  %zu pts%s",m_MotorName,m_RawPoints.size(),m_Standalone?" (standalone)":"");
    ImGui::TextUnformatted(t);
    ImGui::SameLine();ImGui::PushItemWidth(52);
    ImGui::DragFloat("np_i",&m_AnchorNP_INI,0.005f,-1,0,"%.2f");ImGui::SameLine();ImGui::DragFloat("np_m",&m_AnchorNP_MID,0.005f,-1,0,"%.2f");
    ImGui::SameLine();ImGui::DragFloat("pp_i",&m_AnchorPP_INI,0.005f,0,1,"%.2f");ImGui::SameLine();ImGui::DragFloat("pp_m",&m_AnchorPP_MID,0.005f,0,1,"%.2f");ImGui::PopItemWidth();
    ImGui::SameLine();if(ImGui::SmallButton("Fit")){SortRawPoints();FitAndApply();m_EditPopupOpen=false;}
    ImGui::SameLine();if(ImGui::SmallButton("Reset")){m_RawPoints.clear();m_FitPoints.clear();m_SelectedIdx=-1;m_EditPopupOpen=false;}
    ImGui::SameLine();if(ImGui::SmallButton("Done"))m_Open=false;

    // Standalone: show formatted output string with copy button
    if(m_Standalone && m_OutputStr[0]){
        ImGui::SameLine();
        ImGui::TextDisabled("| Output:");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_FrameBg,IM_COL32(30,30,35,255));
        ImGui::InputText("##Out",m_OutputStr,sizeof(m_OutputStr),ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if(ImGui::SmallButton("Copy")) ImGui::SetClipboardText(m_OutputStr);
    }

    ImGui::Separator();DrawPlot();DrawPopupEditor();ImGui::End();
}
