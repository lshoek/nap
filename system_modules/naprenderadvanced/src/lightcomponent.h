#pragma once

// External includes
#include <component.h>
#include <componentptr.h>
#include <cameracomponent.h>
#include <transformcomponent.h>

#include <parameterentrynumeric.h>
#include <parameterentrycolor.h>

namespace nap
{
	// Forward declares
	class LightComponentInstance;

	/**
	 * Light Type Flag
	 */
	enum class ELightType : uint8
	{
		Custom			= 0,
		Directional		= 1,
		Point			= 2,
		Spot			= 3
	};

	/**
	 * Shadow Map Type
	 */
	enum class EShadowMapType : uint8
	{
		Quad = 0,
		Cube = 1
	};

	/**
	 * Light Component default light uniforms
	 */
	namespace uniform
	{
		inline constexpr const char* lightStruct = "light";						// Default light UBO struct name
		inline constexpr const char* shadowStruct = "shadow";					// Default shadow UBO struct name

		namespace light
		{
			inline constexpr const char* color = "color";
			inline constexpr const char* intensity = "intensity";
			inline constexpr const char* origin = "origin";
			inline constexpr const char* direction = "direction";
			inline constexpr const char* attenuation = "attenuation";
			inline constexpr const char* angle = "angle";
			inline constexpr const char* falloff = "falloff";
			inline constexpr const char* flags = "flags";
			inline constexpr const char* lights = "lights";
			inline constexpr const char* count = "count";
			inline constexpr const uint defaultMemberCount = 8;
		}

		namespace shadow
		{
			inline constexpr const char* lightViewProjectionMatrix = "lightViewProjectionMatrix";
            inline constexpr const char* nearFar = "nearFar";
            inline constexpr const char* strength = "strength";
            inline constexpr const char* flags = "flags";
			inline constexpr const char* count = "count";
		}
	}

	/**
	 * Light Component default shadow samplers
	 */
	namespace sampler
	{
		namespace light
		{
			inline constexpr const char* shadowMaps = "shadowMaps";
			inline constexpr const char* cubeShadowMaps = "cubeShadowMaps";
		}
	}

	using LightUniformDataMap = std::unordered_map<std::string, Parameter*>;
	using LightParameterList = std::vector<std::unique_ptr<Parameter>>;

	/**
	 * Base class of light components for NAP RenderAdvanced's light system.
	 *
	 * When present in the scene, the render advanced service can update light uniform data for material instances that are
	 * compatible with the light's shader interface. On initialization, each light component sets up its own registry of
	 * light uniform data and registers itself at the render advanced service. This way, the service is aware of the lights
	 * in the scene and creates the necessary resources for light information and shadow maps. NAP supports a limited
	 * number of lights per scene (`RenderAdvancedService::getMaximumLightCount`). The way in which these blend/interact
	 * depends on the implementation of the shader program. Increasing the maximum number of lights is trivial, however,
	 * with the current implementation it would take up more shader resource slots.
	 *
	 * Each light component has three default uniforms that are set by the RenderAdvanced service:
	 * - `origin`: `vec3` world position of the light.
	 * - `direction`: `vec3` direction of the light. Some lights may choose to ignore this however (e.g. point lights).
	 * - `flags`: an unsigned integer encoding information such as whether shadows are enabled, see `lightflags.h`.
	 *
	 * Other uniforms may be defined by derived light types. They must be in accordance with the data and shader interface
	 * in the `light.glslinc` file in the RenderAdvanced shader folder. New light types can be added here in the future,
	 * or user implementations can use the 'Custom' enum.
	 * 
	 * NAP comes with a default nap::BlinnPhongShader that is compatible with the light system. Hooking this up to a
	 * nap::Material allows for quick scene lighting setups. Material surface uniforms as defined by the shader interface
	 * must be set in data or at runtime. A description of these can be found in the documentation of the shader or its
	 * source file.
	 *
	 * The depth format of shadow maps can be configured in the `nap::RenderAdvancedServiceConfiguration`.
	 *
	 * Rendering with lights requires an additional call to the render advanced service. You can either use `pushLights` on
	 * the render components you wish to render or `renderShadows` with the `updateMaterials` argument set to `true` if you
	 * wish to use shadows too.
	 * 
	 * Update light uniforms of lit components when shadows are disabled.
	 * ~~~~~{.cpp}
	 *	mRenderAdvancedService->pushLights(components_to_render, error_state);
	 *	// mRenderService->renderObjects ...
	 * ~~~~~
	 *
	 * Re-render shadow map and update light uniforms.
	 * ~~~~~{.cpp}
	 *	if (mRenderService->beginHeadlessRecording())
	 *	{
	 *		mRenderAdvancedService->renderShadows(render_comps, true);
	 *		mRenderService->endHeadlessRecording();
	 *	}
	 * ~~~~~
	 */
	class NAPAPI LightComponent : public Component
	{
		RTTI_ENABLE(Component)
		DECLARE_COMPONENT(LightComponent, LightComponentInstance)
	public:
		/**
		 * Get a list of all component types that this component is dependent on (i.e. must be initialized before this one)
		 * @param components the components this object depends on
		 */
		virtual void getDependentComponents(std::vector<rtti::TypeInfo>& components) const override;

		bool mEnabled = true;									///< Property: 'Enabled' Whether the light is enabled
		ResourcePtr<ParameterEntryRGBColorFloat> mColor;		///< Property: 'Color' The light color
		ResourcePtr<ParameterEntryFloat> mIntensity;			///< Property: 'Intensity' The light intensity

		float mShadowStrength = 1.0f;							///< Property: 'ShadowStrength' The amount of light the shadow consumes.
		bool mEnableShadows = false;							///< Property: 'Enable Shadows' Enables shadows and creates shadow map resources for this light.
	};


	/**
	 * Base class of light component instances for NAP RenderAdvanced's light system.
	 *
	 * When present in the scene, the render advanced service can update light uniform data for material instances that are
	 * compatible with the light's shader interface. On initialization, each light component sets up its own registry of
	 * light uniform data and registers itself at the render advanced service. This way, the service is aware of the lights
	 * in the scene and creates the necessary resources for light information and shadow maps. NAP supports a limited
	 * number of lights per scene (`RenderAdvancedService::getMaximumLightCount`). The way in which these blend/interact
	 * depends on the implementation of the shader program. Increasing the maximum number of lights is trivial, however,
	 * with the current implementation it would take up more shader resource slots.
	 *
	 * Each light component has three default uniforms that are set by the RenderAdvanced service:
	 * - `origin`: `vec3` world position of the light.
	 * - `direction`: `vec3` direction of the light. Some lights may choose to ignore this however (e.g. point lights).
	 * - `flags`: an unsigned integer encoding information such as whether shadows are enabled, see `lightflags.h`.
	 *
	 * Other uniforms may be defined by derived light types. They must be in accordance with the data and shader interface
	 * in the `light.glslinc` file in the RenderAdvanced shader folder. New light types can be added here in the future,
	 * or user implementations can use the 'Custom' enum.
	 *
	 * NAP comes with a default nap::BlinnPhongShader that is compatible with the light system. Hooking this up to a
	 * nap::Material allows for quick scene lighting setups. Material surface uniforms as defined by the shader interface
	 * must be set in data or at runtime. A description of these can be found in the documentation of the shader or its
	 * source file.
	 *
	 * The depth format of shadow maps can be configured in the `nap::RenderAdvancedServiceConfiguration`.
	 *
	 * Rendering with lights requires an additional call to the render advanced service. You can either use `pushLights` on
	 * the render components you wish to render or `renderShadows` with the `updateMaterials` argument set to `true` if you
	 * wish to use shadows too.
	 *
	 * Update light uniforms of lit components when shadows are disabled.
	 * ~~~~~{.cpp}
	 *	mRenderAdvancedService->pushLights(components_to_render, error_state);
	 *	// mRenderService->renderObjects ...
	 * ~~~~~
	 *
	 * Re-render shadow map and update light uniforms.
	 * ~~~~~{.cpp}
	 *	if (mRenderService->beginHeadlessRecording())
	 *	{
	 *		mRenderAdvancedService->renderShadows(render_comps, true);
	 *		mRenderService->endHeadlessRecording();
	 *	}
	 * ~~~~~
	 */
	class NAPAPI LightComponentInstance : public ComponentInstance
	{
		friend class RenderAdvancedService;
		RTTI_ENABLE(ComponentInstance)
	public:
		// Constructor
		LightComponentInstance(EntityInstance& entity, Component& resource) :
			ComponentInstance(entity, resource) { }

		/**
		 * Derived lights destructors must remove themselves by calling
		 * LightComponentInstance::removeLightComponent();
		 */
		virtual ~LightComponentInstance() { };

		/**
		 * Initialize LightComponentInstance based on the LightComponent resource
		 * @param entityCreationParams when dynamically creating entities on initialization, add them to this this list.
		 * @param errorState should hold the error message when initialization fails
		 * @return if the LightComponentInstance is initialized successfully
		 */
		virtual bool init(utility::ErrorState& errorState) override;

		/**
		 * Enables the light
		 */
		virtual void enable(bool enable)									{ mIsEnabled = enable; }

		/**
		 * @return whether this light is active
		 */
		virtual bool isEnabled() const										{ return mIsEnabled; };

		/**
		 * Returns whether this light component supports shadows. Override this call on a derived
		 * light component to enable shadow support.
		 * @return whether this light component supports shadows
		 */
		virtual bool isShadowSupported() const								{ return false; }

		/**
		 * @return whether this light component currently produces shadows
		 */
		virtual bool isShadowEnabled() const								{ return mIsShadowEnabled; }

		/**
		 * @return whether this light was registered with the render advanced service
		 */
		bool isRegistered() const											{ return mIsRegistered; }

		/**
		 * @return the light transform
		 */
		const TransformComponentInstance& getTransform() const				{ return *mTransform; }

		/**
		 * @return the light transform
		 */
		TransformComponentInstance& getTransform()							{ return *mTransform; }

		/**
		 * @return the shadow camera if available, else nullptr
		 */
		virtual CameraComponentInstance* getShadowCamera() = 0;

		/**
		 * @return the light type
		 */
		virtual ELightType getLightType() const = 0;

		/**
		 * @return the shadow map type
		 */
		virtual EShadowMapType getShadowMapType() const = 0;

		/**
		 * @return the shadow map resolution
		 */
		virtual uint getShadowMapSize() const								{ return mShadowMapSize; }

		/**
		 * @return the light intensity
		 */
		virtual float getIntensity() const									{ return mResource->mIntensity->getValue(); }

		/**
		 * @return the shadow strength
		 */
		virtual float getShadowStrength() const								{ return mShadowStrength; }

		/**
		 * Sets the shadow strength
		 */
		virtual void setShadowStrength(float strength)						{ mShadowStrength = strength; }

		/**
		 * @return the light color
		 */
		virtual const RGBColorFloat& getColor() const						{ return mResource->mColor->getValue(); }

		/**
		 * @return the position of the light in world space
		 */
		const glm::vec3 getLightPosition() const							{ return math::extractPosition(getTransform().getGlobalTransform()); }

		/**
		 * @return the direction of the light in world space
		 */
		const glm::vec3 getLightDirection() const							{ return -glm::normalize(getTransform().getGlobalTransform()[2]); }

	protected:
		/**
		 * Removes the current light from the render service.
		 */
		void removeLightComponent();

		/**
		 * Registers a light uniform member for updating the shader interface.
		 * @param memberName the uniform member name of the light variable
		 * @param parameter pointer to the parameter to register. If nullptr, creates and registers a default parameter at runtime
		 */
		template <typename ParameterType, typename DataType>
		void registerLightUniformMember(const std::string& memberName, Parameter* parameter, const DataType& value);

		LightComponent* mResource						= nullptr;
		TransformComponentInstance* mTransform			= nullptr;

		bool mIsEnabled									= true;
		bool mIsShadowEnabled							= false;
		float mShadowStrength							= 1.0f;
		uint mShadowMapSize								= 512U;

		LightParameterList mParameterList;				// List of parameters that are owned by light component instead of the the resource manager

	private:
		Parameter* getLightUniform(const std::string& memberName);

		LightUniformDataMap mUniformDataMap;			// Maps uniform names to parameters
		bool mIsRegistered = false;
	};
}


//////////////////////////////////////////////////////////////////////////
// Template definitions
//////////////////////////////////////////////////////////////////////////

template <typename ParameterType, typename DataType>
void nap::LightComponentInstance::registerLightUniformMember(const std::string& memberName, Parameter* parameter, const DataType& value)
{
	auto* param = parameter;
	if (param == nullptr)
	{
		param = mParameterList.emplace_back(std::make_unique<ParameterType>()).get();
		auto* typed_param = static_cast<ParameterType*>(param);
		typed_param->mName = memberName;
		typed_param->mValue = value;

		utility::ErrorState error_state;
		if (!param->init(error_state))
			assert(false);
	}
	assert(param != nullptr);

	const auto it = mUniformDataMap.insert({ memberName, param });
	assert(it.second);
}
