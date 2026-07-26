#include "o2/stdafx.h"
#include "o2/O2.h"
#include "o2/Application/Application.h"
#include "TokenDelivery/GameAssetsGen.h"

#include <filesystem>

extern void InitializeTypesGameLib();

using namespace o2;

// Assets generator tool: boots a full application (render is needed to construct the
// sprite components) and writes the game prototypes and the bootstrap scene into the
// project Assets/. Run it after changing the construction code in GameAssetsGen.cpp,
// then rebuild assets. cwd is pinned to the executable folder so the relative asset
// paths resolve the same way as a direct game launch from Bin.
int main(int argc, char** argv)
{
	if (argc > 0 && argv[0])
	{
		std::filesystem::path exe(argv[0]);
		if (exe.has_parent_path())
		{
			std::error_code ec;
			std::filesystem::current_path(exe.parent_path(), ec);
		}
	}

	// UIDs of new assets come from rand(); unseeded it repeats the same sequence every
	// run and regenerated assets collide with ids kept from earlier generations
	srand((unsigned)time(nullptr));

	InitializeTypesGameLib();
	INITIALIZE_O2;

	auto app = mmake<Application>();
	app->Initialize();

	td::GenerateGameAssets();

	app->Deinitialize();
	return 0;
}
