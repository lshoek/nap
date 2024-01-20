/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// Local includes
#include "fftutils.h"

// Nap includes
#include <component.h>
#include <parameternumeric.h>
#include <smoothdamp.h>

namespace nap
{
	class FluxMeasurementComponentInstance;
	class FFTAudioNodeComponentInstance;
			
	/**
	 * Component to measure flux of the audio signal from an @AudioComponentBase.
	 */
	class NAPAPI FluxMeasurementComponent : public Component
	{
		RTTI_ENABLE(Component)
		DECLARE_COMPONENT(FluxMeasurementComponent, FluxMeasurementComponentInstance)	
	public:
		/**
		 * FilterParameterItem
		 */
		class NAPAPI FilterParameterItem : public Resource
		{
			RTTI_ENABLE(Resource)
		public:
			bool init(utility::ErrorState& errorState) override { return true; }

			ResourcePtr<ParameterFloat> mParameter;
			ResourcePtr<ParameterFloat> mMultiplier;
			ResourcePtr<ParameterFloat> mOffset;
			ResourcePtr<ParameterFloat> mTargetOnset;
			ResourcePtr<ParameterFloat> mDecay;
			ResourcePtr<ParameterFloat> mStretch;

			float mMinHz = 0.0f;
			float mMaxHz = 44100.0f;
			float mOnsetImpact = 2.0f;
			float mSmoothTime = 0.05f;
			uint mEvaluationSampleCount = 1000;
		};

		// Constructor
		FluxMeasurementComponent() :
			Component() { }

		void getDependentComponents(std::vector<rtti::TypeInfo>& components) const override;

		std::vector<rtti::ObjectPtr<FilterParameterItem>> mParameters;
		bool mEnable = true;
	};
		
		
	/**
	 * Instance of component to measure onsets of the audio signal from an @AudioComponentBase.
	 * A specific frequency band to be measured can be specified.
	 */
	class NAPAPI FluxMeasurementComponentInstance : public ComponentInstance
	{
		RTTI_ENABLE(ComponentInstance)
	public:
		/**
		 * OnsetData
		 */
		class OnsetData
		{
			friend class FluxMeasurementComponentInstance;
		public:
			OnsetData(FluxMeasurementComponent::FilterParameterItem& item) :
				mParameter(*item.mParameter),
				mMultiplier(item.mMultiplier.get()),
				mOffset(item.mOffset.get()),
				mTargetOnset(item.mTargetOnset.get()),
				mDecay(item.mDecay.get()),
				mStretch(item.mStretch.get()),
				mOnsetImpact(item.mOnsetImpact),
				mOnsetSmoother({ 0.0f, item.mSmoothTime }),
				mMinHz(std::clamp(item.mMinHz, 0.0f, 44100.0f)),
				mMaxHz(std::clamp(item.mMaxHz, 0.0f, 44100.0f)),
				mEvaluationSampleCount(item.mEvaluationSampleCount)
			{ }

			// Exponential Moving Average
			float computeMovingAverage(float newValue, float& outAverage);

			ParameterFloat& mParameter;
			ParameterFloat* mMultiplier = nullptr;
			ParameterFloat* mOffset = nullptr;
			ParameterFloat* mDecay = nullptr;
			ParameterFloat* mTargetOnset = nullptr;
			ParameterFloat* mStretch = nullptr;

			float mMinHz;
			float mMaxHz;
			float mOnsetImpact;
			math::FloatSmoothOperator mOnsetSmoother;
			math::FloatSmoothOperator mStretchSmoother{ 1.0f, 0.5f };
			uint mEvaluationSampleCount;

		private:
			uint mSamplesEvaluated = 0;
			float mSampleAverage = 0.0f;
			float mOnsetValue = 0.0f;
			float mVelocity = 0.0f;
			float mAcceleration = 0.0f;
		};

		// Constructor
		FluxMeasurementComponentInstance(EntityInstance& entity, Component& resource) :
			ComponentInstance(entity, resource) {}

		// Initialize the component
		bool init(utility::ErrorState& errorState) override;

		/**
		 * Update this component
		 * @param deltaTime the time in between cooks in seconds
		 */
		virtual void update(double deltaTime) override;

		/**
		 * 
		 */
		const std::vector<rtti::ObjectPtr<FluxMeasurementComponent::FilterParameterItem>>& getParameterItems() const { return mResource->mParameters; }

	private:
		FluxMeasurementComponent* mResource = nullptr;
		FFTAudioNodeComponentInstance* mFFTAudioComponent = nullptr;

		std::vector<OnsetData> mOnsetList;

		FFTBuffer::AmplitudeSpectrum mPreviousBuffer;
		float mElapsedTime = 0.0f;
	};
}
