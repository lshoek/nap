/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// Local includes
#include "vulkan/vulkan_core.h"
#include "rtti/typeinfo.h"

// External Includes
#include <nap/device.h>
#include <nap/resourceptr.h>
#include <nap/numeric.h>
#include <nap/group.h>
#include <string>
#include <vector>

namespace nap
{
	class RenderService;
	class Core;

	// RenderMask definition, supports up to 64 different tags
	using RenderMask = uint64;

	/**
	 * Render tags can be used to categorize render components. Unlike render layers, tags are unordered and multiple
	 * of them can be assigned to a single render component. Each tag resource registers itself in the render service
	 * and is assigned a unique tag index on app initialization. This ensures tags can be composited into render masks,
	 * which are bit flags that are fast to compare.
	 *
	 * One useful example would be to categorize specific components as "Debug", distinguishing objects used as visual
	 * aid for debugging purposes from standard objects (tag "Default"). They may be excluded from rendering based on
	 * settings or a window setup for instance. You could do the following:
	 * 
	 * `````{.cpp}
	 * // Consider caching the render mask
	 * RenderMask mask = mRenderService->findRenderMask("Default") | mRenderService->findRenderMask("Debug");
	 * mRenderService->renderObjects(renderTarget, camera, render_comps, mask);
	 * `````
	 */
	class NAPAPI RenderTag : public Device
	{
		friend class RenderService;
		RTTI_ENABLE(Device)
	public:
		RenderTag(Core& core);
		virtual ~RenderTag() = default;

		/**
		 * Register the RenderTag with the RenderService
		 */
		virtual bool start(utility::ErrorState& errorState) override;

		/**
		 * Unregister the RenderTag with the RenderService
		 */
		virtual void stop() override;

		/**
		 * @return the index of the tag in the registry.
		 */
		uint getIndex() const;

		std::string mName;									///< Property: 'Name' The tag name

	private:
		RenderService& mRenderService;
	};

	// RenderTagList definition
	using RenderTagList = std::vector<rtti::ObjectPtr<RenderTag>>;

	/**
	 * Creates a render mask from a list of tags
	 * @param tags a list of tags to create a mask from
	 * @return the render mask
	 */
	static RenderMask createRenderMask(const RenderTagList& tags)
	{
		RenderMask mask = 0U;
		for (const auto& tag : tags)
			mask |= 0x01 << tag->getIndex();
		return mask;
	}

	/**
	 * Compares component and inclusion masks, returns true if any of the tags overlap using bitwise AND
	 * @param componentMask the render mask of a component
	 * @param inclusionMask the render mask to compare with
	 * @return true if the componentMask is included in the inclusionMask
	 */
	static bool compareRenderMask(RenderMask componentMask, RenderMask inclusionMask)
	{
		return (componentMask == 0U) || ((componentMask & inclusionMask) > 0U);
	}

	// RenderTagGroup definition
	using RenderTagGroup = Group<RenderTag>;
}
