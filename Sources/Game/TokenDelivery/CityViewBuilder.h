#pragma once

#include "TokenDelivery/CityModel.h"
#include "o2/Scene/Actor.h"

using namespace o2;

namespace td
{
	constexpr const char* kWorldLayer = "World";
	constexpr const char* kUILayer = "UI";

	struct CityViewHandles
	{
		Ref<Actor>         root;           // all static city actors are children of this
		Vector<Ref<Actor>> officeAnchors;  // per order: world point above the office roof
		Ref<Actor>         hologram;       // pulsing chip above the fountain
	};

	// Instantiates sprites for the whole generated city: backdrop, ground tiles, buildings,
	// props. Static draw depths follow the iso depth of the footprint.
	CityViewHandles BuildCityView(const CityModel& city);
}
