#include "o2/stdafx.h"
#include "TokenDelivery/GameUIStyle.h"

#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/VectorFontAsset.h"
#include "o2/Render/FontStyle.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/Text.h"
#include "o2/Render/VectorFontEffects.h"
#include "o2/Scene/UI/UIManager.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Scene/UI/Widgets/HorizontalProgress.h"
#include "o2/Scene/UI/Widgets/Label.h"

using namespace o2;

namespace td
{
	static AssetRef<VectorFontAsset> UIFont()
	{
		auto font = o2Assets.GetAssetRefByType<VectorFontAsset>(String("Game/UI/Fredoka-SemiBold.ttf"));
		if (!font)
			font = o2Assets.GetAssetRefByType<VectorFontAsset>(String("debugFont.ttf"));
		return font;
	}

	static Ref<Text> MakeStyleText(int height, const Color4& color, bool shadow, bool stroke)
	{
		auto text = mmake<Text>(UIFont());
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

		// dark navy text for white pills/bubbles/windows (reference numbers)
		auto darkLabel = mmake<Label>();
		darkLabel->AddLayer("text", MakeStyleText(24, Color4(58, 70, 94, 255), false, false));
		o2UI.AddWidgetStyle(darkLabel, "dark");

		auto makeButton = [&](const char* sprite, const char* style, bool withCaption)
		{
			auto button = mmake<Button>();
			button->AddLayer("back", mmake<Sprite>(String(sprite)));
			if (withCaption)
				button->AddLayer("caption", MakeStyleText(30, Color4(255, 255, 255, 255), false, true));
			o2UI.AddWidgetStyle(button, style);
		};
		makeButton("Game/UI/button_blue.png", "standard", true);
		makeButton("Game/UI/button_green.png", "green", true);
		makeButton("Game/UI/boost_wide.png", "boost", false);
		makeButton("Game/UI/arrow_n.png", "arrow_n", false);
		makeButton("Game/UI/arrow_e.png", "arrow_e", false);
		makeButton("Game/UI/arrow_s.png", "arrow_s", false);
		makeButton("Game/UI/arrow_w.png", "arrow_w", false);
		makeButton("Game/UI/gear.png", "gear", false);

		// the dark capsule sticks out around the bar, so it stays visible at full value
		auto progress = mmake<HorizontalProgress>();
		progress->AddLayer("back", mmake<Sprite>(String("Game/UI/fuel_bg.png")),
						   Layout::BothStretch(-9.0f, -8.0f, -9.0f, -8.0f));
		progress->AddLayer("bar", mmake<Sprite>(String("Game/UI/fuel_fill.png")));
		o2UI.AddWidgetStyle(progress, "fuel");
	}
}
