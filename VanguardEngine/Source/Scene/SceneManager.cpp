// Copyright (c) 2019-2022 Andrew Depke

#include "SceneManager.h"
#include <Scene/SceneManager.h>
#include <Core/Base.h>
#include <Core/Config.h>
#include <Scene/Archive.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/Device.h>
#include <Rendering/Resource.h>
#include <Asset/AssetManager.h>
#include <Asset/AssetComponents.h>
#include <Scene/Serialize.h>  // Always include after components.

#include <sqlite3.h>
#include <stb_image.h>

#include <algorithm>
#include <cstring>
#include <system_error>

// All components must be specified here. When adding new components, do not forget to update
// this list, or they won't get serialized.
#define ALL_COMPONENTS(archive) \
	.component<NameComponent, TransformComponent>(archive) \
	.component<CameraComponent, LightComponent, TimeOfDayComponent>(archive) \
	.component<AssetComponent>(archive)

constexpr auto sqlCreate = R"(
	CREATE TABLE scene(
		entities BLOB,
		thumbnail BLOB
	);
)";

constexpr auto sqlInsert = R"(
	INSERT INTO scene VALUES(
		?, ?
	);
)";

constexpr auto sqlSelect = R"(
	SELECT entities FROM scene LIMIT 1;
)";

constexpr auto sqlSelectMetadata = R"(
	SELECT thumbnail FROM scene LIMIT 1;
)";

namespace Scene
{
	// Decodes a thumbnail blob into a GPU texture.
	TextureHandle UploadThumbnail(RenderDevice& device, const void* data, int size)
	{
		if (!data || size <= 0)
		{
			return {};
		}

		int width = 0;
		int height = 0;
		int channels = 0;
		auto* pixels = stbi_load_from_memory(
			reinterpret_cast<const stbi_uc*>(data),
			size,
			&width, &height, &channels,
			STBI_rgb_alpha);

		if (!pixels || width <= 0 || height <= 0)
		{
			if (pixels)
			{
				stbi_image_free(pixels);
			}
			return {};
		}

		std::vector<uint8_t> bytes(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
		std::memcpy(bytes.data(), pixels, bytes.size());
		stbi_image_free(pixels);

		TextureDescription description{};
		description.bindFlags = BindFlag::ShaderResource;
		description.accessFlags = AccessFlag::CPUWrite;
		description.width = static_cast<uint32_t>(width);
		description.height = static_cast<uint32_t>(height);
		description.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		const auto handle = device.GetResourceManager().Create(description, VGText("Scene thumbnail"));
		device.GetResourceManager().Write(handle, bytes);
		device.GetDirectList().TransitionBarrier(handle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		return handle;
	}

	bool LoadMetadata(RenderDevice& device, const std::filesystem::path &path, SceneMetadata& meta)
	{
		sqlite3* db;
		if (const auto code = sqlite3_open(path.generic_string().c_str(), &db); code != 0)
		{
			sqlite3_close(db);
			return false;
		}

		sqlite3_stmt* statement = nullptr;
		if (const auto code = sqlite3_prepare(db, sqlSelectMetadata, -1, &statement, nullptr); code != SQLITE_OK)
		{
			sqlite3_close(db);
			return false;
		}
		if (const auto code = sqlite3_step(statement); code != SQLITE_ROW)
		{
			sqlite3_close(db);
			return false;
		}
		const auto thumbnail = sqlite3_column_blob(statement, 0);
		const auto thumbnailSize = sqlite3_column_bytes(statement, 0);

		meta.name = path.filename().replace_extension().string();
		meta.path = path;
		std::error_code timeError;
		meta.lastModified = std::filesystem::last_write_time(path, timeError);
		if (timeError)
		{
			// Fall back to epoch so the entry still sorts deterministically.
			meta.lastModified = std::filesystem::file_time_type{};
		}
		meta.thumbnail = UploadThumbnail(device, thumbnail, thumbnailSize);

		sqlite3_finalize(statement);
		sqlite3_close(db);

		return true;
	}

	std::vector<SceneMetadata> List(RenderDevice& device)
	{
		std::vector<std::filesystem::path> scenes;
		for (const auto& scene : std::filesystem::directory_iterator(Config::scenesPath))
		{
			if (scene.path().extension().generic_wstring() == VGText(".scene"))
			{
				scenes.emplace_back(scene);
			}
		}

		std::vector<SceneMetadata> sceneMetas;
		for (const auto& scene : scenes)
		{
			SceneMetadata meta;
			if (!LoadMetadata(device, scene, meta))
			{
				VGLogWarning(logScene, "Failed to load scene metadata for scene '{}'", scene.generic_wstring());
			}
			sceneMetas.emplace_back(std::move(meta));
		}

		std::stable_sort(sceneMetas.begin(), sceneMetas.end(),
			[](const SceneMetadata& a, const SceneMetadata& b)
			{
				return a.lastModified > b.lastModified;
			});

		return sceneMetas;
	}

	bool Load(entt::registry &registry, const std::filesystem::path &path)
	{
		sqlite3* db;
		if (const auto code = sqlite3_open(path.generic_string().c_str(), &db); code != 0)
		{
			VGLogError(logScene, "Failed to load scene '{}': {}", path.generic_wstring(), Str2WideStr(sqlite3_errmsg(db)));
			sqlite3_close(db);
			return false;
		}

		sqlite3_stmt* statement = nullptr;
		if (const auto code = sqlite3_prepare(db, sqlSelect, -1, &statement, nullptr); code != SQLITE_OK)
		{
			VGLogError(logScene, "Failed to load scene '{}': {}", path.generic_wstring(), Str2WideStr(sqlite3_errmsg(db)));
			sqlite3_close(db);
			return false;
		}
		if (const auto code = sqlite3_step(statement); code != SQLITE_ROW)
		{
			VGLogError(logScene, "Failed to load scene '{}': {}", path.generic_wstring(), Str2WideStr(sqlite3_errmsg(db)));
			sqlite3_close(db);
			return false;
		}
		const auto entities = sqlite3_column_blob(statement, 0);
		const auto entitiesSize = sqlite3_column_bytes(statement, 0);

		ArchiveOutput archive{ (char*)entities, static_cast<size_t>(entitiesSize) };

		// #TODO: Instead of nuking the registry and loading fresh, consider using an EnTT continuous loader.
		// Some fancy logic along the lines of only re-loading entities that are in the data, otherwise ignoring existing
		// entities.
		registry.clear();
		auto snapshot = entt::snapshot_loader{ registry };
		snapshot
			.entities(archive)
			ALL_COMPONENTS(archive);

		// Rebuild GPU-side asset data.
		AssetManager::Get().ResolveMeshes(registry);

		VGLog(logScene, "Loaded scene '{}'", path.generic_wstring());
		return true;
	}

	bool Save(const entt::registry &registry, const std::filesystem::path &path, const std::vector<uint8_t>& thumbnail)
	{
		sqlite3* db;
		if (const auto code = sqlite3_open(path.generic_string().c_str(), &db); code != 0)
		{
			VGLogError(logScene, "Failed to save scene '{}': {}", path.generic_wstring(), Str2WideStr(sqlite3_errmsg(db)));
			sqlite3_close(db);
			return false;
		}

		ArchiveInput archive;

		auto snapshot = entt::snapshot{ registry };
		snapshot
			.entities(archive)
			ALL_COMPONENTS(archive);

		const auto bytes = archive.ToBytes();

		char* dbError;
		if (const auto code = sqlite3_exec(db, sqlCreate, nullptr, nullptr, &dbError); code != SQLITE_OK)
		{
			VGLogError(logScene, "Failed to save scene: {}", Str2WideStr(dbError));
			sqlite3_free(dbError);
			sqlite3_close(db);
			return false;
		}

		sqlite3_stmt* statement = nullptr;
		if (const auto code = sqlite3_prepare(db, sqlInsert, -1, &statement, nullptr); code != SQLITE_OK)
		{
			VGLogError(logScene, "Failed to save scene: {}", Str2WideStr(sqlite3_errmsg(db)));
			sqlite3_close(db);
			return false;
		}
		if (const auto code = sqlite3_bind_blob(statement, 1, bytes.data(), bytes.size(), SQLITE_STATIC); code != SQLITE_OK)
		{
			VGLogError(logScene, "Failed to save scene: {}", Str2WideStr(sqlite3_errmsg(db)));
			sqlite3_close(db);
			return false;
		}
		// If a thumbnail was provided, bind it directly, otherwise use NULL.
		const auto thumbnailBindResult = thumbnail.empty()
			? sqlite3_bind_null(statement, 2)
			: sqlite3_bind_blob(statement, 2, thumbnail.data(), static_cast<int>(thumbnail.size()), SQLITE_STATIC);
		if (thumbnailBindResult != SQLITE_OK)
		{
			VGLogError(logScene, "Failed to save scene: {}", Str2WideStr(sqlite3_errmsg(db)));
			sqlite3_close(db);
			return false;
		}
		if (const auto code = sqlite3_step(statement); code != SQLITE_DONE)
		{
			VGLogError(logScene, "Failed to save scene: {}", Str2WideStr(sqlite3_errmsg(db)));
			sqlite3_close(db);
			return false;
		}
		if (const auto code = sqlite3_finalize(statement); code != SQLITE_OK)
		{
			VGLogError(logScene, "Failed to save scene: {}", Str2WideStr(sqlite3_errmsg(db)));
			sqlite3_close(db);
			return false;
		}

		sqlite3_close(db);

		VGLog(logScene, "Saved scene '{}'", path.generic_wstring());
		return true;
	}

	void Clear(entt::registry& registry)
	{
		registry.clear();
	}
}