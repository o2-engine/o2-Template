#pragma once

#include "o2/Sound/SoundPlayer.h"

using namespace o2;

namespace td
{
	// ---------------------------------------------------------------------------------
	// Game sound bank: looped beds (gameplay music, town ambience, car engine and tyre
	// screech) plus the one-shots (win, lose, token credit, button click). The engine
	// exposes only a master volume, so the settings switches are per-player volume and
	// trigger gates here. Owned by the controller component and pumped with the real
	// frame dt, so the sounds keep running through the gameplay pause.
	// ---------------------------------------------------------------------------------
	class GameAudio: public RefCounterable
	{
	public:
		// Creates the players over the Sound/*.mp3 assets and starts the town ambience
		void Load();

		// Pumps the players
		void Update(float dt);

		// Enables or disables all sound effects; the settings window switch
		void SetSoundEnabled(bool enabled);

		// Returns are the sound effects enabled
		bool IsSoundEnabled() const { return mSoundEnabled; }

		// Enables or disables the music; the settings window switch
		void SetMusicEnabled(bool enabled);

		// Returns is the music enabled
		bool IsMusicEnabled() const { return mMusicEnabled; }

		// Plays or stops the gameplay music; off while a result window is up
		void SetMusicActive(bool active);

		// Drives the engine loop: on while the car moves, pitched up with the speed
		void SetDriving(float speedNorm);

		// Drives the tyre screech loop by the drift intensity after a turn
		void SetDrift(float intensity);

		// One-shot button click; every button and switch goes through it
		void PlayButton();

		// One-shot token credit tick: a chip landed in the car bed or in an order tooltip
		void PlayChips();

		// One-shot win jingle
		void PlayWin();

		// One-shot lose jingle
		void PlayLose();

		// Players exposed for tests
		const Ref<SoundPlayer>& GetMusicPlayer() const { return mMusic; }
		const Ref<SoundPlayer>& GetTownPlayer() const { return mTown; }
		const Ref<SoundPlayer>& GetEnginePlayer() const { return mEngine; }
		const Ref<SoundPlayer>& GetTyresPlayer() const { return mTyres; }
		const Ref<SoundPlayer>& GetWinPlayer() const { return mWinning; }
		const Ref<SoundPlayer>& GetLosePlayer() const { return mLoose; }
		const Ref<SoundPlayer>& GetChipsPlayer() const { return mChips; }
		const Ref<SoundPlayer>& GetButtonPlayer() const { return mBtn; }

	private:
		bool mSoundEnabled = true; // Sound effects switch state
		bool mMusicEnabled = true; // Music switch state

		float mDriftIntensity = 0.0f; // Last drift intensity, scales the tyres volume

		Ref<SoundPlayer> mMusic;   // Gameplay music loop (Sound/back.mp3)
		Ref<SoundPlayer> mTown;    // Town ambience loop, always on (Sound/town.mp3)
		Ref<SoundPlayer> mEngine;  // Player car engine loop (Sound/engine.mp3)
		Ref<SoundPlayer> mTyres;   // Tyre screech loop while drifting (Sound/tyres.mp3)
		Ref<SoundPlayer> mWinning; // Win jingle (Sound/winning.mp3)
		Ref<SoundPlayer> mLoose;   // Lose jingle (Sound/loose.mp3)
		Ref<SoundPlayer> mChips;   // Token credit tick (Sound/chips.mp3)
		Ref<SoundPlayer> mBtn;     // Button click (Sound/btn.mp3)

		Vector<Ref<SoundPlayer>> mPlayers; // All of the above, for the update pump

	private:
		// Creates a player over the sound asset at the path
		static Ref<SoundPlayer> MakePlayer(const String& path, float volume, Loop loop);

		// Starts or stops a loop to match the wanted state
		static void SetLooping(const Ref<SoundPlayer>& player, bool playing);

		// Plays a one-shot from the start; skipped while the effects are switched off
		void PlayOneShot(const Ref<SoundPlayer>& player);

		// Reapplies the switch-gated volumes to every player
		void ApplyVolumes();
	};
}
