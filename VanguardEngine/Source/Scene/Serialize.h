// Copyright (c) 2019-2022 Andrew Depke

#pragma once

// Contains serialization info for third party structs. All first party structs
// should be defined intrusively. Include this file after all relevant components.

namespace DirectX
{
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(XMFLOAT3, x, y, z);
};