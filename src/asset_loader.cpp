#include "asset_loader.h"
#include "scene.h"
#include "primitive_factory.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <tiny_gltf.h>
#include <iostream>
#include <vector>

namespace AssetLoader {

static primitives::MeshGL meshFromAssimp(const aiMesh* amesh) {
    std::vector<float> verts;
    std::vector<unsigned int> idx;
    verts.reserve(amesh->mNumVertices * 3);
    for(unsigned int i=0;i<amesh->mNumVertices;++i){ verts.push_back(amesh->mVertices[i].x); verts.push_back(amesh->mVertices[i].y); verts.push_back(amesh->mVertices[i].z); }
    for(unsigned int f=0; f<amesh->mNumFaces; ++f){ const aiFace& face = amesh->mFaces[f]; for(unsigned int k=0;k<face.mNumIndices;++k) idx.push_back(face.mIndices[k]); }
    primitives::MeshGL m; m.upload(verts, idx); return m;
}

bool loadModelWithAssimp(const std::string& path, Scene& scene) {
    Assimp::Importer importer;
    const aiScene* ascene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices);
    if(!ascene) { std::cerr << "Assimp failed to load: " << importer.GetErrorString() << "\n"; return false; }
    // iterate meshes
    for(unsigned int i=0;i<ascene->mNumMeshes;++i){ const aiMesh* am = ascene->mMeshes[i]; primitives::MeshGL m = meshFromAssimp(am); SceneEntity e; e.type = primitives::PrimitiveType::Cube; e.mesh = std::make_unique<primitives::MeshGL>(std::move(m)); scene.addEntity(std::move(e)); }
    return true;
}

bool loadModelWithTinyGLTF(const std::string& path, Scene& scene) {
    tinygltf::Model model; tinygltf::TinyGLTF loader; std::string err, warn;
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);
    if(!warn.empty()) std::cerr << "tinygltf warn: " << warn << "\n";
    if(!err.empty()) std::cerr << "tinygltf err: " << err << "\n";
    if(!ret) return false;
    // minimal: load first mesh primitives positions only
    for(size_t mi=0; mi<model.meshes.size(); ++mi){ const tinygltf::Mesh& mesh = model.meshes[mi]; for(const auto& prim : mesh.primitives){ if(prim.attributes.count("POSITION")==0) continue; const tinygltf::Accessor& acc = model.accessors[prim.attributes.at("POSITION")]; const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView]; const tinygltf::Buffer& buf = model.buffers[bv.buffer]; const unsigned char* data = buf.data.data() + bv.byteOffset + acc.byteOffset; size_t vc = acc.count; std::vector<float> verts; verts.resize(vc*3); memcpy(verts.data(), data, vc*3*sizeof(float)); std::vector<unsigned int> idx; if(prim.indices >= 0){ const tinygltf::Accessor& ia = model.accessors[prim.indices]; const tinygltf::BufferView& ibv = model.bufferViews[ia.bufferView]; const tinygltf::Buffer& ibuf = model.buffers[ibv.buffer]; const unsigned char* idata = ibuf.data.data() + ibv.byteOffset + ia.byteOffset; size_t ic = ia.count; idx.resize(ic); if(ia.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT){ const unsigned short* s = (const unsigned short*)idata; for(size_t k=0;k<ic;++k) idx[k] = s[k]; } else if(ia.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT){ const unsigned int* s = (const unsigned int*)idata; for(size_t k=0;k<ic;++k) idx[k] = s[k]; } }
    primitives::MeshGL m; m.upload(verts, idx); SceneEntity e; e.type = primitives::PrimitiveType::Cube; e.mesh = std::make_unique<primitives::MeshGL>(std::move(m)); scene.addEntity(std::move(e)); } }
    return true;
}

bool loadModel(const std::string& path, Scene& scene) {
    // choose tinygltf for glb/gltf/vrm otherwise use assimp
    std::string p = path;
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);
    if(p.size() >= 4 && (p.substr(p.size()-4)==".gltf" || p.substr(p.size()-4)==".glb" || p.substr(p.size()-4)==".vrm")){
        if(loadModelWithTinyGLTF(path, scene)) return true;
    }
    return loadModelWithAssimp(path, scene);
}

} // namespace AssetLoader
