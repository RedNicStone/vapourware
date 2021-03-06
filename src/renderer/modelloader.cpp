//
// Created by nic on 29/12/2021.
//

#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_USE_MAPBOX_EARCUT

#include "modelloader.h"


std::shared_ptr<ModelLoader> ModelLoader::create(const std::shared_ptr<TextureLibrary> &textureLibrary,
                                                 const std::shared_ptr<MaterialLibrary> &materialLibrary) {
    auto modelLoader = std::make_shared<ModelLoader>();

    modelLoader->config.triangulate = true;
    modelLoader->config.triangulation_method = "earcut";
    modelLoader->config.mtl_search_path = "";
    modelLoader->config.vertex_color = false;

    modelLoader->reader = tinyobj::ObjReader();

    modelLoader->textureLibrary = textureLibrary;
    modelLoader->materialLibrary = materialLibrary;

    return modelLoader;
}

void ModelLoader::loadMaterialProperties(char *property,
                                         const MaterialPropertyBuiltGeneric *materialProperty,
                                         tinyobj::material_t material) {
    if (materialProperty->input & MATERIAL_PROPERTY_INPUT_CONSTANT)
        switch (hash(materialProperty->attributeName)) {
            case "diffuse"_hash:memcpy(property, &material.diffuse, materialProperty->getSize());
                break;
            case "emission"_hash:memcpy(property, &material.emission, materialProperty->getSize());
                break;
            case "transparency"_hash:memcpy(property, &material.transmittance, materialProperty->getSize());
                break;
            case "specular"_hash:memcpy(property, &material.specular, materialProperty->getSize());
                break;
            case "roughness"_hash:memcpy(property, &material.roughness, materialProperty->getSize());
                break;
            case "metallic"_hash:memcpy(property, &material.metallic, materialProperty->getSize());
                break;
            case "sheen"_hash:memcpy(property, &material.sheen, materialProperty->getSize());
                break;
            case "clear_coat_thickness"_hash: memcpy(property,
                                                     &material.clearcoat_thickness,
                                                     materialProperty->getSize());
                break;
            case "clear_coat_roughness"_hash: memcpy(property,
                                                     &material.clearcoat_roughness,
                                                     materialProperty->getSize());
                break;
            case "anisotropy"_hash:memcpy(property, &material.anisotropy, materialProperty->getSize());
                break;
            case "anisotropy_rotation"_hash: memcpy(property,
                                                    &material.anisotropy_rotation,
                                                    materialProperty->getSize());
                break;
        }
}

std::vector<std::shared_ptr<Texture>> ModelLoader::loadMaterialTextures(const std::shared_ptr<
    MaterialPropertyLayoutBuilt> &materialLayout,
                                                                        const tinyobj::material_t &material,
                                                                        const std::shared_ptr<TextureLibrary> &textureLibrary,
                                                                        const std::string &modelFilename) {
    auto textures = std::vector<std::shared_ptr<Texture>>(materialLayout->properties.size(), nullptr);
    for (size_t i = 0; i < materialLayout->properties.size(); i++) {
        if (materialLayout->properties[i]->input & MATERIAL_PROPERTY_INPUT_TEXTURE) {
            std::string texFile;
            switch (hash(materialLayout->properties[i]->attributeName)) {
                case "diffuse"_hash:texFile = locateTexture(material.diffuse_texname, modelFilename);
                    if (!texFile.empty())
                        textures[i] =
                            textureLibrary->createTexture(texFile, materialLayout->properties[i]->pixelFormat);
                    break;
                case "emission"_hash:texFile = locateTexture(material.emissive_texname, modelFilename);
                    if (!texFile.empty())
                        textures[i] =
                            textureLibrary->createTexture(texFile, materialLayout->properties[i]->pixelFormat);
                    break;
                case "specular"_hash:texFile = locateTexture(material.specular_texname, modelFilename);
                    if (!texFile.empty())
                        textures[i] =
                            textureLibrary->createTexture(texFile, materialLayout->properties[i]->pixelFormat);
                    break;
                case "roughness"_hash:texFile = locateTexture(material.roughness_texname, modelFilename);
                    if (!texFile.empty())
                        textures[i] =
                            textureLibrary->createTexture(texFile, materialLayout->properties[i]->pixelFormat);
                    break;
                case "metallic"_hash:texFile = locateTexture(material.metallic_texname, modelFilename);
                    if (!texFile.empty())
                        textures[i] =
                            textureLibrary->createTexture(texFile, materialLayout->properties[i]->pixelFormat);
                    break;
                case "sheen"_hash:texFile = locateTexture(material.sheen_texname, modelFilename);
                    if (!texFile.empty())
                        textures[i] =
                            textureLibrary->createTexture(texFile, materialLayout->properties[i]->pixelFormat);
                    break;
            }
            if (texFile.empty())
                materialLayout->properties[i]->input = MATERIAL_PROPERTY_INPUT_CONSTANT;
            else
                materialLayout->properties[i]->input = MATERIAL_PROPERTY_INPUT_TEXTURE;
        } else
            materialLayout->properties[i]->input = MATERIAL_PROPERTY_INPUT_CONSTANT;
    }

    return textures;
}

std::string ModelLoader::locateTexture(const std::string &textureFilename, const std::string &modelFilename) {
    if (textureFilename.empty())
        return "";
    auto textureFilenameV = textureFilename;
    Utils::replaceAll(textureFilenameV, "\\", "/");
    auto textureFile = std::filesystem::path(textureFilenameV);
    if (!std::filesystem::exists(textureFile) || std::filesystem::is_directory(textureFile)) {
        auto modelFile = std::filesystem::path(modelFilename);
        textureFile = modelFile.parent_path() / textureFile;
        if (!std::filesystem::exists(textureFile) || std::filesystem::is_directory(textureFile))
            return "";
    }
    return textureFile;
}

std::vector<std::shared_ptr<Mesh>> ModelLoader::import(const std::string &filename,
                                                       const std::shared_ptr<MasterMaterialTemplate> &masterMaterialTemplate,
                                                       bool normalizePos) {
    if (!reader.ParseFromFile(filename, config)) {
        if (!reader.Error().empty()) {
            throw std::runtime_error(reader.Error());
        }
        throw std::runtime_error("TinyOBJLoader encountered an error");
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &materials = reader.GetMaterials();

    std::vector<std::shared_ptr<Mesh>> meshes;
    std::vector<std::shared_ptr<Material>> surfaceMaterials;
    meshes.reserve(shapes.size());
    surfaceMaterials.reserve(materials.size());

    for (const auto &material: materials) {
        char *properties = new char[masterMaterialTemplate->getPropertySize()];
        size_t offset = 0;
        for (const auto &property: masterMaterialTemplate->getPropertyLayout()->properties) {
            loadMaterialProperties(properties + offset, property, material);
            offset += property->getSize();
        }
        auto layout = copyLayout(masterMaterialTemplate->getPropertyLayout());
        auto textures = loadMaterialTextures(layout, material, textureLibrary, filename);

        auto masterMaterial = materialLibrary->createMasterMaterial(masterMaterialTemplate, material.name, layout);
        auto mat = materialLibrary->createMaterial(masterMaterial, properties, textures, material.name);
        surfaceMaterials.push_back(mat);
    }

    // Loop over shapes
    for (const auto &shape: shapes) {
        std::unordered_map<int, std::shared_ptr<Meshlet>> meshlets;

        BoundingBox bbox{};
        bbox.min = *reinterpret_cast<const glm::vec3 *>(attrib.vertices.data());
        bbox.max = *reinterpret_cast<const glm::vec3 *>(attrib.vertices.data());

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};
        uint32_t vertexIndex = 0;

        // Loop over faces(polygon)
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {

            int materialID = shape.mesh.material_ids[f];
            if (meshlets[materialID] == nullptr) {
                meshlets[materialID] = std::make_shared<Meshlet>();
                if (materialID >= 0)
                    meshlets[materialID]->material = surfaceMaterials[static_cast<unsigned long>(materialID)];
            }

            // Loop over vertices in the face.
            for (size_t v = 0; v < 3; v++) {
                tinyobj::index_t idx = shape.mesh.indices[f * 3 + v];

                Vertex vertex{};
                vertex.pos =
                    *reinterpret_cast<const glm::vec3 *>(attrib.vertices.data() + 3 * size_t(idx.vertex_index));

                if (idx.normal_index >= 0) {
                    vertex.normal =
                        {attrib.normals[static_cast<uint>(3 * idx.normal_index + 0)] * INT16_MAX,
                         attrib.normals[static_cast<uint>(3 * idx.normal_index + 1)] * INT16_MAX,
                         attrib.normals[static_cast<uint>(3 * idx.normal_index + 2)] * INT16_MAX};
                }

                if (idx.texcoord_index >= 0) {
                    vertex.uv =
                        {(attrib.texcoords[static_cast<uint>(2 * idx.texcoord_index + 0)]) * UINT16_MAX,
                         (1.0f - attrib.texcoords[static_cast<uint>(2 * idx.texcoord_index + 1)]) * UINT16_MAX};
                }

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertexIndex);
                    meshlets[materialID]->indexData.push_back(static_cast<uint32_t>(
                                                                  vertexIndex));
                    vertexIndex++;

                    bbox.min = glm::min(bbox.min, vertex.pos);
                    bbox.max = glm::max(bbox.max, vertex.pos);
                } else
                    meshlets[materialID]->indexData.push_back(uniqueVertices[vertex]);
            }
        }

        std::vector<std::shared_ptr<Meshlet>> meshletVector;
        meshletVector.reserve(meshlets.size());
        uint32_t firstIndex = 0;
        for (const auto &kv: meshlets) {
            kv.second->firstIndex = firstIndex;
            firstIndex += kv.second->indexData.size();
            meshletVector.push_back(kv.second);
        }

        std::vector<Vertex> vertexVector;
        vertexVector.resize(uniqueVertices.size());
        for (const auto &kv: uniqueVertices) {
            vertexVector[kv.second] = kv.first;
        }

        meshes.push_back(Mesh::create(meshletVector, vertexVector, bbox, shape.name, normalizePos));
    }

    return meshes;
}
