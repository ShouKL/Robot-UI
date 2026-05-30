#pragma once

#include "imgui.h"

#include "spectrum/imgui_spectrum.h"

#include "IconFontCppHeaders/IconsFontAwesome4.h"

enum class ImGuiTheme {

	WalnutDefault,  // Walnut

	Cinder,         //

	AdobeSpectrum,  // Adobe

	EnemyMouse,     //

	LedSynthmaster, //LED

	Dougblinks,     //

	Classic,         //

	Dark,

	Light

};

class ImGuiStyleManager

{

private:

	// ======== 主题数据 ========
	ImGuiTheme m_Theme = ImGuiTheme::WalnutDefault;

	ImGuiStyle& mStyle;

	ImGuiIO& mIO;

	const char* themeNames[9] = { "Walnut Default", "Cinder", "Adobe Spectrum", "Enemy Mouse",

								 "Led Synthmaster", "Dougblinks", "Classic",

								 "Dark", "Light" };

public:

	// ======== 主题查询 ========
	ImGuiTheme GetTheme() const { return m_Theme; }

	bool GetInvert() const { return m_bStyleInvert; }

	float GetAlpha() const { return m_Alpha; }

	// ======== 主题定义 ========
	ImGuiStyleManager& WalnutDefaultTheme();

	ImGuiStyleManager& CinderTheme();

	ImGuiStyleManager& SpectrumTheme();

	ImGuiStyleManager& EnemymouseTheme();

	ImGuiStyleManager& LedSynthmasterTheme();

	ImGuiStyleManager& DougblinksTheme();

	ImGuiStyleManager& ClassicTheme();

	ImGuiStyleManager& DarkTheme();

	ImGuiStyleManager& LightTheme();

	// ======== 主题切换与样式应用 ========
	void DrawStylePanel();

	void ApplyImGuiStyle(ImGuiTheme theme, bool invert_mode = false, float alpha = 1.0f);

	void SetStyleColors(ImGuiTheme theme, bool invert_mode = false, float alpha = 1.0f);

	void ApplyActiveStyle();

	void RevertStyle();

	// ======== 构造/析构 ========
	ImGuiStyleManager();

	~ImGuiStyleManager();

private:
	// ======== 运行时状态 ========
	ImGuiTheme m_ActiveTheme = ImGuiTheme::WalnutDefault;
	bool m_bStyleInvert = false;
	float m_Alpha = 1.0f;
	bool m_ActiveBStyleInvert = false;
	float m_ActiveAlpha = 1.0f;
	ImGuiStyle m_DefaultStyle;
	float m_PreviewTimer = 0.0f;
};
