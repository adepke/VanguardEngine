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
	float nearPlane = 5000.f;
	float farPlane = 1.f;

	return (2.f * farPlane) / (nearPlane + farPlane - (1.f - hyperbolicDepth) * (nearPlane - farPlane));
}