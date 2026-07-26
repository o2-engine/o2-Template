#include "o2/stdafx.h"
#include "TokenDelivery/GameUIStyle.h"

#include "o2/Animation/AnimationClip.h"
#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/VectorFontAsset.h"
#include "o2/Render/FontStyle.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/Text.h"
#include "o2/Render/VectorFontEffects.h"
#include "o2/Scene/UI/UIManager.h"
#include "o2/Scene/UI/WidgetLayer.h"
#include "o2/Scene/UI/WidgetState.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Scene/UI/Widgets/Label.h"
#include "o2/Scene/UI/Widgets/Toggle.h"

using namespace o2;

namespace td
{
	AssetRef<VectorFontAsset> GameUIFont()
	{
		// Baloo 2 ExtraBold (OFL, Google Fonts) — rounded chunky glyphs like the reference
		auto font = o2Assets.GetAssetRefByType<VectorFontAsset>(String("Game/UI/Baloo2-ExtraBold.ttf"));
		if (!font)
			font = o2Assets.GetAssetRefByType<VectorFontAsset>(String("Game/UI/Fredoka-SemiBold.ttf"));
		if (!font)
			font = o2Assets.GetAssetRefByType<VectorFontAsset>(String("debugFont.ttf"));
		return font;
	}

	Ref<Label> MakeGameLabel(const Ref<Widget>& parent, const WString& text, int height,
							 const String& style)
	{
		auto label = o2UI.CreateLabel(text, style);
		if (auto drawable = label->GetLayerDrawable<Text>("text"))
		{
			// widget style cloning resets the Text drawable (font, color, aligns, font
			// style) to engine defaults — reapply everything here
			drawable->SetFontAsset(GameUIFont());
			drawable->SetHeight(height);
			drawable->SetHorAlign(HorAlign::Middle);
			drawable->SetVerAlign(VerAlign::Middle);
			drawable->SetColor(style == "dark" ? Color4(34, 41, 65, 255)
							 : style == "quest" ? Color4(248, 240, 216, 255)
							 : Color4(255, 255, 255, 255));
			if (style == "quest")
			{
				auto fontStyle = mmake<FontStyle>();
				fontStyle->AddEffect<FontStrokeEffect>(2.5f, Color4(44, 58, 82, 220), 100);
				drawable->SetFontStyle(fontStyle);
			}
			else if (style == "standard")
			{
				auto fontStyle = mmake<FontStyle>();
				fontStyle->AddEffect<FontShadowEffect>(2.0f, Vec2I(1, -2), Color4(30, 40, 60, 90));
				drawable->SetFontStyle(fontStyle);
			}
		}
		parent->AddChild(label);
		return label;
	}

	static Ref<Text> MakeStyleText(int height, const Color4& color, bool shadow, bool stroke)
	{
		auto text = mmake<Text>(GameUIFont());
		text->SetHeight(height);
		text->SetColor(color);
		text->SetHorAlign(HorAlign::Middle);
		text->SetVerAlign(VerAlign::Middle);

		auto style = mmake<FontStyle>();
		if (stroke)
			style->AddEffect<FontStrokeEffect>(2.5f, Color4(44, 58, 82, 160), 100);
		if (shadow)
			style->AddEffect<FontShadowEffect>(2.0f, Vec2I(1, -2), Color4(30, 40, 60, 90));
		if (stroke || shadow)
			text->SetFontStyle(style);
		return text;
	}

	void BuildGameUIStyles()
	{
		// white text with a soft shadow — panels and captions, like the reference HUD
		auto label = mmake<Label>();
		label->AddLayer("text", MakeStyleText(24, Color4(255, 255, 255, 255), true, false));
		o2UI.AddWidgetStyle(label, "standard");

		// solid dark navy for white pills/bubbles/windows (reference numbers)
		auto darkLabel = mmake<Label>();
		darkLabel->AddLayer("text", MakeStyleText(24, Color4(34, 41, 65, 255), false, false));
		o2UI.AddWidgetStyle(darkLabel, "dark");

		// beige with a dark outline — the completed task panel text
		auto questLabel = mmake<Label>();
		questLabel->AddLayer("text", MakeStyleText(24, Color4(248, 240, 216, 255), false, true));
		o2UI.AddWidgetStyle(questLabel, "quest");

		// press-down: every visual layer shifts a few pixels down while the cursor holds
		auto pressAnim = [](bool withCaption)
		{
			const Vec2F down(0.0f, -6.0f);
			auto anim = AnimationClip::EaseInOut("layer/back/layout/offsetMin", Vec2F(), down, 0.06f);
			*anim->AddTrack<Vec2F>("layer/back/layout/offsetMax") =
				AnimationTrack<Vec2F>::EaseInOut(Vec2F(), down, 0.06f);
			if (withCaption)
			{
				*anim->AddTrack<Vec2F>("layer/caption/layout/offsetMin") =
					AnimationTrack<Vec2F>::EaseInOut(Vec2F(), down, 0.06f);
				*anim->AddTrack<Vec2F>("layer/caption/layout/offsetMax") =
					AnimationTrack<Vec2F>::EaseInOut(Vec2F(), down, 0.06f);
			}
			return anim;
		};

		auto makeButton = [&](const char* sprite, const char* style, bool withCaption)
		{
			auto button = mmake<Button>();
			button->AddLayer("back", mmake<Sprite>(String(sprite)));
			if (withCaption)
				button->AddLayer("caption", MakeStyleText(30, Color4(255, 255, 255, 255), false, true));
			button->AddState("pressed", pressAnim(withCaption))->offStateAnimationSpeed = 0.5f;
			o2UI.AddWidgetStyle(button, style);
		};
		makeButton("Game/UI/blue_btn.png", "blue", true);
		makeButton("Game/UI/ui_accept_btn.png", "accept", false);
		makeButton("Game/UI/settings_btn.png", "settings", false);

		// sound/music switch: empty socket is baked into the settings window, the style
		// draws the ON capsule fading in and the knob sliding left<->right (fixed 123x59)
		{
			const float kKnob = 72.0f;
			const Vec2F offMin(29.5f - kKnob*0.5f, -kKnob*0.5f);
			const Vec2F offMax(29.5f + kKnob*0.5f, kKnob*0.5f);
			const Vec2F shift(123.0f - 59.0f, 0.0f);

			auto toggle = mmake<Toggle>();
			toggle->AddLayer("on", mmake<Sprite>(String("Game/UI/ui_on_off_bg.png")));
			auto knob = toggle->AddLayer("knob", mmake<Sprite>(String("Game/UI/ui_on_off_knob.png")),
										 Layout(Vec2F(0.0f, 0.5f), Vec2F(0.0f, 0.5f), offMin, offMax));

			auto valueAnim = AnimationClip::EaseInOut("layer/on/transparency", 0.0f, 1.0f, 0.12f);
			*valueAnim->AddTrack<Vec2F>("layer/knob/layout/offsetMin") =
				AnimationTrack<Vec2F>::EaseInOut(offMin, offMin + shift, 0.12f);
			*valueAnim->AddTrack<Vec2F>("layer/knob/layout/offsetMax") =
				AnimationTrack<Vec2F>::EaseInOut(offMax, offMax + shift, 0.12f);
			toggle->AddState("value", valueAnim);
			o2UI.AddWidgetStyle(toggle, "switch");
		}
	}
}
