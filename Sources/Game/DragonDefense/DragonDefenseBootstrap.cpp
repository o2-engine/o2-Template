#include "o2/stdafx.h"
#include "DragonDefenseBootstrap.h"

#include "DragonInputComponent.h"
#include "DragonLayout.h"
#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/JavaScriptAsset.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/Text.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Components/ScriptableComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/WidgetLayer.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Scene/UI/Widgets/Image.h"
#include "o2/Scene/UI/Widgets/Label.h"
#include "o2/Scripts/ScriptEngine.h"
#include "o2/Utils/Math/Layout.h"

using namespace dragon;

static const String kSprites = "DragonDefense/Sprites/";
static const String kFont = "DragonDefense/Fonts/Philosopher-Bold.ttf";
static const Color4 kTextColor(255, 255, 255);
static const Color4 kGoldColor(255, 214, 130);
static const Color4 kButtonTextColor(84, 52, 22);

static const BorderI kPanelSlice(22, 22, 22, 22);
static const BorderI kButtonSlice(14, 14, 14, 14);
static const BorderI kBarFrameSlice(22, 10, 22, 10);
static const BorderI kBarFillSlice(9, 9, 9, 9);

Ref<Actor> DragonDefenseBootstrap::CreateBootstrapActor()
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName("Bootstrap");
	actor->AddComponent<DragonDefenseBootstrap>();
	return actor;
}

void DragonDefenseBootstrap::SaveBootstrapScene(const String& path)
{
	CreateBootstrapActor();
	o2Scene.UpdateAddedEntities(); // register the actor in scene roots without starting it
	o2Scene.Save(path);
}

void DragonDefenseBootstrap::OnStart()
{
	if (mBuilt)
		return;

	mBuilt = true;

	auto prevMode = Actor::GetDefaultCreationMode();
	Actor::SetDefaultCreationMode(ActorCreateMode::InScene);

	BuildLayersAndCamera();

	// common root so that sibling screens are reachable from scripts via ".." paths
	auto root = CreateContainer(nullptr, "DragonDefense");
	BuildBattleScreen(root);
	BuildCastleScreen(root);
	BuildSettingsScreen(root);
	BuildGameController(root);

	Actor::SetDefaultCreationMode(prevMode);
}

void DragonDefenseBootstrap::BuildLayersAndCamera()
{
	Vector<String> layers = { "BG", "Field", "Objects", "Dragons", "FX", "FXBar", "FXBarFill", "UI" };
	for (auto& layer : layers)
		o2Scene.AddLayer(layer);

	auto camera = mmake<CameraActor>();
	camera->SetName("ui camera");
	camera->SetFittedSize(kScreenSize);
	camera->fillBackground = true;
	camera->fillColor = Color4(24, 26, 34);
	camera->drawLayers.SetLayers(layers);
}

Ref<Actor> DragonDefenseBootstrap::CreateContainer(const Ref<Actor>& parent, const String& name)
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName(name);
	if (parent)
		parent->AddChild(actor);

	actor->transform->SetSize2D(Vec2F(0, 0));
	actor->transform->SetPosition2D(Vec2F(0, 0));
	return actor;
}

Ref<Actor> DragonDefenseBootstrap::CreateSprite(const Ref<Actor>& parent, const String& name, const String& image,
												const String& layer, const Vec2F& pos, const Vec2F& size,
												const Color4& color)
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName(name);
	if (parent)
		parent->AddChild(actor);

	actor->SetLayer(layer);

	auto sprite = actor->AddComponent<ImageComponent>();
	sprite->LoadFromImage(image, false);
	if (color != Color4::White())
		sprite->SetColor(color);

	actor->transform->SetSize2D(size);
	actor->transform->SetPosition2D(pos);
	return actor;
}

void DragonDefenseBootstrap::SetWidgetRect(const Ref<Widget>& widget, const Vec2F& pos, const Vec2F& size)
{
	widget->layout->anchorMin = Vec2F(0, 0);
	widget->layout->anchorMax = Vec2F(0, 0);
	widget->layout->offsetMin = pos - size*0.5f;
	widget->layout->offsetMax = pos + size*0.5f;
}

void DragonDefenseBootstrap::SetWidgetDepth(const Ref<Widget>& widget, float depth)
{
	// equal depths are ordered unpredictably inside a layer — set explicit ones
	widget->SetDrawingDepthInheritFromParent(false);
	widget->SetDrawingDepth(depth);
}

Ref<Sprite> DragonDefenseBootstrap::MakeSliced(const String& image, const BorderI& slice)
{
	auto sprite = mmake<Sprite>(image);
	sprite->SetMode(SpriteMode::Sliced);
	sprite->SetSliceBorder(slice);
	return sprite;
}

Ref<Image> DragonDefenseBootstrap::CreateImageWidget(const Ref<Actor>& parent, const String& name, const String& image,
													 const Vec2F& pos, const Vec2F& size, const Color4& color,
													 float depth, const BorderI& slice)
{
	auto widget = mmake<Image>();
	widget->SetName(name);
	if (parent)
		parent->AddChild(widget);

	widget->SetLayer("UI");

	auto sprite = slice != BorderI() ? MakeSliced(image, slice) : mmake<Sprite>(image);
	if (color != Color4::White())
		sprite->SetColor(color);
	widget->SetImage(sprite);

	SetWidgetRect(widget, pos, size);
	SetWidgetDepth(widget, depth);
	return widget;
}

Ref<Button> DragonDefenseBootstrap::CreateButton(const Ref<Actor>& parent, const String& name, const Vec2F& pos,
												 const Vec2F& size, const WString& caption, int captionHeight,
												 const String& icon, const Vec2F& iconSize)
{
	auto button = mmake<Button>();
	button->SetName(name);
	if (parent)
		parent->AddChild(button);

	button->SetLayer("UI");
	button->AddLayer("back", MakeSliced(kSprites + "button9.png", kButtonSlice), Layout::BothStretch());

	if (!icon.IsEmpty())
		button->AddLayer("icon", mmake<Sprite>(icon), Layout::Based(BaseCorner::Center, iconSize));

	if (!caption.IsEmpty())
	{
		auto text = mmake<Text>(kFont);
		text->SetHeight(captionHeight);
		text->SetColor(kButtonTextColor);
		text->SetHorAlign(HorAlign::Middle);
		text->SetVerAlign(VerAlign::Middle);
		button->AddLayer("caption", text, Layout::BothStretch());
		button->SetCaption(caption);
	}

	SetWidgetRect(button, pos, size);
	SetWidgetDepth(button, 10.0f);
	return button;
}

Ref<Label> DragonDefenseBootstrap::CreateLabel(const Ref<Actor>& parent, const String& name, const WString& text,
											   const Vec2F& rectMin, const Vec2F& rectMax, int height,
											   const Color4& color, HorAlign horAlign)
{
	auto label = mmake<Label>();
	label->SetName(name);
	if (parent)
		parent->AddChild(label);

	label->SetLayer("UI");
	label->SetFontAsset(AssetRef<FontAsset>(kFont));
	label->SetHeight(height);
	label->SetColor(color);
	label->SetHorAlign(horAlign);
	label->SetVerAlign(VerAlign::Middle);
	label->layout->anchorMin = Vec2F(0, 0);
	label->layout->anchorMax = Vec2F(0, 0);
	label->layout->offsetMin = rectMin;
	label->layout->offsetMax = rectMax;
	label->SetText(text);
	SetWidgetDepth(label, 20.0f);
	return label;
}

void DragonDefenseBootstrap::CreateBar(const Ref<Actor>& parent, const String& namePrefix, const Vec2F& pos,
									   const Vec2F& size, const Color4& fillColor)
{
	CreateImageWidget(parent, namePrefix + "BarFrame", kSprites + "bar_frame9.png", pos, size,
					  Color4::White(), 2.0f, kBarFrameSlice);

	// the fill is a sliced sprite: scripts animate progress by resizing the widget width
	Vec2F fillSize(size.x - 10.0f, size.y - 10.0f);
	auto fill = CreateImageWidget(parent, namePrefix + "BarFill", kSprites + "bar_fill9.png", pos, fillSize,
								  fillColor, 3.0f, kBarFillSlice);
	fill->layout->anchorMin = Vec2F(0, 0);
	fill->layout->anchorMax = Vec2F(0, 0);
	fill->layout->offsetMin = pos - fillSize*0.5f;
	fill->layout->offsetMax = pos + fillSize*0.5f;
}

void DragonDefenseBootstrap::BuildBattleScreen(const Ref<Actor>& root)
{
	auto battle = CreateContainer(root, "Battle");

	CreateSprite(battle, "BG", kSprites + "battle_bg.png", "BG", Vec2F(0, 0), kScreenSize);
	CreateSprite(battle, "FrontLine", kSprites + "front_line.png", "Field", kFrontLinePos, kFrontLineSize);

	auto hud = CreateContainer(battle, "Hud");
	CreateImageWidget(hud, "TopLeftPanel", kSprites + "panel9.png", Vec2F(-410, 356), Vec2F(430, 84),
					  Color4::White(), 1.0f, kPanelSlice);
	CreateLabel(hud, "CastleHpLabel", "ЗАМОК HP: 100/100", Vec2F(-595, 364), Vec2F(-225, 390), 18, kTextColor, HorAlign::Left);
	CreateBar(hud, "Hp", Vec2F(-410, 340), Vec2F(370, 22), Color4(214, 60, 46));

	CreateImageWidget(hud, "TopRightPanel", kSprites + "panel9.png", Vec2F(15, 356), Vec2F(430, 84),
					  Color4::White(), 1.0f, kPanelSlice);
	CreateLabel(hud, "FortWaveLabel", "ФОРТ 2   ВОЛНА: 0/10", Vec2F(-170, 364), Vec2F(200, 390), 18, kTextColor, HorAlign::Left);
	CreateBar(hud, "Wave", Vec2F(15, 340), Vec2F(370, 22), Color4(235, 186, 52));

	CreateButton(hud, "NextWaveBtn", kNextWaveBtnPos, kNextWaveBtnSize, "СЛЕД. ВОЛНА", 15, "", Vec2F());

	CreateLabel(hud, "MessageShadow", "", Vec2F(-497, -4), Vec2F(503, 96), 44, Color4(20, 12, 8, 200), HorAlign::Middle);
	CreateLabel(hud, "Message", "", Vec2F(-500, 0), Vec2F(500, 100), 44, Color4(255, 226, 140), HorAlign::Middle);

	auto buyPanel = CreateContainer(battle, "BuyPanel");
	CreateImageWidget(buyPanel, "Panel", kSprites + "panel9.png", Vec2F(-374, -352), Vec2F(500, 88),
					  Color4::White(), 1.0f, kPanelSlice);
	CreateLabel(buyPanel, "Caption", "КУПИТЬ ЮНИТ 2 УР. (100 з.)", Vec2F(-614, -330), Vec2F(-134, -306), 16, kGoldColor, HorAlign::Middle);
	CreateLabel(buyPanel, "CoinsLabel", "Монеты: 0", Vec2F(-240, -378), Vec2F(-134, -352), 15, kGoldColor, HorAlign::Middle);

	const char* classes[4] = { "tank", "archer", "mage", "support" };
	for (int i = 0; i < 4; i++)
	{
		CreateButton(buyPanel, String::Format("Buy%i", i), BuyBtnPos(i), kBuyBtnSize,
					 "", 0, kSprites + String::Format("unit_%s_2.png", classes[i]), Vec2F(32, 44));
	}

	auto boosterPanel = CreateContainer(battle, "BoosterPanel");
	CreateImageWidget(boosterPanel, "Panel", kSprites + "panel9.png", Vec2F(374, -352), Vec2F(500, 88),
					  Color4::White(), 1.0f, kPanelSlice);
	CreateLabel(boosterPanel, "Caption", "ТАКТИЧЕСКИЕ БУСТЕРЫ", Vec2F(134, -330), Vec2F(614, -306), 16, kGoldColor, HorAlign::Middle);

	const char* boosterIcons[3] = { "icon_bomb.png", "icon_freeze.png", "icon_swap.png" };
	for (int i = 0; i < 3; i++)
	{
		CreateButton(boosterPanel, String::Format("Booster%i", i), BoosterBtnPos(i), kBoosterBtnSize,
					 "", 0, kSprites + boosterIcons[i], Vec2F(40, 40));
		CreateLabel(boosterPanel, String::Format("Charge%i", i), "x0",
					Vec2F(BoosterBtnPos(i).x + 2, -392), Vec2F(BoosterBtnPos(i).x + 32, -370), 14, kTextColor, HorAlign::Middle);
	}
}

void DragonDefenseBootstrap::BuildCastleScreen(const Ref<Actor>& root)
{
	auto castle = CreateContainer(root, "Castle");

	CreateSprite(castle, "BG", kSprites + "castle_bg.png", "BG", Vec2F(0, 0), kScreenSize);
	CreateSprite(castle, "Barracks", kSprites + "building_barracks.png", "Objects", Vec2F(-340, -50), Vec2F(280, 300));
	CreateSprite(castle, "Forge", kSprites + "building_forge.png", "Objects", Vec2F(340, -60), Vec2F(300, 280));

	CreateImageWidget(castle, "TopPanel", kSprites + "panel9.png", Vec2F(-420, 356), Vec2F(400, 84),
					  Color4::White(), 1.0f, kPanelSlice);
	CreateLabel(castle, "CurrencyLabel", "Монеты: 0   Материалы: 0", Vec2F(-590, 340), Vec2F(-250, 366), 17, kTextColor, HorAlign::Left);
	CreateLabel(castle, "CastleTitle", "ЗАМОК", Vec2F(-590, 362), Vec2F(-250, 392), 20, kGoldColor, HorAlign::Left);
	CreateLabel(castle, "TitleLabel", "DRAGON DEFENSE", Vec2F(-250, 336), Vec2F(250, 390), 34, kGoldColor, HorAlign::Middle);

	CreateLabel(castle, "BarracksInfo", "КАЗАРМЫ УР. 2", Vec2F(-470, -182), Vec2F(-210, -150), 19, kTextColor, HorAlign::Middle);
	CreateButton(castle, "BarracksBtn", kBarracksBtnPos, kUpgradeBtnSize, "УЛУЧШИТЬ", 16, "", Vec2F());

	CreateLabel(castle, "ForgeInfo", "КУЗНИЦА УР. 0", Vec2F(210, -182), Vec2F(470, -150), 19, kTextColor, HorAlign::Middle);
	CreateButton(castle, "ForgeBtn", kForgeBtnPos, kUpgradeBtnSize, "УЛУЧШИТЬ", 16, "", Vec2F());

	CreateButton(castle, "ToBattleBtn", kToBattleBtnPos, kToBattleBtnSize, "В БОЙ!", 28, "", Vec2F());
	CreateButton(castle, "BalanceBtn", kBalanceBtnPos, kBalanceBtnSize, "БАЛАНС", 18, "", Vec2F());
}

void DragonDefenseBootstrap::BuildSettingsScreen(const Ref<Actor>& root)
{
	auto settings = CreateContainer(root, "Settings");

	CreateSprite(settings, "BG", kSprites + "castle_bg.png", "BG", Vec2F(0, 0), kScreenSize);
	CreateSprite(settings, "Dim", kSprites + "white.png", "Field", Vec2F(0, 0), kScreenSize, Color4(12, 10, 16, 185));

	CreateImageWidget(settings, "Panel", kSprites + "panel9.png", Vec2F(0, -10), Vec2F(1180, 724),
					  Color4::White(), 1.0f, kPanelSlice);
	CreateLabel(settings, "Title", "БАЛАНС ИГРЫ", Vec2F(-300, 292), Vec2F(300, 330), 28, kGoldColor, HorAlign::Middle);

	// parameter rows; scripts fill in the names and values
	auto params = CreateContainer(settings, "Params");
	for (int i = 0; i < kSettingsParamRows; i++)
	{
		Vec2F row = SettingsParamRowPos(i);
		CreateLabel(params, String::Format("ParamName%i", i), "", row + Vec2F(-270, -13), row + Vec2F(-15, 13), 15, kTextColor, HorAlign::Left);
		CreateButton(params, String::Format("ParamMinus%i", i), row + kParamMinusOffset, kParamBtnSize, "-", 20, "", Vec2F());
		CreateLabel(params, String::Format("ParamValue%i", i), "", row + Vec2F(40, -13), row + Vec2F(120, 13), 16, kGoldColor, HorAlign::Middle);
		CreateButton(params, String::Format("ParamPlus%i", i), row + kParamPlusOffset, kParamBtnSize, "+", 20, "", Vec2F());
	}

	auto toggles = CreateContainer(settings, "Toggles");
	CreateLabel(toggles, "UnitsCaption", "ЮНИТЫ:", Vec2F(-575, -168), Vec2F(-425, -136), 17, kGoldColor, HorAlign::Left);
	const char* unitIcons[4] = { "unit_tank_1.png", "unit_archer_1.png", "unit_mage_1.png", "unit_support_1.png" };
	for (int i = 0; i < 4; i++)
	{
		CreateButton(toggles, String::Format("UnitToggle%i", i), UnitTogglePos(i), kToggleBtnSize,
					 "", 0, kSprites + unitIcons[i], Vec2F(28, 38));
	}

	CreateLabel(toggles, "DragonsCaption", "ДРАКОНЫ:", Vec2F(-575, -238), Vec2F(-425, -206), 17, kGoldColor, HorAlign::Left);
	const char* dragonIcons[5] = { "dragon_normal.png", "dragon_tank.png", "dragon_range.png", "dragon_fire.png", "dragon_boss.png" };
	for (int i = 0; i < 5; i++)
	{
		CreateButton(toggles, String::Format("DragonToggle%i", i), DragonTogglePos(i), kToggleBtnSize,
					 "", 0, kSprites + dragonIcons[i], Vec2F(38, 38));
	}

	CreateButton(settings, "BackBtn", kSettingsBackBtnPos, kSettingsBackBtnSize, "НАЗАД", 20, "", Vec2F());
}

void DragonDefenseBootstrap::BuildGameController(const Ref<Actor>& root)
{
	auto modelScript = o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/DragonDefense/DragonModel.js"));
	if (modelScript)
		o2Scripts.Run(modelScript->Parse());

	auto game = CreateContainer(root, "Game");
	game->AddComponent<DragonInputComponent>();

	auto scriptable = game->AddComponent<ScriptableComponent>();
	scriptable->SetScript(o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/DragonDefense/DragonGame.js")));
}
// --- META ---

DECLARE_CLASS(DragonDefenseBootstrap, DragonDefenseBootstrap);
// --- END META ---
