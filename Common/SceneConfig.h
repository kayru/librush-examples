#pragma once

// Per-scene {camera, settings} config, Reflect-serialized and keyed by model path.
// Settings is the app's own describable struct; modelFilename may be null (procedural).

#include "Reflect.h"
#include "Utils.h"

#include <Rush/UtilCamera.h>
#include <Rush/UtilLog.h>

#include <string>

namespace Rush
{

template <typename Settings>
struct SceneConfigRoot
{
	Camera&   camera;
	Settings& settings;
	template <typename Ar> void describe(Ar& ar)
	{
		ar.field("camera", camera);
		ar.field("settings", settings);
	}
};

template <typename Settings>
void saveSceneConfig(const char* tag, const char* modelFilename, u32 version, Camera& camera, Settings& settings)
{
	const std::string path = sceneConfigPath(tag, modelFilename);
	SceneConfigRoot<Settings> root{camera, settings};
	if (Reflect::saveToFile(path.c_str(), version, root))
	{
		RUSH_LOG("Saved config to '%s'", path.c_str());
	}
}

// Returns true if a config file was found and applied (camera/settings overwritten).
template <typename Settings>
bool loadSceneConfig(const char* tag, const char* modelFilename, u32 version, Camera& camera, Settings& settings)
{
	const std::string path = sceneConfigPath(tag, modelFilename);
	SceneConfigRoot<Settings> root{camera, settings};
	if (Reflect::loadFromFile(path.c_str(), version, root))
	{
		RUSH_LOG("Loaded config from '%s'", path.c_str());
		return true;
	}
	RUSH_LOG("No usable config at '%s' (using defaults)", path.c_str());
	return false;
}

} // namespace Rush
