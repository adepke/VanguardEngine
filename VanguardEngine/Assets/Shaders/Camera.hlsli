// Copyright (c) 2019-2021 Andrew Depke

#pragma pack_matrix(row_major)

struct CameraBuffer
{
	matrix viewMatrix;
	matrix projectionMatrix;
};

float LinearizeDepth(float hyperbolicDepth)
{
	// #TODO: Pass the plane values to the shader dynamically.
	float nearPlane = 1.f;
	float farPlane = 5000.f;

	return (2.f * nearPlane) / (farPlane + nearPlane - hyperbolicDepth * (farPlane - nearPlane));
}