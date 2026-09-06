/*
SoLoud audio engine
Copyright (c) 2013-2015 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#include <string.h>
#include "soloud_internal.h"

// Core "basic" operations - play, stop, etc

namespace SoLoud
{
	handle Soloud::play(AudioSource &aSound, float aVolume, float aPan, bool aPaused, unsigned int aBus)
	{
		if (aSound.mFlags & AudioSource::SINGLE_INSTANCE)
		{
			// Only one instance allowed, stop others
			aSound.stop();
		}

		// Creation of an audio instance may take significant amount of time,
		// so let's not do it inside the audio thread mutex.
		aSound.mSoloud = this;
		SoLoud::AudioSourceInstance *instance = aSound.createInstance();
		return playPrepared(aSound, instance, aVolume, aPan, aPaused, aBus);
	}

	handle Soloud::playPrepared(AudioSource &aSound, AudioSourceInstance *instance,
		float aVolume, float aPan, bool aPaused, unsigned int aBus)
	{
		if (!instance) return 0;
		aSound.mSoloud = this;

		lockAudioMutex_internal();
		int ch = findFreeVoice_internal();
		if (ch < 0) 
		{
			unlockAudioMutex_internal();
			delete instance;
			return UNKNOWN_ERROR;
		}
		if (!aSound.mAudioSourceID)
		{
			aSound.mAudioSourceID = mAudioSourceID;
			mAudioSourceID++;
		}
		mVoice[ch] = instance;
		mVoice[ch]->mAudioSourceID = aSound.mAudioSourceID;
		mVoice[ch]->mBusHandle = aBus;
		mVoice[ch]->init(aSound, mPlayIndex);
		m3dData[ch].init(aSound);

		mPlayIndex++;

		// 20 bits, skip the last one (top bits full = voice group)
		if (mPlayIndex == 0xfffff) 
		{
			mPlayIndex = 0;
		}

		if (aPaused)
		{
			mVoice[ch]->mFlags |= AudioSourceInstance::PAUSED;
		}

		setVoicePan_internal(ch, aPan);
		if (aVolume < 0)
		{
			setVoiceVolume_internal(ch, aSound.mVolume);
		}
		else
		{
			setVoiceVolume_internal(ch, aVolume);
		}

		// Fix initial voice volume ramp up		
		int i;
		for (i = 0; i < MAX_CHANNELS; i++)
		{
			mVoice[ch]->mCurrentChannelVolume[i] = mVoice[ch]->mChannelVolume[i] * mVoice[ch]->mOverallVolume;
		}

		setVoiceRelativePlaySpeed_internal(ch, 1);
		
		for (i = 0; i < FILTERS_PER_STREAM; i++)
		{
			if (aSound.mFilter[i])
			{
				mVoice[ch]->mFilter[i] = aSound.mFilter[i]->createInstance();
			}
		}

		mActiveVoiceDirty = true;

		unlockAudioMutex_internal();

		int handle = getHandleFromVoice_internal(ch);
		return handle;
	}

	handle Soloud::playClocked(time aSoundTime, AudioSource &aSound, float aVolume, float aPan, unsigned int aBus)
	{
		handle h = play(aSound, aVolume, aPan, 1, aBus);
		lockAudioMutex_internal();
		// mLastClockedTime is cleared to zero at start of every output buffer
		time lasttime = mLastClockedTime;
		if (lasttime == 0)
		{
			mLastClockedTime = aSoundTime;
			lasttime = aSoundTime;
		}
		unlockAudioMutex_internal();
		int samples = (int)floor((aSoundTime - lasttime) * mSamplerate);
		// Make sure we don't delay too much (or overflow)
		if (samples < 0 || samples > 2048)		
			samples = 0;
		setDelaySamples(h, samples);
		setPause(h, 0);
		return h;
	}

	handle Soloud::playBackground(AudioSource &aSound, float aVolume, bool aPaused, unsigned int aBus)
	{
		handle h = play(aSound, aVolume, 0.0f, aPaused, aBus);
		setPanAbsolute(h, 1.0f, 1.0f);
		return h;
	}

	result Soloud::seek(handle aVoiceHandle, time aSeconds)
	{
		result res = SO_NO_ERROR;
		result singleres = SO_NO_ERROR;
		FOR_ALL_VOICES_PRE
			singleres = mVoice[ch]->seek(aSeconds, mScratch.mData, mScratchSize);
			if (singleres == SO_NO_ERROR)
			{
				const double frame = floor(mVoice[ch]->mStreamPosition * mVoice[ch]->mBaseSamplerate);
				mVoice[ch]->mRestoreClockValid = frame >= 0 && frame < double(UINT64_MAX >> 20);
				if (mVoice[ch]->mRestoreClockValid)
					mVoice[ch]->mSourceFrameFixed = uint64_t(frame) << 20;
			}
		if (singleres != SO_NO_ERROR)
			res = singleres;
		FOR_ALL_VOICES_POST
		return res;
	}


	static void clearVoiceResampler(AudioSourceInstance *voice)
	{
		voice->mLeftoverSamples = 0;
		voice->mSrcOffset = 0;
		for (unsigned int buffer = 0; buffer < 2; ++buffer)
		{
			if (voice->mResampleData[buffer])
				memset(voice->mResampleData[buffer], 0,
					sizeof(float) * SAMPLE_GRANULARITY * MAX_CHANNELS);
		}
	}

	void Soloud::clearResamplerBuffers(handle aVoiceHandle)
	{
		// FOR_ALL_VOICES holds the audio mutex, including handle validation.
		FOR_ALL_VOICES_PRE
			clearVoiceResampler(mVoice[ch]);
		FOR_ALL_VOICES_POST
	}

	bool Soloud::getVoiceFrameCursor(handle aVoiceHandle, VoiceFrameCursor &aCursor)
	{
		lockAudioMutex_internal();
		const int ch = getVoiceFromHandle_internal(aVoiceHandle);
		if (ch < 0 || !mVoice[ch]->mRestoreClockValid || mVoice[ch]->mParentClockDelayFrames
			|| !(mVoice[ch]->mBaseSamplerate >= 1)
			|| double(mVoice[ch]->mBaseSamplerate) > UINT32_MAX || !mSamplerate)
		{
			unlockAudioMutex_internal();
			return false;
		}
		const AudioSourceInstance *voice = mVoice[ch];
		aCursor.frame = voice->mSourceFrameFixed >> 20;
		aCursor.fraction = unsigned(voice->mSourceFrameFixed & ((1u << 20) - 1));
		aCursor.sourceRate = unsigned(voice->mBaseSamplerate);
		aCursor.outputRate = mSamplerate;
		aCursor.volume = voice->mSetVolume;
		aCursor.looping = (voice->mFlags & AudioSourceInstance::LOOPING) != 0;
		unlockAudioMutex_internal();
		return true;
	}

	result Soloud::startBusVoice(handle aVoiceHandle, Bus &aBus)
	{
		lockAudioMutex_internal();
		const int ch = getVoiceFromHandle_internal(aVoiceHandle);
		const int bus = ch >= 0 ? getVoiceFromHandle_internal(mVoice[ch]->mBusHandle) : -1;
		if (ch < 0 || bus < 0 || mVoice[bus] != aBus.mInstance)
		{
			unlockAudioMutex_internal();
			return INVALID_PARAMETER;
		}
		AudioSourceInstance *voice = mVoice[ch];
		AudioSourceInstance *parent = mVoice[bus];
		aBus.mChannelHandle = voice->mBusHandle;
		bool idle = true;
		for (unsigned int other = 0; other < mHighestVoice; ++other)
			if (other != unsigned(ch) && mVoice[other] && mVoice[other]->mBusHandle == voice->mBusHandle)
				idle = false;
		if (idle) clearVoiceResampler(parent);
		if (parent->mSamplerate != mSamplerate || parent->mBusHandle != 0)
			voice->mRestoreClockValid = false;
		else voice->mParentClockDelayFrames = parent->mLeftoverSamples;
		setVoicePause_internal(ch, (parent->mFlags & AudioSourceInstance::PAUSED) != 0);
		unlockAudioMutex_internal();
		return SO_NO_ERROR;
	}

	bool Soloud::hasConsumedVoiceTail(handle aVoiceHandle, uint64_t aSourceFrames)
	{
		lockAudioMutex_internal();
		const int ch = getVoiceFromHandle_internal(aVoiceHandle);
		bool ended = ch < 0;
		if (ch >= 0 && aSourceFrames && aSourceFrames < (UINT64_MAX >> 20) - 2)
		{
			const AudioSourceInstance *voice = mVoice[ch];
			const float ratio = voice->mSamplerate / float(mSamplerate);
			if (!(voice->mFlags & AudioSourceInstance::LOOPING) && ratio > 0 && ratio < 4096
				&& !voice->mParentClockDelayFrames && mVoice[ch]->hasEnded())
			{
				const uint64_t step = uint64_t(floor(double(ratio) * (1u << 20)));
				const uint64_t end = (aSourceFrames + 1) * (uint64_t(1) << 20) + step;
				ended = step && voice->mSourceFrameFixed >= end;
			}
		}
		unlockAudioMutex_internal();
		return ended;
	}

	result Soloud::primeRestoredVoice(handle aVoiceHandle, Bus &aBus, unsigned int aFrames,
		unsigned int aInitialFraction, const VoiceFrameCursor &aTarget)
	{
		// All allocation is outside the device lock. Work under it is bounded to
		// a short history window; this does not run mix_internal/global faders.
		if (!aTarget.sourceRate || !aTarget.outputRate || aFrames > 4096 || aInitialFraction >= (1u << 20)
			|| aTarget.fraction >= (1u << 20) || aTarget.frame > (UINT64_MAX >> 20)) return INVALID_PARAMETER;
		AlignedFloatBuffer output, scratch;
		if (output.init(SAMPLE_GRANULARITY * MAX_CHANNELS) != SO_NO_ERROR
			|| scratch.init(SAMPLE_GRANULARITY * MAX_CHANNELS) != SO_NO_ERROR) return OUT_OF_MEMORY;
		lockAudioMutex_internal();
		const int ch = getVoiceFromHandle_internal(aVoiceHandle);
		const int bus = ch >= 0 ? getVoiceFromHandle_internal(mVoice[ch]->mBusHandle) : -1;
		if (ch < 0 || bus < 0 || mVoice[bus] != aBus.mInstance || aTarget.outputRate != mSamplerate
			|| aTarget.sourceRate != mVoice[ch]->mBaseSamplerate || mResampler != RESAMPLER_LINEAR
			|| aBus.mResampler != RESAMPLER_LINEAR || mVoice[bus]->mSamplerate != mSamplerate)
		{
			unlockAudioMutex_internal();
			return INVALID_PARAMETER;
		}
		AudioSourceInstance *voice = mVoice[ch];
		AudioSourceInstance *busVoice = mVoice[bus];
		const bool busPaused = (busVoice->mFlags & AudioSourceInstance::PAUSED) != 0;
		const bool noAutostop = (voice->mFlags & AudioSourceInstance::DISABLE_AUTOSTOP) != 0;
		voice->mFlags |= AudioSourceInstance::DISABLE_AUTOSTOP;
		aBus.mChannelHandle = voice->mBusHandle;
		setVoicePause_internal(ch, false);
		setVoicePause_internal(bus, false);
		calcActiveVoices_internal();
		if (!voice->mResampleData[0] || !busVoice->mResampleData[0])
		{
			setVoicePause_internal(ch, true);
			setVoicePause_internal(bus, busPaused);
			if (!noAutostop) voice->mFlags &= ~AudioSourceInstance::DISABLE_AUTOSTOP;
			unlockAudioMutex_internal();
			return OUT_OF_MEMORY;
		}
		clearVoiceResampler(voice);
		clearVoiceResampler(busVoice);
		// The first source block load subtracts one block from this offset,
		// leaving the explicitly prepared fractional phase intact.
		voice->mSrcOffset = SAMPLE_GRANULARITY * (1u << 20) + aInitialFraction;
		for (unsigned int offset = 0; offset < aFrames;)
		{
			const unsigned int count = (aFrames - offset > SAMPLE_GRANULARITY) ? SAMPLE_GRANULARITY : aFrames - offset;
			mixBus_internal(output.mData, count, SAMPLE_GRANULARITY, scratch.mData,
				busVoice->mBusHandle, float(mSamplerate), mChannels, mResampler, busVoice);
			offset += count;
		}
		voice->mSourceFrameFixed = (aTarget.frame << 20) + aTarget.fraction;
		voice->mRestoreClockValid = true;
		voice->mParentClockDelayFrames = 0;
		voice->mStreamPosition = (double(aTarget.frame) + double(aTarget.fraction) / (1u << 20)) / aTarget.sourceRate;
		setVoicePause_internal(ch, busPaused);
		setVoicePause_internal(bus, busPaused);
		if (!noAutostop) voice->mFlags &= ~AudioSourceInstance::DISABLE_AUTOSTOP;
		unlockAudioMutex_internal();
		return SO_NO_ERROR;
	}

	void Soloud::stop(handle aVoiceHandle)
	{
		FOR_ALL_VOICES_PRE
			stopVoice_internal(ch);
		FOR_ALL_VOICES_POST
	}

	void Soloud::stopAudioSource(AudioSource &aSound)
	{
		if (aSound.mAudioSourceID)
		{
			lockAudioMutex_internal();
			
			int i;
			for (i = 0; i < (signed)mHighestVoice; i++)
			{
				if (mVoice[i] && mVoice[i]->mAudioSourceID == aSound.mAudioSourceID)
				{
					stopVoice_internal(i);
				}
			}
			unlockAudioMutex_internal();
		}
	}

	void Soloud::stopAll()
	{
		int i;
		lockAudioMutex_internal();
		for (i = 0; i < (signed)mHighestVoice; i++)
		{
			stopVoice_internal(i);
		}
		unlockAudioMutex_internal();
	}

	int Soloud::countAudioSource(AudioSource &aSound)
	{
		int count = 0;
		if (aSound.mAudioSourceID)
		{
			lockAudioMutex_internal();

			int i;
			for (i = 0; i < (signed)mHighestVoice; i++)
			{
				if (mVoice[i] && mVoice[i]->mAudioSourceID == aSound.mAudioSourceID)
				{
					count++;
				}
			}
			unlockAudioMutex_internal();
		}
		return count;
	}

}
