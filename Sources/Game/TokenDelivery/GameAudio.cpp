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

	// the filling stream ticks every ~0.12 s while a tick sounds ~0.5 s: four voices keep
	// the overlapping tails from cutting each other
	static const int kChipsVoices = 4;

	static const float kVolumeRampTime = 0.12f;

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
		mMusic = MakePlayer("Sound/back.mp3", 0.0f, Loop::Repeat);
		mTown = MakePlayer("Sound/town.mp3", 0.0f, Loop::Repeat);
		mEngine = MakePlayer("Sound/engine.mp3", 0.0f, Loop::Repeat);
		mTyres = MakePlayer("Sound/tyres.mp3", 0.0f, Loop::Repeat);
		mWinning = MakePlayer("Sound/winning.mp3", kWinVolume, Loop::None);
		mLoose = MakePlayer("Sound/loose.mp3", kLoseVolume, Loop::None);
		mBtn = MakePlayer("Sound/btn.mp3", kBtnVolume, Loop::None);

		mChips.Clear();
		for (int i = 0; i < kChipsVoices; i++)
			mChips.Add(MakePlayer("Sound/chips.mp3", kChipsVolume, Loop::None));

		// the loop beds play permanently, muted; audibility comes from the volume ramps
		mMusic->Play();
		mTown->Play();
		mEngine->Play();
		mTyres->Play();
	}

	void GameAudio::Update(float dt)
	{
		if (!mMusic)
			return;

		float gate = mSoundEnabled ? 1.0f : 0.0f;
		RampVolume(mMusic, mMusicEnabled && mMusicActive ? kMusicVolume : 0.0f, dt);
		RampVolume(mTown, kTownVolume*gate, dt);
		RampVolume(mEngine, mSpeedNorm > 0.02f ? kEngineVolume*gate : 0.0f, dt);
		RampVolume(mTyres, kTyresVolume*mDriftIntensity*gate, dt);
	}

	void GameAudio::SetSoundEnabled(bool enabled)
	{
		mSoundEnabled = enabled;
		ApplyVolumes();
	}

	void GameAudio::SetMusicEnabled(bool enabled)
	{
		mMusicEnabled = enabled;
	}

	void GameAudio::SetDriving(float speedNorm)
	{
		mSpeedNorm = Math::Clamp01(speedNorm);
		if (mEngine)
			mEngine->SetPitch(0.75f + 0.5f*mSpeedNorm);
	}

	void GameAudio::PlayButton()
	{
		PlayOneShot(mBtn);
	}

	void GameAudio::PlayChips()
	{
		if (mChips.IsEmpty())
			return;

		PlayOneShot(mChips[mNextChipsVoice]);
		mNextChipsVoice = (mNextChipsVoice + 1)%mChips.Count();
	}

	void GameAudio::PlayWin()
	{
		PlayOneShot(mWinning);
	}

	void GameAudio::PlayLose()
	{
		PlayOneShot(mLoose);
	}

	void GameAudio::RampVolume(const Ref<SoundPlayer>& player, float target, float dt)
	{
		float volume = player->GetVolume();
		float step = dt/kVolumeRampTime;
		player->SetVolume(volume + Math::Clamp(target - volume, -step, step));
	}

	void GameAudio::PlayOneShot(const Ref<SoundPlayer>& player)
	{
		if (!player || !mSoundEnabled)
			return;

		// the player time is never advanced (see the class header), so Play always starts
		// the backend from zero; Stop first so a retrigger passes the playing check
		player->Stop();
		player->Play();
	}

	void GameAudio::ApplyVolumes()
	{
		if (!mMusic)
			return;

		float gate = mSoundEnabled ? 1.0f : 0.0f;
		mWinning->SetVolume(kWinVolume*gate);
		mLoose->SetVolume(kLoseVolume*gate);
		mBtn->SetVolume(kBtnVolume*gate);
		for (auto& voice : mChips)
			voice->SetVolume(kChipsVolume*gate);
	}
}
