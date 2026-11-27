#pragma once
#include <assimp/material.h>
#include <array>
#include <string>

namespace Assimp {
	class Importer;
}

struct aiScene;
namespace Engine::ModelImport {
	// キャッシュに書く参照と、復元する Assimp マテリアルの順序をそろえる。
	inline constexpr std::array MaterialTextureTypes{aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE,
		aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_NORMALS};

	// 返却シーンの所有者は呼び出し側のImporter。キャッシュ不正時は元ファイルへ戻る
	const aiScene* ImportScene(Assimp::Importer& importer, const std::string& path);
} // namespace Engine::ModelImport
