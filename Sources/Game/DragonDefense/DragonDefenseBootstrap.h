#pragma once

#include "o2/Scene/Component.h"
#include "o2/Utils/Math/Color.h"

using namespace o2;

namespace o2
{
	class Button;
	class Image;
	class Label;
	class Sprite;
}

// Entry point of the Dragon Defense prototype. Lives in the bootstrap scene;
// on start it builds both game screens (battle and castle) in code, runs the
// model script and attaches the JS game controller.
class DragonDefenseBootstrap: public Component
{
public:
	// Creates the bootstrap actor in the current scene; used by the app and tests
	static Ref<Actor> CreateBootstrapActor();

	// Builds a minimal scene with only the bootstrap actor and saves it to path
	static void SaveBootstrapScene(const String& path);

	SERIALIZABLE(DragonDefenseBootstrap);
	CLONEABLE_REF(DragonDefenseBootstrap);

private:
	bool mBuilt = false;

	void OnStart() override;

	void BuildLayersAndCamera();
	void BuildBattleScreen(const Ref<Actor>& root);
	void BuildCastleScreen(const Ref<Actor>& root);
	void BuildSettingsScreen(const Ref<Actor>& root);
	void BuildGameController(const Ref<Actor>& root);

	static Ref<Actor> CreateContainer(const Ref<Actor>& parent, const String& name);

	static Ref<Actor> CreateSprite(const Ref<Actor>& parent, const String& name, const String& image,
								   const String& layer, const Vec2F& pos, const Vec2F& size,
								   const Color4& color = Color4::White());

	static void SetWidgetRect(const Ref<Widget>& widget, const Vec2F& pos, const Vec2F& size);

	static void SetWidgetDepth(const Ref<Widget>& widget, float depth);

	static Ref<Sprite> MakeSliced(const String& image, const BorderI& slice);

	static Ref<Image> CreateImageWidget(const Ref<Actor>& parent, const String& name, const String& image,
										const Vec2F& pos, const Vec2F& size, const Color4& color = Color4::White(),
										float depth = 1.0f, const BorderI& slice = BorderI());

	static void CreateBar(const Ref<Actor>& parent, const String& namePrefix, const Vec2F& pos,
						  const Vec2F& size, const Color4& fillColor);

	static Ref<Button> CreateButton(const Ref<Actor>& parent, const String& name, const Vec2F& pos, const Vec2F& size,
									const WString& caption, int captionHeight, const String& icon, const Vec2F& iconSize);

	static Ref<Label> CreateLabel(const Ref<Actor>& parent, const String& name, const WString& text,
								  const Vec2F& rectMin, const Vec2F& rectMax, int height,
								  const Color4& color, HorAlign horAlign);

	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(DragonDefenseBootstrap)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(DragonDefenseBootstrap)
{
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mBuilt);
}
END_META;
CLASS_METHODS_META(DragonDefenseBootstrap)
{

    FUNCTION().PUBLIC().SIGNATURE_STATIC(Ref<Actor>, CreateBootstrapActor);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(void, SaveBootstrapScene, const String&);
    FUNCTION().PRIVATE().SIGNATURE(void, OnStart);
    FUNCTION().PRIVATE().SIGNATURE(void, BuildLayersAndCamera);
    FUNCTION().PRIVATE().SIGNATURE(void, BuildBattleScreen, const Ref<Actor>&);
    FUNCTION().PRIVATE().SIGNATURE(void, BuildCastleScreen, const Ref<Actor>&);
    FUNCTION().PRIVATE().SIGNATURE(void, BuildSettingsScreen, const Ref<Actor>&);
    FUNCTION().PRIVATE().SIGNATURE(void, BuildGameController, const Ref<Actor>&);
    FUNCTION().PRIVATE().SIGNATURE_STATIC(Ref<Actor>, CreateContainer, const Ref<Actor>&, const String&);
    FUNCTION().PRIVATE().SIGNATURE_STATIC(Ref<Actor>, CreateSprite, const Ref<Actor>&, const String&, const String&, const String&, const Vec2F&, const Vec2F&, const Color4&);
    FUNCTION().PRIVATE().SIGNATURE_STATIC(void, SetWidgetRect, const Ref<Widget>&, const Vec2F&, const Vec2F&);
    FUNCTION().PRIVATE().SIGNATURE_STATIC(void, SetWidgetDepth, const Ref<Widget>&, float);
    FUNCTION().PRIVATE().SIGNATURE_STATIC(Ref<Sprite>, MakeSliced, const String&, const BorderI&);
    FUNCTION().PRIVATE().SIGNATURE_STATIC(Ref<Image>, CreateImageWidget, const Ref<Actor>&, const String&, const String&, const Vec2F&, const Vec2F&, const Color4&, float, const BorderI&);
    FUNCTION().PRIVATE().SIGNATURE_STATIC(void, CreateBar, const Ref<Actor>&, const String&, const Vec2F&, const Vec2F&, const Color4&);
    FUNCTION().PRIVATE().SIGNATURE_STATIC(Ref<Button>, CreateButton, const Ref<Actor>&, const String&, const Vec2F&, const Vec2F&, const WString&, int, const String&, const Vec2F&);
    FUNCTION().PRIVATE().SIGNATURE_STATIC(Ref<Label>, CreateLabel, const Ref<Actor>&, const String&, const WString&, const Vec2F&, const Vec2F&, int, const Color4&, HorAlign);
}
END_META;
// --- END META ---
