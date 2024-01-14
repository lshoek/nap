#pragma once

// Local includes
#include "lightcomponent.h"

// External includes
#include <perspcameracomponent.h>

namespace nap
{
	// Forward declares
	class PointLightComponentInstance;

	/**
	 * Point light component for NAP RenderAdvanced's light system.
	 * 
	 * Omnidirectional light that emits from its origin. Therefore, ignores the `direction` uniform. The Render Advanced
	 * service creates and manages a `nap::CubeDepthRenderTarget` and `nap::DepthRenderTextureCube` for rendering this
	 * light's shadow maps.
	 */
	class NAPAPI PointLightComponent : public LightComponent
	{
		RTTI_ENABLE(LightComponent)
		DECLARE_COMPONENT(PointLightComponent, PointLightComponentInstance)
	public:
		ResourcePtr<ParameterEntryFloat> mAttenuation;			///< Property: 'Attenuation' The rate at which light intensity is lost over distance from the origin
		ComponentPtr<PerspCameraComponent> mShadowCamera;		///< Property: 'ShadowCamera' Camera that produces the depth texture for a directional light
		uint mShadowMapSize = 512U;								///< Property: 'ShadowMapSize' The horizontal and vertical dimension of the shadow map for this light
	};


	/**
	 * Point light component instance for NAP RenderAdvanced's light system.
	 *
	 * Omnidirectional light that emits from its origin. Therefore, ignores the `direction` uniform. The Render Advanced
	 * service creates and manages a `nap::CubeDepthRenderTarget` and `nap::DepthRenderTextureCube` for rendering this
	 * light's shadow maps.
	 */
	class NAPAPI PointLightComponentInstance : public LightComponentInstance
	{
		RTTI_ENABLE(LightComponentInstance)
	public:
		PointLightComponentInstance(EntityInstance& entity, Component& resource) :
			LightComponentInstance(entity, resource) { }

		// Destructor
		virtual ~PointLightComponentInstance() override { LightComponentInstance::removeLightComponent(); }

		/**
		 * Initialize LightComponentInstance based on the LightComponent resource
		 * @param entityCreationParams when dynamically creating entities on initialization, add them to this this list.
		 * @param errorState should hold the error message when initialization fails
		 * @return if the LightComponentInstance is initialized successfully
		 */
		virtual bool init(utility::ErrorState& errorState) override;

		/**
		 * @return whether this light component supports shadows
		 */
		virtual bool isShadowSupported() const override						{ return true; }

		/**
		 * @return the shadow camera if available, else nullptr
		 */
		virtual CameraComponentInstance* getShadowCamera() override			{ return mShadowCamera.get(); }

		/**
		 * @return the light type
		 */
		virtual ELightType getLightType() const	override					{ return ELightType::Point; }

		/**
		 * @return the shadow map type
		 */
		virtual EShadowMapType getShadowMapType() const override			{ return EShadowMapType::Cube; }

	private:
		// Shadow camera
		ComponentInstancePtr<PerspCameraComponent> mShadowCamera = { this, &PointLightComponent::mShadowCamera };
	};
}
