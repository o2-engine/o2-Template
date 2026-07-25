#include "o2/stdafx.h"
#include "TokenDelivery/GameUIStyle.h"

#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/VectorFontAsset.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/Text.h"
#include "o2/Scene/UI/UIManager.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Scene/UI/Widgets/HorizontalProgress.h"
#include "o2/Scene/UI/Widgets/Label.h"

using namespace o2;

namespace td
{
	static Ref<Text> MakeStyleText(const AssetRef<VectorFontAsset>& font, int height,
								   const Color4& color)
	{
		auto text = mmake<Text>(font);
		text->SetHeight(height);
		text->SetColor(color);
		text->SetHorAlign(HorAlign::Middle);
		text->SetVerAlign(VerAlign::Middle);
		return text;
	}

	void BuildGameUIStyles()
	{
		auto font = o2Assets.GetAssetRefByType<VectorFontAsset>(String("debugFont.ttf"));

		auto label = mmake<Label>();
		label->AddLayer("text", MakeStyleText(font, 24, Color4(255, 255, 255, 255)));
		o2UI.AddWidgetStyle(label, "standard");

		auto makeButton = [&](const char* sprite, const char* style, bool withCaption)
		{
			auto button = mmake<Button>();
			button->AddLayer("back", mmake<Sprite>(String(sprite)));
			if (withCaption)
				button->AddLayer("caption", MakeStyleText(font, 30, Color4(255, 255, 255, 255)));
			o2UI.AddWidgetStyle(button, style);
		};
		makeButton("Game/UI/button_blue.png", "standard", true);
		makeButton("Game/UI/button_green.png", "green", true);
		makeButton("Game/UI/boost_btn.png", "boost", false);
		makeButton("Game/UI/arrow_n.png", "arrow_n", false);
		makeButton("Game/UI/arrow_e.png", "arrow_e", false);
		makeButton("Game/UI/arrow_s.png", "arrow_s", false);
		makeButton("Game/UI/arrow_w.png", "arrow_w", false);
		makeButton("Game/UI/gear.png", "gear", false);

		auto progress = mmake<HorizontalProgress>();
		progress->AddLayer("back", mmake<Sprite>(String("Game/UI/fuel_bg.png")));
		progress->AddLayer("bar", mmake<Sprite>(String("Game/UI/fuel_fill.png")),
						   Layout::BothStretch(10.0f, 9.0f, 10.0f, 9.0f));
		o2UI.AddWidgetStyle(progress, "fuel");
	}
}
