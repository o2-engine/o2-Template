#pragma once

#include "TokenDelivery/ArtManifest.g.h"
#include "TokenDelivery/IsoMath.h"
#include "o2/Scene/Actor.h"

using namespace o2;

namespace td::art
{
	const SpriteMeta* Find(const char* path);

	// Creates an actor with an ImageComponent, sized to the image with the pivot from the
	// manifest, placed at worldPos with an explicit draw depth on the given layer
	Ref<Actor> MakeSprite(const char* path, const String& layer, const Vec2F& worldPos,
						  float depth, const Ref<Actor>& parent = nullptr);

	// Pivot in normalized bottom-left coordinates (art pivots are top-left pixel based)
	Vec2F NormalizedPivot(const SpriteMeta& meta);
}
