#include <Components.h>

#include <glm/gtc/matrix_transform.hpp>

namespace eg::Components
{
	glm::mat4x4 Camera::buildProjection(vk::Extent2D extent) const
	{
		auto proj = glm::perspectiveRH_ZO(glm::radians(mFov), (float)extent.width / (float)extent.height, mNear, mFar);
		proj[1][1] *= -1;
		return proj;
	}
	glm::mat4x4 Camera::buildView() const
	{
		glm::mat4x4 finalTransform(1.0f);

		finalTransform = glm::rotate(finalTransform, glm::radians(mPitch), { 1, 0, 0 });
		finalTransform = glm::rotate(finalTransform, glm::radians(mYaw), { 0, 1, 0 });
		finalTransform = glm::translate(finalTransform, -mPosition);


		return finalTransform;
	}
}