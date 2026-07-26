#include "o2/stdafx.h"
#include "TokenDelivery/GameAudio.h"

#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/SoundAsset.h"

namespace td
{
	static const float kMusicVolume = 0.45f;
	static const float kTownVolume = 0.3f;
	static const float kEngineVolume = 0.35f;
	static const float kTyresVolume = 0.6f;
	static const float kWinVolume = 0.8f;
	static const float kLoseVolume = 0.8f;
	static const float kChipsVolume = 0.55f;
	static const float kBtnVolume = 0.7f;

	Ref<SoundPlayer> GameAudio::MakePlayer(const String& path, float volume, Loop loop)
	{
		auto player = mmake<SoundPlayer>();
		player->SetSound(o2Assets.GetAssetRefByType<SoundAsset>(path));
		player->SetVolume(volume);
		player->SetLoop(loop);
		return player;
	}

	void GameAudio::Load()
	{
		mMusic = MakePlayer("Sound/back.mp3", kMusicVolume, Loop::Repeat);
		mTown = MakePlayer("Sound/town.mp3", kTownVolume, Loop::Repeat);
		mEngine = MakePlayer("Sound/engine.mp3", kEngineVolume, Loop::Repeat);
		mTyres = MakePlayer("Sound/tyres.mp3", 0.0f, Loop::Repeat);
		mWinning = MakePlayer("Sound/winning.mp3", kWinVolume, Loop::None);
		mLoose = MakePlayer("Sound/loose.mp3", kLoseVolume, Loop::None);
		mChips = MakePlayer("Sound/chips.mp3", kChipsVolume, Loop::None);
		mBtn = MakePlayer("Sound/btn.mp3", kBtnVolume, Loop::None);

		mPlayers = { mMusic, mTown, mEngine, mTyres, mWinning, mLoose, mChips, mBtn };

		mTown->Play();
	}

	void GameAudio::Update(float dt)
	{
		for (auto& player : mPlayers)
			player->Update(dt);
	}

	void GameAudio::SetSoundEnabled(bool enabled)
	{
		mSoundEnabled = enabled;
		ApplyVolumes();
	}

	void GameAudio::SetMusicEnabled(bool enabled)
	{
		mMusicEnabled = enabled;
		ApplyVolumes();
	}

	void GameAudio::SetMusicActive(bool active)
	{
		if (mMusic)
			SetLooping(mMusic, active);
	}

	void GameAudio::SetDriving(float speedNorm)
	{
		if (!mEngine)
			return;

		mEngine->SetPitch(0.75f + 0.5f*Math::Clamp01(speedNorm));
		SetLooping(mEngine, speedNorm > 0.02f);
	}

	void GameAudio::SetDrift(float intensity)
	{
		if (!mTyres)
			return;

		mDriftIntensity = Math::Clamp01(intensity);
		mTyres->SetVolume(mSoundEnabled ? kTyresVolume*mDriftIntensity : 0.0f);
		SetLooping(mTyres, mDriftIntensity > 0.1f);
	}

	void GameAudio::PlayButton()
	{
		PlayOneShot(mBtn);
	}

	void GameAudio::PlayChips()
	{
		PlayOneShot(mChips);
	}

	void GameAudio::PlayWin()
	{
		PlayOneShot(mWinning);
	}

	void GameAudio::PlayLose()
	{
		PlayOneShot(mLoose);
	}

	void GameAudio::SetLooping(const Ref<SoundPlayer>& player, bool playing)
	{
		if (playing && !player->IsPlaying())
			player->Play();
		else if (!playing && player->IsPlaying())
			player->Stop();
	}

	void GameAudio::PlayOneShot(const Ref<SoundPlayer>& player)
	{
		if (player && mSoundEnabled)
			player->RewindAndPlay();
	}

	void GameAudio::ApplyVolumes()
	{
		if (!mMusic)
			return;

		mMusic->SetVolume(mMusicEnabled ? kMusicVolume : 0.0f);

		float gate = mSoundEnabled ? 1.0f : 0.0f;
		mTown->SetVolume(kTownVolume*gate);
		mEngine->SetVolume(kEngineVolume*gate);
		mTyres->SetVolume(kTyresVolume*mDriftIntensity*gate);
		mWinning->SetVolume(kWinVolume*gate);
		mLoose->SetVolume(kLoseVolume*gate);
		mChips->SetVolume(kChipsVolume*gate);
		mBtn->SetVolume(kBtnVolume*gate);
	}
}
