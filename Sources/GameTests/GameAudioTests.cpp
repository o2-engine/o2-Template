#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "TokenDelivery/GameAudio.h"
#include "o2/Sound/SoundSystem.h"

using namespace o2;
using namespace td;

namespace
{
	Ref<GameAudio> MakeLoadedAudio()
	{
		auto audio = mmake<GameAudio>();
		audio->Load();
		return audio;
	}

	// a single big-dt update converges every volume ramp to its target
	void SettleVolumes(const Ref<GameAudio>& audio)
	{
		audio->Update(1.0f);
	}
}

// The bank loads over the built Sound/*.mp3 assets; headless runs on the null backend.
// A loop bed runs only while it is audible: a silent one keeps decoding its mp3 for nothing
TEST(GameAudioBank, LoadsPlayersAndRunsOnlyAudibleLoopBeds)
{
	auto audio = MakeLoadedAudio();

	ASSERT_TRUE(audio->GetMusicPlayer());
	EXPECT_TRUE(audio->GetMusicPlayer()->GetSound());
	EXPECT_TRUE(audio->GetTownPlayer()->GetSound());
	EXPECT_TRUE(audio->GetEnginePlayer()->GetSound());
	EXPECT_TRUE(audio->GetTyresPlayer()->GetSound());
	EXPECT_TRUE(audio->GetWinPlayer()->GetSound());
	EXPECT_TRUE(audio->GetLosePlayer()->GetSound());
	EXPECT_TRUE(audio->GetButtonPlayer()->GetSound());
	ASSERT_FALSE(audio->GetChipsPlayers().IsEmpty());
	EXPECT_TRUE(audio->GetChipsPlayers()[0]->GetSound());

	// town ambience fades in on its own, the music waits for the active gate, the driving beds
	// wait for the car
	SettleVolumes(audio);
	EXPECT_GT(audio->GetTownPlayer()->GetVolume(), 0.0f);
	EXPECT_TRUE(audio->GetTownPlayer()->IsPlaying());

	EXPECT_NEAR(audio->GetMusicPlayer()->GetVolume(), 0.0f, 0.001f);
	EXPECT_FALSE(audio->GetMusicPlayer()->IsPlaying());
	EXPECT_FALSE(audio->GetEnginePlayer()->IsPlaying());
	EXPECT_FALSE(audio->GetTyresPlayer()->IsPlaying());
}

TEST(GameAudioBank, MusicFollowsTheActiveGate)
{
	auto audio = MakeLoadedAudio();

	audio->SetMusicActive(true);
	SettleVolumes(audio);
	EXPECT_GT(audio->GetMusicPlayer()->GetVolume(), 0.0f);

	audio->SetMusicActive(false);
	SettleVolumes(audio);
	EXPECT_NEAR(audio->GetMusicPlayer()->GetVolume(), 0.0f, 0.001f);
	EXPECT_FALSE(audio->GetMusicPlayer()->IsPlaying()); // muted bed stops decoding

	audio->SetMusicActive(true);
	SettleVolumes(audio);
	EXPECT_GT(audio->GetMusicPlayer()->GetVolume(), 0.0f);
	EXPECT_TRUE(audio->GetMusicPlayer()->IsPlaying());
}

TEST(GameAudioBank, MusicSwitchMutesTheActiveMusic)
{
	auto audio = MakeLoadedAudio();
	audio->SetMusicActive(true);
	SettleVolumes(audio);
	ASSERT_GT(audio->GetMusicPlayer()->GetVolume(), 0.0f);

	audio->SetMusicEnabled(false);
	SettleVolumes(audio);
	EXPECT_FALSE(audio->IsMusicEnabled());
	EXPECT_NEAR(audio->GetMusicPlayer()->GetVolume(), 0.0f, 0.001f);
	EXPECT_FALSE(audio->GetMusicPlayer()->IsPlaying());

	audio->SetMusicEnabled(true);
	SettleVolumes(audio);
	EXPECT_GT(audio->GetMusicPlayer()->GetVolume(), 0.0f);
}

TEST(GameAudioBank, SoundSwitchGatesLoopsAndOneShots)
{
	auto audio = MakeLoadedAudio();
	audio->SetMusicActive(true);

	audio->SetSoundEnabled(false);
	SettleVolumes(audio);
	EXPECT_FALSE(audio->IsSoundEnabled());
	EXPECT_NEAR(audio->GetTownPlayer()->GetVolume(), 0.0f, 0.001f);
	EXPECT_NEAR(audio->GetButtonPlayer()->GetVolume(), 0.0f, 0.001f);

	audio->PlayButton();
	EXPECT_FALSE(audio->GetButtonPlayer()->IsPlaying());

	// the sound switch does not touch the music channel
	EXPECT_GT(audio->GetMusicPlayer()->GetVolume(), 0.0f);

	audio->SetSoundEnabled(true);
	SettleVolumes(audio);
	EXPECT_GT(audio->GetTownPlayer()->GetVolume(), 0.0f);

	audio->PlayButton();
	EXPECT_TRUE(audio->GetButtonPlayer()->IsPlaying());
}

TEST(GameAudioBank, EngineFollowsSpeed)
{
	auto audio = MakeLoadedAudio();

	audio->SetDriving(0.5f);
	SettleVolumes(audio);
	EXPECT_GT(audio->GetEnginePlayer()->GetVolume(), 0.0f);
	EXPECT_NEAR(audio->GetEnginePlayer()->GetPitch(), 1.0f, 0.001f);

	audio->SetDriving(1.0f);
	EXPECT_NEAR(audio->GetEnginePlayer()->GetPitch(), 1.25f, 0.001f);

	audio->SetDriving(0.0f);
	SettleVolumes(audio);
	EXPECT_NEAR(audio->GetEnginePlayer()->GetVolume(), 0.0f, 0.001f);
	EXPECT_FALSE(audio->GetEnginePlayer()->IsPlaying());
}

TEST(GameAudioBank, TyresFollowDriftIntensity)
{
	auto audio = MakeLoadedAudio();

	audio->SetDrift(1.0f);
	SettleVolumes(audio);
	float full = audio->GetTyresPlayer()->GetVolume();
	EXPECT_GT(full, 0.0f);

	audio->SetDrift(0.5f);
	SettleVolumes(audio);
	EXPECT_NEAR(audio->GetTyresPlayer()->GetVolume(), full*0.5f, 0.001f);

	audio->SetDrift(0.0f);
	SettleVolumes(audio);
	EXPECT_NEAR(audio->GetTyresPlayer()->GetVolume(), 0.0f, 0.001f);
}

TEST(GameAudioBank, VolumeRampIsGradual)
{
	auto audio = MakeLoadedAudio();

	// one 60 fps frame moves the town fade only part of the way to its target
	audio->Update(1.0f/60.0f);
	float step = audio->GetTownPlayer()->GetVolume();
	EXPECT_GT(step, 0.0f);

	SettleVolumes(audio);
	EXPECT_GT(audio->GetTownPlayer()->GetVolume(), step);
}

// The filling stream ticks faster than one tick sounds: the voices rotate so a new tick
// does not cut the previous one
TEST(GameAudioBank, ChipsRotateVoices)
{
	auto audio = MakeLoadedAudio();
	auto& voices = audio->GetChipsPlayers();
	ASSERT_GE(voices.Count(), 2);

	audio->PlayChips();
	audio->PlayChips();
	EXPECT_TRUE(voices[0]->IsPlaying());
	EXPECT_TRUE(voices[1]->IsPlaying());

	// the rotation wraps around and retriggers the first voice without a crash
	for (int i = 2; i < voices.Count() + 1; i++)
		audio->PlayChips();
	EXPECT_TRUE(voices[0]->IsPlaying());
}
