#include "Components.h"

namespace eg::Components
{
	bool CameraFrustumCuller::isPointInFrustum(const glm::vec3& point) const
	{
		for (const auto& plane : mFrustumPlanes) {
			if (glm::dot(plane.normal, point) + plane.d < 0.0f)
				return false;
		}
		return true;
	}

	bool CameraFrustumCuller::isBoundingBoxInFrustum(const glm::vec3& min, const glm::vec3& max, const glm::vec3* offset) const
	{
		return false;
	}

	bool CameraFrustumCuller::isSphereInFrustum(const glm::vec3& center, float radius) const
	{
		for (const auto& plane : mFrustumPlanes) {
			if (glm::dot(plane.normal, center) + plane.d < -radius)
				return false;
		}
		return true;
	}

	CameraFrustumCuller::FrustumPlane CameraFrustumCuller::extractPlane(float a, float b, float c, float d)
	{
		glm::vec3 normal(a, b, c);
		float length = glm::length(normal);
		return { normal / length, d / length };
	}


	void CameraFrustumCuller::updateFrustumPlanes(vk::Extent2D extent)
	{
		glm::mat4 view = mCamera.buildView();
		glm::mat4 proj = mCamera.buildProjection(extent);
		glm::mat4 viewProj = proj * view;

		// Extract the 6 frustum planes from the view-projection matrix
		// Each plane: ax + by + cz + d = 0, represented as (normal, d)

		// Left
		mFrustumPlanes[0] = extractPlane(
			viewProj[0][3] + viewProj[0][0],
			viewProj[1][3] + viewProj[1][0],
			viewProj[2][3] + viewProj[2][0],
			viewProj[3][3] + viewProj[3][0]);

		// Right
		mFrustumPlanes[1] = extractPlane(
			viewProj[0][3] - viewProj[0][0],
			viewProj[1][3] - viewProj[1][0],
			viewProj[2][3] - viewProj[2][0],
			viewProj[3][3] - viewProj[3][0]);

		// Bottom
		mFrustumPlanes[2] = extractPlane(
			viewProj[0][3] + viewProj[0][1],
			viewProj[1][3] + viewProj[1][1],
			viewProj[2][3] + viewProj[2][1],
			viewProj[3][3] + viewProj[3][1]);

		// Top
		mFrustumPlanes[3] = extractPlane(
			viewProj[0][3] - viewProj[0][1],
			viewProj[1][3] - viewProj[1][1],
			viewProj[2][3] - viewProj[2][1],
			viewProj[3][3] - viewProj[3][1]);

		// Near
		mFrustumPlanes[4] = extractPlane(
			viewProj[0][3] + viewProj[0][2],
			viewProj[1][3] + viewProj[1][2],
			viewProj[2][3] + viewProj[2][2],
			viewProj[3][3] + viewProj[3][2]);

		// Far
		mFrustumPlanes[5] = extractPlane(
			viewProj[0][3] - viewProj[0][2],
			viewProj[1][3] - viewProj[1][2],
			viewProj[2][3] - viewProj[2][2],
			viewProj[3][3] - viewProj[3][2]);
	}
}