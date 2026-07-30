#pragma once

/**
 * @file AssetIncludes.hpp
 * @brief Umbrella header exposing the asset system (pulled in by Echelon.hpp).
 */

// ---- Core ----
#include "Asset/Asset.hpp"
#include "Asset/AssetMetadata.hpp"
#include "Asset/Registry.hpp"
#include "Asset/AssetManager.hpp"

// ---- Mesh ----
#include "Asset/Mesh/MeshVertex.hpp"
#include "Asset/Mesh/Mesh.hpp"
#include "Asset/Mesh/Primitives.hpp"

// ---- Importers ----
#include "Asset/Importers/ImportContext.hpp"
#include "Asset/Importers/ImportResult.hpp"
#include "Asset/Importers/Importer.hpp"
#include "Asset/Importers/OBJ/OBJImporter.hpp"
#include "Asset/Importers/Scene/SceneImporter.hpp"
