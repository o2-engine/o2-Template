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
}

// The bank loads over the built Sound/*.mp3 assets; headless runs on the null backend
TEST(GameAudioBank, LoadsPlayersAndStartsTownAmbience)
{
	auto audio = MakeLoadedAudio();

	ASSERT_TRUE(audio->GetMusicPlayer());
	EXPECT_TRUE(audio->GetMusicPlayer()->GetSound());
	EXPECT_TRUE(audio->GetTownPlayer()->GetSound());
	EXPECT_TRUE(audio->GetEnginePlayer()->GetSound());
	EXPECT_TRUE(audio->GetTyresPlayer()->GetSound());
	EXPECT_TRUE(audio->GetWinPlayer()->GetSound());
	EXPECT_TRUE(audio->GetLosePlayer()->GetSound());
	EXPECT_TRUE(audio->GetChipsPlayer()->GetSound());
	EXPECT_TRUE(audio->GetButtonPlayer()->GetSound());

	EXPECT_TRUE(audio->GetTownPlayer()->IsPlaying());
	EXPECT_FALSE(audio->GetMusicPlayer()->IsPlaying());

	audio->Update(0.1f);
	EXPECT_TRUE(audio->GetTownPlayer()->IsPlaying());
}

TEST(GameAudioBank, MusicFollowsTheActiveGate)
{
	auto audio = MakeLoadedAudio();

	audio->SetMusicActive(true);
	EXPECT_TRUE(audio->GetMusicPlayer()->IsPlaying());

	audio->SetMusicActive(false);
	EXPECT_FALSE(audio->GetMusicPlayer()->IsPlaying());

	audio->SetMusicActive(true);
	EXPECT_TRUE(audio->GetMusicPlayer()->IsPlaying());
}

TEST(GameAudioBank, MusicSwitchMutesButKeepsPlaying)
{
	auto audio = MakeLoadedAudio();
	audio->SetMusicActive(true);

	audio->SetMusicEnabled(false);
	EXPECT_FALSE(audio->IsMusicEnabled());
	EXPECT_TRUE(audio->GetMusicPlayer()->IsPlaying());
	EXPECT_NEAR(audio->GetMusicPlayer()->GetVolume(), 0.0f, 0.001f);

	audio->SetMusicEnabled(true);
	EXPECT_GT(audio->GetMusicPlayer()->GetVolume(), 0.0f);
}

TEST(GameAudioBank, SoundSwitchGatesLoopsAndOneShots)
{
	auto audio = MakeLoadedAudio();

	audio->SetSoundEnabled(false);
	EXPECT_FALSE(audio->IsSoundEnabled());
	EXPECT_NEAR(audio->GetTownPlayer()->GetVolume(), 0.0f, 0.001f);

	audio->PlayButton();
	EXPECT_FALSE(audio->GetButtonPlayer()->IsPlaying());

	audio->SetSoundEnabled(true);
	EXPECT_GT(audio->GetTownPlayer()->GetVolume(), 0.0f);

	audio->PlayButton();
	EXPECT_TRUE(audio->GetButtonPlayer()->IsPlaying());

	// the sound switch does not touch the music channel
	EXPECT_GT(audio->GetMusicPlayer()->GetVolume(), 0.0f);
}

TEST(GameAudioBank, EngineFollowsSpeed)
{
	auto audio = MakeLoadedAudio();

	audio->SetDriving(0.5f);
	EXPECT_TRUE(audio->GetEnginePlayer()->IsPlaying());
	EXPECT_NEAR(audio->GetEnginePlayer()->GetPitch(), 1.0f, 0.001f);

	audio->SetDriving(1.0f);
	EXPECT_NEAR(audio->GetEnginePlayer()->GetPitch(), 1.25f, 0.001f);

	audio->SetDriving(0.0f);
	EXPECT_FALSE(audio->GetEnginePlayer()->IsPlaying());
}

TEST(GameAudioBank, TyresFollowDriftIntensity)
{
	auto audio = MakeLoadedAudio();

	audio->SetDrift(1.0f);
	EXPECT_TRUE(audio->GetTyresPlayer()->IsPlaying());
	EXPECT_GT(audio->GetTyresPlayer()->GetVolume(), 0.0f);

	audio->SetDrift(0.5f);
	EXPECT_TRUE(audio->GetTyresPlayer()->IsPlaying());

	audio->SetDrift(0.0f);
	EXPECT_FALSE(audio->GetTyresPlayer()->IsPlaying());
	EXPECT_NEAR(audio->GetTyresPlayer()->GetVolume(), 0.0f, 0.001f);
}

TEST(GameAudioBank, OneShotFinishesAndRetriggers)
{
	auto audio = MakeLoadedAudio();

	audio->PlayChips();
	ASSERT_TRUE(audio->GetChipsPlayer()->IsPlaying());

	float duration = audio->GetChipsPlayer()->GetDuration();
	ASSERT_GT(duration, 0.0f);
	audio->Update(duration + 0.1f);
	EXPECT_FALSE(audio->GetChipsPlayer()->IsPlaying());

	audio->PlayChips();
	EXPECT_TRUE(audio->GetChipsPlayer()->IsPlaying());
	EXPECT_NEAR(audio->GetChipsPlayer()->GetTime(), 0.0f, 0.001f);
}
