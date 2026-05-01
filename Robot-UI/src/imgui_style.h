#pragma once

#include "imgui.h"
#include "spectrum/imgui_spectrum.h"
#include "IconFontCppHeaders/IconsFontAwesome4.h" 

enum class ImGuiTheme {
	WalnutDefault,  // Walnut 默认风格
	Cinder,         // 灰烬风格
	AdobeSpectrum,  // Adobe 规范风格
	EnemyMouse,     // 敌鼠风格
	LedSynthmaster, //LED 合成器风格
	Dougblinks,     // 道格·宾克斯自适应风格
	Classic,         // 经典风格
	Dark,
	Light
};

class ImGuiStyleManager
{
private:
	ImGuiTheme m_Theme = ImGuiTheme::WalnutDefault;
	ImGuiStyle& mStyle;
	ImGuiIO& mIO;

	const char* themeNames[9] = { "Walnut Default", "Cinder", "Adobe Spectrum", "Enemy Mouse",
								 "Led Synthmaster", "Dougblinks", "Classic",
								 "Dark", "Light" };


public:
	ImGuiTheme GetTheme() const { return m_Theme; }
	bool GetInvert() const { return m_bStyleInvert; }
	float GetAlpha() const { return m_Alpha; }

	ImGuiStyleManager& WalnutDefaultTheme();
	ImGuiStyleManager& CinderTheme();
	ImGuiStyleManager& SpectrumTheme();
	ImGuiStyleManager& EnemymouseTheme();
	ImGuiStyleManager& LedSynthmasterTheme();
	ImGuiStyleManager& DougblinksTheme();
	ImGuiStyleManager& ClassicTheme();
	ImGuiStyleManager& DarkTheme();
	ImGuiStyleManager& LightTheme();

	void DrawStylePanel();
	void ApplyImGuiStyle(ImGuiTheme theme, bool invert_mode = false, float alpha = 1.0f);
	void SetStyleColors(ImGuiTheme theme, bool invert_mode = false, float alpha = 1.0f);
	void ApplyActiveStyle();
	void RevertStyle();

	ImGuiStyleManager();
	~ImGuiStyleManager();

private:
	ImGuiTheme m_ActiveTheme = ImGuiTheme::WalnutDefault;
	bool m_bStyleInvert = false;
	float m_Alpha = 1.0f;
	bool m_ActiveBStyleInvert = false;
	float m_ActiveAlpha = 1.0f;
	ImGuiStyle m_DefaultStyle;
	float m_PreviewTimer = 0.0f;
};
