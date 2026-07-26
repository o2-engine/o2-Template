#pragma once

#include "o2/Sound/SoundPlayer.h"

using namespace o2;

namespace td
{
	// ---------------------------------------------------------------------------------
	// Game sound bank: looped beds (gameplay music, town ambience, car engine and tyre
	// screech) plus the one-shots (win, lose, token credit, button click). The engine
	// exposes only a master volume, so the settings switches are per-player volume and
	// trigger gates here.
	//
	// The players are deliberately not pumped through their IAnimation update: the
	// animation clock always drifts from the audio clock by the output latency, and the
	// resync seeks click and clip the tails. Instead the loops play on the backend
	// permanently and audibility is driven by short volume ramps; one-shots restart
	// from zero and end on the backend by themselves.
	// ---------------------------------------------------------------------------------
	class GameAudio: public RefCounterable
	{
	public:
		// Creates the players over the Sound/*.mp3 assets and starts the loop beds
		void Load();

		// Ramps the loop volumes toward their targets; real dt, runs through the pause
		void Update(float dt);

		// Enables or disables all sound effects; the settings window switch
		void SetSoundEnabled(bool enabled);

		// Returns are the sound effects enabled
		bool IsSoundEnabled() const { return mSoundEnabled; }

		// Enables or disables the music; the settings window switch
		void SetMusicEnabled(bool enabled);

		// Returns is the music enabled
		bool IsMusicEnabled() const { return mMusicEnabled; }

		// Fades the gameplay music in or out; off while a result window is up
		void SetMusicActive(bool active) { mMusicActive = active; }

		// Drives the engine loop: audible while the car moves, pitched up with the speed
		void SetDriving(float speedNorm);

		// Drives the tyre screech loop by the drift intensity after a turn
		void SetDrift(float intensity) { mDriftIntensity = Math::Clamp01(intensity); }

		// One-shot button click; every button and switch goes through it
		void PlayButton();

		// One-shot token credit tick: a chip landed in the car bed or in an order tooltip.
		// Round-robin voices, the filling stream ticks faster than one tick sounds
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
		const Ref<SoundPlayer>& GetButtonPlayer() const { return mBtn; }
		const Vector<Ref<SoundPlayer>>& GetChipsPlayers() const { return mChips; }

	private:
		bool mSoundEnabled = true; // Sound effects switch state
		bool mMusicEnabled = true; // Music switch state

		bool  mMusicActive = false;   // Is the gameplay music faded in
		float mSpeedNorm = 0.0f;      // Car speed 0..1, drives the engine loop
		float mDriftIntensity = 0.0f; // Drift intensity 0..1, drives the tyres loop

		Ref<SoundPlayer> mMusic;   // Gameplay music loop (Sound/back.mp3)
		Ref<SoundPlayer> mTown;    // Town ambience loop, always on (Sound/town.mp3)
		Ref<SoundPlayer> mEngine;  // Player car engine loop (Sound/engine.mp3)
		Ref<SoundPlayer> mTyres;   // Tyre screech loop while drifting (Sound/tyres.mp3)
		Ref<SoundPlayer> mWinning; // Win jingle (Sound/winning.mp3)
		Ref<SoundPlayer> mLoose;   // Lose jingle (Sound/loose.mp3)
		Ref<SoundPlayer> mBtn;     // Button click (Sound/btn.mp3)

		Vector<Ref<SoundPlayer>> mChips; // Token credit tick voices (Sound/chips.mp3)
		int mNextChipsVoice = 0;         // Round-robin cursor over the chips voices

	private:
		// Creates a player over the sound asset at the path
		static Ref<SoundPlayer> MakePlayer(const String& path, float volume, Loop loop);

		// Moves the loop volume toward the target, full swing in about 0.12 s
		static void RampVolume(const Ref<SoundPlayer>& player, float target, float dt);

		// Restarts a one-shot from zero; skipped while the effects are switched off
		void PlayOneShot(const Ref<SoundPlayer>& player);

		// Reapplies the switch-gated volumes to the one-shot players
		void ApplyVolumes();
	};
}
