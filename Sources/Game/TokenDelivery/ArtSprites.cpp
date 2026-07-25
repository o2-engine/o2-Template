#include "o2/stdafx.h"
#include "TokenDelivery/ArtSprites.h"

#include "o2/Scene/Components/ImageComponent.h"

namespace td::art
{
	const SpriteMeta* Find(const char* path)
	{
		for (auto& meta : kSprites)
		{
			if (strcmp(meta.path, path) == 0)
				return &meta;
		}
		return nullptr;
	}

	Vec2F NormalizedPivot(const SpriteMeta& meta)
	{
		return Vec2F((float)meta.px/(float)meta.w, 1.0f - (float)meta.py/(float)meta.h);
	}

	Ref<Actor> MakeSprite(const char* path, const String& layer, const Vec2F& worldPos,
						  float depth, const Ref<Actor>& parent)
	{
		auto meta = Find(path);
		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName(path);
		actor->SetLayer(layer);

		auto image = actor->AddComponent<ImageComponent>();
		image->LoadFromImage(String(path));

		if (meta)
		{
			actor->transform->SetSize2D(Vec2F((float)meta->w, (float)meta->h));
			actor->transform->SetPivot2D(NormalizedPivot(*meta));
		}
		else
			image->FitActorByImage();

		if (parent)
			parent->AddChild(actor, false);

		actor->transform->SetWorldPosition(Vec3F(worldPos.x, worldPos.y, 0.0f));
		actor->SetDrawingDepth(depth);
		return actor;
	}
}
