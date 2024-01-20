/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "fluxmeasurementcomponent.h"
#include "fftaudionodecomponent.h"
#include "fftutils.h"

// Nap includes
#include <entity.h>
#include <nap/core.h>

RTTI_BEGIN_CLASS(nap::FluxMeasurementComponent::FilterParameterItem)
	RTTI_PROPERTY("Parameter", &nap::FluxMeasurementComponent::FilterParameterItem::mParameter, nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Multiplier", &nap::FluxMeasurementComponent::FilterParameterItem::mMultiplier, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Offset", &nap::FluxMeasurementComponent::FilterParameterItem::mOffset, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("TargetOnset", &nap::FluxMeasurementComponent::FilterParameterItem::mTargetOnset, nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Decay", &nap::FluxMeasurementComponent::FilterParameterItem::mDecay, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Stretch", &nap::FluxMeasurementComponent::FilterParameterItem::mStretch, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("OnsetImpact", &nap::FluxMeasurementComponent::FilterParameterItem::mOnsetImpact, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("MinHertz", &nap::FluxMeasurementComponent::FilterParameterItem::mMinHz, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("MaxHertz", &nap::FluxMeasurementComponent::FilterParameterItem::mMaxHz, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("EvaluationSampleCount", &nap::FluxMeasurementComponent::FilterParameterItem::mEvaluationSampleCount, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("SmoothTime", &nap::FluxMeasurementComponent::FilterParameterItem::mSmoothTime, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS(nap::FluxMeasurementComponent)
	RTTI_PROPERTY("Parameters", &nap::FluxMeasurementComponent::mParameters, nap::rtti::EPropertyMetaData::Default | nap::rtti::EPropertyMetaData::Embedded)
	RTTI_PROPERTY("Enable", &nap::FluxMeasurementComponent::mEnable, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::FluxMeasurementComponentInstance)
	RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{
	//////////////////////////////////////////////////////////////////////////
	// Static
	//////////////////////////////////////////////////////////////////////////

	// Always ensure a decreasing gradient
	static const float sFluxEpsilon = 0.0001f;


	//////////////////////////////////////////////////////////////////////////
	// FluxMeasurementComponent
	//////////////////////////////////////////////////////////////////////////

	void FluxMeasurementComponent::getDependentComponents(std::vector<rtti::TypeInfo>& components) const
	{
		components.emplace_back(RTTI_OF(FFTAudioNodeComponent));
	}


	//////////////////////////////////////////////////////////////////////////
	// FluxMeasurementComponentInstance
	//////////////////////////////////////////////////////////////////////////

	bool FluxMeasurementComponentInstance::init(utility::ErrorState& errorState)
	{
		// Fetch resource
		mResource = getComponent<FluxMeasurementComponent>();

		// Ensure FFTAudioComponentInstance is available
		mFFTAudioComponent = &getEntityInstance()->getComponent<FFTAudioNodeComponentInstance>();
		if (!errorState.check(mFFTAudioComponent != nullptr, "Missing nap::FFTAudioComponentInstance under entity"))
			return false;

		const uint bin_count = mFFTAudioComponent->getFFTBuffer().getBinCount();
		mOnsetList.reserve(mResource->mParameters.size());
		for (auto& entry : mResource->mParameters)
		{
			if (!errorState.check(entry->mMinHz < entry->mMaxHz, "%s: Invalid filter parameter item. Minimum hertz higher than maximum hertz.", mResource->mID.c_str()))
				return false;

			mOnsetList.emplace_back(*entry);
		}

		mPreviousBuffer.resize(bin_count);
		return true;
	}


	void FluxMeasurementComponentInstance::update(double deltaTime)
	{
		if (!mResource->mEnable)
			return;

		const float delta_time = static_cast<float>(deltaTime);
		mElapsedTime += delta_time;

		// Fetch amplitudes
		const auto& amps = mFFTAudioComponent->getFFTBuffer().getAmplitudeSpectrum();

		for (auto& entry : mOnsetList)
		{
			const float interval = utility::interval(mFFTAudioComponent->getFFTBuffer().getBinCount()-1, 44100.0f);
			const uint min_bin = static_cast<uint>(entry.mMinHz / interval);
			const uint max_bin = static_cast<uint>(entry.mMaxHz / interval);

			float flux = utility::flux(amps, mPreviousBuffer, min_bin, max_bin);
			float mult = (entry.mMultiplier != nullptr) ? entry.mMultiplier->mValue : 1.0f;
			float raw_onset = flux * mult;
			float previous_onset = entry.mOnsetValue;
			
			// Acceleration
			if (raw_onset > previous_onset)
			{
				// Compute upwards force on acceleration proportionate to the difference in onset
				float diff = std::abs(raw_onset - previous_onset);
				entry.mAcceleration = (1.0f - std::pow(diff - 1.0f, 2.0f)) * entry.mOnsetImpact;
				entry.mVelocity = 0.0f;
			}
			else
			{
				float decay = (entry.mDecay != nullptr) ? entry.mDecay->mValue : 0.1f;
				entry.mAcceleration -= decay * delta_time * 1000.0f;
			}
			entry.mVelocity = std::max(entry.mVelocity + entry.mAcceleration * delta_time, -1000.0f);
			float max_onset = std::max(raw_onset, previous_onset);
			float onset = std::max(max_onset + entry.mVelocity * delta_time, 0.0f);

			// Compute stretch factor to normalize output to target average over a time period
			float stretch = 1.0f;
			if (entry.mStretch != nullptr)
			{
				float average_onset = std::max(entry.computeMovingAverage(onset, entry.mSampleAverage), glm::epsilon<float>()*2.0f);
				float target_onset = (entry.mTargetOnset != nullptr) ? entry.mTargetOnset->mValue : 0.25f;
				float factor = target_onset / average_onset;
				entry.mStretch->setValue(entry.mStretchSmoother.update(factor, delta_time));
				stretch = entry.mStretch->mValue;
			}

			entry.mOnsetValue = onset;
			float smooth_onset = entry.mOnsetSmoother.update(entry.mOnsetValue, delta_time);
			float stretch_onset = smooth_onset * stretch;
			float offset = (entry.mOffset != nullptr) ? entry.mOffset->mValue : 0.0f;
			entry.mParameter.setValue(stretch_onset + offset);
		}

		// Copy
		mPreviousBuffer = amps;
	}


	//////////////////////////////////////////////////////////////////////////
	// FluxMeasurementComponentInstance::OnsetData
	//////////////////////////////////////////////////////////////////////////

	float FluxMeasurementComponentInstance::OnsetData::computeMovingAverage(float value, float& outAverage)
	{
		if (mSamplesEvaluated < mEvaluationSampleCount)
		{
			float result = mSamplesEvaluated * outAverage + value;
			outAverage = result / (mSamplesEvaluated + 1.0f);
			++mSamplesEvaluated;
		}
		else
		{
			float mult = 2.0f / (mEvaluationSampleCount + 1.0f);
			outAverage = (value - outAverage) * mult + outAverage;
		}
		return outAverage;
	}
}
