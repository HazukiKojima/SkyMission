#pragma once
#include <string>
namespace Assimp {
	class Importer;
}
struct aiScene;
namespace Engine::ModelImport {
	// 返却シーンの所有者は呼び出し側のImporter。キャッシュ不正時は元ファイルへ戻る。
	const aiScene* ImportScene(Assimp::Importer& importer, const std::string& path);
} // namespace Engine::ModelImport
