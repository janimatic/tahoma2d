/*
* REFERENCES :
* https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
* 
* examples of gltf animation usage:
 * https://github.com/HK416/glTF_Animation/blob/master/src/src/Model/model.cpp
* https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/1cf511b13adfc9161fc59f137982e210e65eda1d/base/VulkanglTFModel.cpp#L1204
* 
* toonz keyframe api ?
* void Model::updateAnimation(uint32_t index, float time)
* TStageObject::updateKeyframes
*/
#include "gltfmodel.h"
#include <iostream>

#define TINYGLTF_IMPLEMENTATION  
// error LNK2005: "public: void __cdecl std::basic_ifstream<char,struct
// std::char_traits<char> >::`vbase destructor'(void)"
// USING linker > command line > additional options: add /FORCE:MULTIPLE (bad idea ?)
// https://learn.microsoft.com/en-us/cpp/error-messages/tool-errors/linker-tools-error-lnk2005?view=msvc-170
// https://learn.microsoft.com/en-us/cpp/build/reference/force-force-file-output?view=msvc-170
// https://github.com/syoyo/tinyusdz/issues/124 > inline ReadWholeFile &
// GetFileSizeInBytes in tiny_gltf.h !// toonz\sources\include\tfilepath_io.h
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
//#define STBI_MSC_SECURE_CRT
#include "tiny_gltf.h"

/// <summary>
/// Each of the animation’s samplers defines the input / output pair: 
/// - a set of floating - point scalar values representing linear time in SECONDS;
/// - and a set of vectors or
/// scalars representing the animated property.
/// </summary>
AnimationSampler::AnimationSampler() : interpolation(), inputs(), outputs() {
  interpolation = INTERPOLATION_TYPE::UNKNOWN;
}
AnimationSampler::AnimationSampler(const AnimationSampler &other)
    : interpolation(other.interpolation)
    , inputs(other.inputs)
    , outputs(other.outputs) {}
AnimationChannel::AnimationChannel() : path_type(), node_id(), sampler_id() {
  path_type  = PATH_TYPE::UNKNOWN;
  node_id    = -1;
  sampler_id = -1;
}
AnimationChannel::AnimationChannel(const AnimationChannel &other) 
: path_type(other.path_type)
    , node_id(other.node_id)
    , sampler_id(other.sampler_id) {}

Vertex::Vertex()
    : position(), normal(), texcoord(), joint(), weight() {}

Vertex::Vertex(const Vertex &other)
    : position(other.position)
    , normal(other.normal)
    , texcoord(other.texcoord)
    , joint(other.joint)
    , weight(other.weight) {}

Mesh::Mesh()
    : name()
    , vertices()
    , indices()
    , material_id()
    , matrix()
    , joint_matrices()
    , vao()
    , vbo()
    , ebo() {
  for (unsigned int i = 0; i < MAX_NUM_JOINTS; ++i)
    joint_matrices[i] = {{1.0, 1.0, 1.0, 1.0},
              {1.0, 1.0, 1.0, 1.0},
              {1.0, 1.0, 1.0, 1.0},
              {1.0, 1.0, 1.0, 1.0}};

    //joint_matrices[i] = orca::Identity(joint_matrices[i]);
  material_id = -1;
  vao         = 0;
  vbo         = 0;
  ebo         = 0;
}
Mesh::Mesh(const Mesh &other)
    : name(other.name)
    , vertices(other.vertices)
    , indices(other.indices)
    , material_id(other.material_id)
    , matrix(other.matrix)
    , joint_matrices(other.joint_matrices)
    , vao(other.vao)
    , vbo(other.vbo)
    , ebo(other.ebo) {}

Animation::Animation()
    : name(), samplers(), channels(), start_time(), end_time() {
  start_time = std::numeric_limits<float>::max();
  end_time   = std::numeric_limits<float>::min();
}

Animation::Animation(const Animation &other)
    : name(other.name)
    , samplers(other.samplers)
    , channels(other.channels)
    , start_time(other.start_time)
    , end_time(other.end_time) {}

Node::Node()
    : name()
    , child_ids()
    , matrix()
    , translate()
    , rotate()
    , scale()
    , node_id()
    , parent_id()
    , mesh_id()
    , skin_id() 
    , camera_id() {

  //matrix    = orca::Identity(matrix);
  matrix    = {{1.0, 1.0, 1.0, 1.0},
            {1.0, 1.0, 1.0, 1.0},
            {1.0, 1.0, 1.0, 1.0},
            {1.0, 1.0, 1.0, 1.0}};
  vec3f v   = {1.0, 1.0, 1.0};
  scale     = v;
  node_id   = -1;
  parent_id = -1;
  mesh_id   = -1;
  skin_id   = -1;
  camera_id = -1;
}

/* copy constructor */
Node::Node(const Node &other)
    : name(other.name)
    , child_ids(other.child_ids)
    , matrix(other.matrix)
    , translate(other.translate)
    , rotate(other.rotate)
    , scale(other.scale)
    , node_id(other.node_id)
    , parent_id(other.parent_id)
    , mesh_id(other.mesh_id)
    , skin_id(other.skin_id) 
    , camera_id(other.camera_id) 
{}

Image::Image() : width(), height(), component(), data(), name() {}

Image::Image(const Image &other)
    : width(other.width)
    , height(other.height)
    , component(other.component)
    , data(other.data)
    , name(other.name) {}

Texture::Texture() : name(), image(), tbo() {}

Texture::Texture(const Texture &other)
    : name(other.name)
    , image(other.image)
    , tbo(other.tbo) {}

Camera::Camera()
    : camera_type(), znear(), zfar(), aspectRatio(), yfov(), xmag(), ymag() {}

Camera::Camera(const Camera &other)
    : camera_type(other.camera_type), 
    znear(other.znear), 
    zfar(other.zfar)
    , aspectRatio(other.aspectRatio)
    , yfov(other.yfov)
    , xmag(other.xmag)
    , ymag(other.ymag){}

bool GltfModel::exportGltf(QString path) { return false; }
bool GltfModel::importGltf(QString path) {
  tinygltf::TinyGLTF loader;
  std::string err, warn;
  bool ret = false;
  if (path.endsWith(".glb", Qt::CaseInsensitive))
    ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.toStdString());
  else if (path.endsWith(".gltf", Qt::CaseInsensitive))
    ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.toStdString());
  else {
    return false;
  }
  if (!warn.empty()) printf("Warn: %s\n", warn.c_str());
  if (!err.empty()) printf("Err: %s\n", err.c_str());
  if (!ret) {
    throw std::runtime_error("Failed to load glTF file(" + path.toStdString() +
                             "), error: " + err);
    return false;
  }
  for (auto i = 0; i < model.meshes.size(); ++i) {
    meshes.insert(
        std::make_pair(i, loadMesh(model.meshes[i])));
  }
  const tinygltf::Scene &scene = model.scenes[model.defaultScene];
  for (std::size_t i = 0; i < scene.nodes.size(); ++i) {
    loadNode(model.nodes[scene.nodes[i]], -1,
                 scene.nodes[i], nodes);
  }
  for (std::size_t i = 0; i < model.cameras.size(); ++i) {
    cameras.insert(std::make_pair(i, loadCamera(model.cameras[i])));
  }
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].mesh_id > -1) {
      meshes[nodes[i].mesh_id].matrix = nodes[i].matrix;
    }
  }
  for (std::size_t i = 0; i < model.textures.size(); ++i) {
    textures.insert(
        std::make_pair(i, loadTexture(model.textures[i])));
  }
  for (auto i = 0; i < model.animations.size(); ++i) {
    animations.insert(std::make_pair(i, loadAnimation(model.animations[i])));
  }
  return ret;
}

Mesh GltfModel::loadMesh(const tinygltf::Mesh &gltf_mesh) { 
    Mesh mesh;
  mesh.name = gltf_mesh.name;

  /* NOTE: Assume that there is only one primitive in a mesh */
  /* TODO: Modify it to work on more than one primitive. */
  for (const auto &primitive : gltf_mesh.primitives) {
    /* save material id */
    mesh.material_id = primitive.material;

    /* load indices data */
    {
      const auto &accessor    = model.accessors[primitive.indices];
      const auto &buffer_view = model.bufferViews[accessor.bufferView];
      const auto &buffer      = model.buffers[buffer_view.buffer];

      const auto data_address =
          buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
      const auto byte_stride = accessor.ByteStride(buffer_view);
      const auto count       = accessor.count;

      mesh.indices.clear();
      mesh.indices.reserve(count);
      if (accessor.type == TINYGLTF_TYPE_SCALAR) {
        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_BYTE ||
            accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
          for (std::size_t i = 0; i < count; ++i)
            mesh.indices.emplace_back(*(reinterpret_cast<const char *>(
                data_address + i * byte_stride)));
        } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT ||
                   accessor.componentType ==
                       TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
          for (std::size_t i = 0; i < count; ++i)
            mesh.indices.emplace_back(*(reinterpret_cast<const short *>(
                data_address + i * byte_stride)));
        } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_INT ||
                   accessor.componentType ==
                       TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
          for (std::size_t i = 0; i < count; ++i)
            mesh.indices.emplace_back(*(
                reinterpret_cast<const int *>(data_address + i * byte_stride)));
        } else {
          throw std::runtime_error("Undefined indices component type.");
        }
      } else {
        throw std::runtime_error("Undefined indices type.");
      }
    }

    if (mesh.indices.size() > 0) {
      /* converts an indices array into the 'TRIANGLES' mode indices array */
      if (primitive.mode == TINYGLTF_MODE_TRIANGLES) { /* empty */
      } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN) {
        auto triangle_fan = std::move(mesh.indices);
        mesh.indices.clear();

        for (std::size_t i = 2; i < triangle_fan.size(); ++i) {
          mesh.indices.push_back(triangle_fan[0]);
          mesh.indices.push_back(triangle_fan[i - 1]);
          mesh.indices.push_back(triangle_fan[i - 0]);
        }
      } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
        auto triangle_strip = std::move(mesh.indices);
        mesh.indices.clear();

        for (std::size_t i = 2; i < triangle_strip.size(); ++i) {
          mesh.indices.push_back(triangle_strip[i - 2]);
          mesh.indices.push_back(triangle_strip[i - 1]);
          mesh.indices.push_back(triangle_strip[i - 0]);
        }
      } else {
        throw std::runtime_error("Undefined primitive mode.");
      }
    }

    mesh.vertices.resize(mesh.indices.size());
    for (const auto &attribute : primitive.attributes) {
      /* load vertices data */
      /* NOTE: render witout using an index buffer      */
      /* TODO: change to use index buffer for rendering */
      {
        const auto &accessor    = model.accessors[attribute.second];
        const auto &buffer_view = model.bufferViews[accessor.bufferView];
        const auto &buffer      = model.buffers[buffer_view.buffer];

        const auto data_address =
            buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
        const auto byte_stride = accessor.ByteStride(buffer_view);

        if (attribute.first == "POSITION") {
          if (accessor.type == TINYGLTF_TYPE_VEC3) {
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
              for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
                auto face                   = mesh.indices[i];
                mesh.vertices[i].position.x = *(reinterpret_cast<const float *>(
                    data_address + 0 * sizeof(float) + face * byte_stride));
                mesh.vertices[i].position.y = *(reinterpret_cast<const float *>(
                    data_address + 1 * sizeof(float) + face * byte_stride));
                mesh.vertices[i].position.z = *(reinterpret_cast<const float *>(
                    data_address + 2 * sizeof(float) + face * byte_stride));
              }
            } 
            //else if (accessor.componentType ==
            //           TINYGLTF_COMPONENT_TYPE_DOUBLE) {
            //  for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
            //    auto face = mesh.indices[i];
            //    mesh.vertices[i].position.x =
            //        *(reinterpret_cast<const double *>(data_address +
            //                                           0 * sizeof(double) +
            //                                           face * byte_stride));
            //    mesh.vertices[i].position.y =
            //        *(reinterpret_cast<const double *>(data_address +
            //                                           1 * sizeof(double) +
            //                                           face * byte_stride));
            //    mesh.vertices[i].position.z =
            //        *(reinterpret_cast<const double *>(data_address +
            //                                           2 * sizeof(double) +
            //                                           face * byte_stride));
            //  }
            //} 
            else {
              throw std::runtime_error(
                  "Undefined \'POSITION\' attribute component type.");
            }
          } else {
            throw std::runtime_error("Undefined \'POSITION\' attribute type.");
          }
        }  // endif 'POSITION'
        else if (attribute.first == "NORMAL") {
          if (accessor.type == TINYGLTF_TYPE_VEC3) {
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
              for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
                auto face                 = mesh.indices[i];
                mesh.vertices[i].normal.x = *(reinterpret_cast<const float *>(
                    data_address + 0 * sizeof(float) + face * byte_stride));
                mesh.vertices[i].normal.y = *(reinterpret_cast<const float *>(
                    data_address + 1 * sizeof(float) + face * byte_stride));
                mesh.vertices[i].normal.z = *(reinterpret_cast<const float *>(
                    data_address + 2 * sizeof(float) + face * byte_stride));
              }
            } 
            //else if (accessor.componentType ==
            //           TINYGLTF_COMPONENT_TYPE_DOUBLE) {
            //  for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
            //    auto face                 = mesh.indices[i];
            //    mesh.vertices[i].normal.x = *(reinterpret_cast<const double *>(
            //        data_address + 0 * sizeof(double) + face * byte_stride));
            //    mesh.vertices[i].normal.y = *(reinterpret_cast<const double *>(
            //        data_address + 1 * sizeof(double) + face * byte_stride));
            //    mesh.vertices[i].normal.z = *(reinterpret_cast<const double *>(
            //        data_address + 2 * sizeof(double) + face * byte_stride));
            //  }
            //} 
            else {
              throw std::runtime_error(
                  "Undefined \'NORMAL\' attribute component type.");
            }
          } else {
            throw std::runtime_error("Undefined \'NORMAL\' attribute type.");
          }
        }  // endif 'NORMAL'
        else if (attribute.first == "TEXCOORD_0") {
          if (accessor.type == TINYGLTF_TYPE_VEC2) {
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
              for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
                auto face                   = mesh.indices[i];
                mesh.vertices[i].texcoord.x = *(reinterpret_cast<const float *>(
                    data_address + 0 * sizeof(float) + face * byte_stride));
                mesh.vertices[i].texcoord.y = *(reinterpret_cast<const float *>(
                    data_address + 1 * sizeof(float) + face * byte_stride));
              }
            } 
            //else if (accessor.componentType ==
            //           TINYGLTF_COMPONENT_TYPE_DOUBLE) {
            //  for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
            //    auto face = mesh.indices[i];
            //    mesh.vertices[i].texcoord.x =
            //        *(reinterpret_cast<const double *>(data_address +
            //                                           0 * sizeof(double) +
            //                                           face * byte_stride));
            //    mesh.vertices[i].texcoord.y =
            //        *(reinterpret_cast<const double *>(data_address +
            //                                           1 * sizeof(double) +
            //                                           face * byte_stride));
            //  }
            //} 
            else {
              throw std::runtime_error(
                  "Undefined \'TEXCOORD_0\' attribute component type.");
            }
          } else {
            throw std::runtime_error(
                "Undefined \'TEXCOORD_0\' attribute type.");
          }
        }  // endif 'TEXCOORD_0'
        else if (attribute.first == "JOINTS_0") {
          if (accessor.type == TINYGLTF_TYPE_VEC4) {
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT ||
                accessor.componentType ==
                    TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
              for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
                auto face                = mesh.indices[i];
                mesh.vertices[i].joint.x = *(reinterpret_cast<const short *>(
                    data_address + 0 * sizeof(short) + face * byte_stride));
                mesh.vertices[i].joint.y = *(reinterpret_cast<const short *>(
                    data_address + 1 * sizeof(short) + face * byte_stride));
                mesh.vertices[i].joint.z = *(reinterpret_cast<const short *>(
                    data_address + 2 * sizeof(short) + face * byte_stride));
                mesh.vertices[i].joint.w = *(reinterpret_cast<const short *>(
                    data_address + 3 * sizeof(short) + face * byte_stride));
              }
            } else {
              throw std::runtime_error(
                  "Undefined \'JOINTS_0\' attribute component type.");
            }
          } else {
            throw std::runtime_error("Undefined \'JOINTS_0\' attribute type.");
          }
        }  // endif 'JOINTS_0'
        else if (attribute.first == "WEIGHTS_0") {
          if (accessor.type == TINYGLTF_TYPE_VEC4) {
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
              for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
                auto face                 = mesh.indices[i];
                mesh.vertices[i].weight.x = *(reinterpret_cast<const float *>(
                    data_address + 0 * sizeof(float) + face * byte_stride));
                mesh.vertices[i].weight.y = *(reinterpret_cast<const float *>(
                    data_address + 1 * sizeof(float) + face * byte_stride));
                mesh.vertices[i].weight.z = *(reinterpret_cast<const float *>(
                    data_address + 2 * sizeof(float) + face * byte_stride));
                mesh.vertices[i].weight.w = *(reinterpret_cast<const float *>(
                    data_address + 3 * sizeof(float) + face * byte_stride));
              }
            } 
            //else if (accessor.componentType ==
            //           TINYGLTF_COMPONENT_TYPE_DOUBLE) {
            //  for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
            //    auto face                 = mesh.indices[i];
            //    mesh.vertices[i].weight.x = *(reinterpret_cast<const double *>(
            //        data_address + 0 * sizeof(double) + face * byte_stride));
            //    mesh.vertices[i].weight.y = *(reinterpret_cast<const double *>(
            //        data_address + 1 * sizeof(double) + face * byte_stride));
            //    mesh.vertices[i].weight.z = *(reinterpret_cast<const double *>(
            //        data_address + 2 * sizeof(double) + face * byte_stride));
            //    mesh.vertices[i].weight.w = *(reinterpret_cast<const double *>(
            //        data_address + 3 * sizeof(double) + face * byte_stride));
            //  }
            //} 
            else {
              throw std::runtime_error(
                  "Undefined \'WEIGHTS_0\' attribute type.");
            }
          } else {
            throw std::runtime_error("Undefined \'WEIGHTS_0\' attribute type.");
          }
        }  // endif 'WEIGHTS_0'
      }
    }  // for each attribute in primitive
  }    // for each primitive in glTF mesh

  return mesh;
}

void GltfModel::loadNode(const tinygltf::Node &gltf_node, int parent_id,
                         int current_id, std::map<int, Node> &nodes) {
  Node node;
  node.name      = gltf_node.name;
  node.child_ids = gltf_node.children;
  node.node_id   = current_id;
  node.parent_id = parent_id;
  node.mesh_id   = gltf_node.mesh;
  node.skin_id   = gltf_node.skin;
  node.camera_id = gltf_node.camera;

  /* save matrix data */
  if (gltf_node.matrix.size() == 16) {
    ///node.matrix = orca::MakeMatrix4X4<float>(gltf_node.matrix.data());
    // https://codereview.stackexchange.com/questions/144381/4x4-matrix-implementation-in-c
    // https://github.com/HK416/glTF_Animation/blob/master/external/include/orca/matrix_functions.hpp
    //node.matrix = {{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}, {12, 13, 14, 15}};
    node.matrix = {{gltf_node.matrix[0], gltf_node.matrix[1], gltf_node.matrix[2], gltf_node.matrix[3]},
                   {gltf_node.matrix[4], gltf_node.matrix[5], gltf_node.matrix[6], gltf_node.matrix[7]},
                   {gltf_node.matrix[8], gltf_node.matrix[9], gltf_node.matrix[10], gltf_node.matrix[11]},
                   {gltf_node.matrix[12], gltf_node.matrix[13], gltf_node.matrix[14], gltf_node.matrix[15]}};
  }

  /* save translation data */
  if (gltf_node.translation.size() == 3) {
    // node.translate = orca::MakeVector3<float>(gltf_node.translation.data());
    vec3f v        = {gltf_node.translation[0], gltf_node.translation[1],
               gltf_node.translation[2]};
    node.translate = v;
  }

  /* save rotation data */
  if (gltf_node.rotation.size() == 4) {
    /* store quaternion information in vec4 type */
    //node.rotate = orca::MakeVector4<float>(gltf_node.rotation.data());
    vec3f v     = {gltf_node.rotation[0], gltf_node.rotation[1],
               gltf_node.rotation[2]};
    node.rotate = v;
  }

  /* save sacle data */
  if (gltf_node.scale.size() == 3) {
    //node.scale = orca::MakeVector3<float>(gltf_node.scale.data());
    vec3f v = {gltf_node.scale[0], gltf_node.scale[1], gltf_node.scale[2]};
    node.scale = v;
  }

  /* save the loaded node to the node map */
  nodes.insert(std::make_pair(current_id, node));

  /* stores child nodes */
  for (std::size_t i = 0; i < node.child_ids.size(); ++i) {
    loadNode(model.nodes[node.child_ids[i]], current_id,
                 node.child_ids[i], nodes);
  }
}

Texture GltfModel::loadTexture(const tinygltf::Texture &gltf_texture) {
  Texture texture;
  /* save the glTF image */
  {
    const tinygltf::Image &gltf_image = model.images[gltf_texture.source];
    texture.image.name                = gltf_image.uri;
    texture.image.width               = gltf_image.width;
    texture.image.height              = gltf_image.height;
    texture.image.component           = gltf_image.component;
    texture.image.data                = gltf_image.image;
  }
  ///* save the glTF image sampler */
  //{
  //  if (gltf_texture.sampler > -1) {
  //    const tinygltf::Sampler &gltf_sampler =
  //        gltf_model.samplers[gltf_texture.sampler];
  //    texture.sampler.name       = gltf_sampler.name;
  //    texture.sampler.min_filter = GetFilterMode(gltf_sampler.minFilter);
  //    texture.sampler.mag_filter = GetFilterMode(gltf_sampler.magFilter);
  //    texture.sampler.wrap_R     = GetWrapMode(gltf_sampler.wrapR);
  //    texture.sampler.wrap_S     = GetWrapMode(gltf_sampler.wrapS);
  //    texture.sampler.wrap_T     = GetWrapMode(gltf_sampler.wrapT);
  //  }
  //}
  return texture;
}

Camera GltfModel::loadCamera(const tinygltf::Camera &gltf_camera) {
  Camera camera;
  if (gltf_camera.type == "perspective") {
    camera.camera_type = CAMERA_TYPE::PERSPECTIVE;
    camera.aspectRatio = gltf_camera.perspective.aspectRatio;
    camera.yfov = gltf_camera.perspective.yfov;
  } else if (gltf_camera.type == "orthographic") {
    camera.camera_type = CAMERA_TYPE::ORTHOGRAPHIC;
    camera.xmag        = gltf_camera.orthographic.xmag;
    camera.ymag        = gltf_camera.orthographic.ymag;
  } else {
    camera.camera_type = CAMERA_TYPE::UNKNOWN;
    throw std::runtime_error("UNKNOWN camera type.");
  }
  return camera;
}

Animation GltfModel::loadAnimation(const tinygltf::Animation &anim) {
  // https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/base/VulkanglTFModel.cpp
  // https://github.com/HK416/glTF_Animation/blob/master/src/src/Model/model.cpp
  // bug in blender 3.2 gltf exporter !? https://github.com/KhronosGroup/glTF-Blender-IO/pull/1679
  Animation animation;
  animation.name = anim.name;
  animation.samplers.reserve(anim.samplers.size());
  for (auto sampler : anim.samplers) {
    AnimationSampler anim_sampler;
    if (sampler.interpolation == "LINEAR")
      anim_sampler.interpolation = INTERPOLATION_TYPE::LINEAR;
    else if (sampler.interpolation == "STEP")
      anim_sampler.interpolation = INTERPOLATION_TYPE::STEP;
    else if (sampler.interpolation == "CUBICSPLINE")
      anim_sampler.interpolation = INTERPOLATION_TYPE::CUBICSPLINE;
    else
      anim_sampler.interpolation = INTERPOLATION_TYPE::UNKNOWN;
    // sampler input time values
    //std::vector<float> frames;
    //std::vector<std::vector<float>> transforms;
    int interpolation;
    // Read sampler input time values
    {
      const tinygltf::Accessor &accessor = model.accessors[sampler.input];
      if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        throw std::runtime_error(
            "Warning::animation sampler inputs type is not float.");
      }
      const tinygltf::BufferView &bufferView =
          model.bufferViews[accessor.bufferView];
      const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
      const void *dataPtr =
          &buffer.data[accessor.byteOffset + bufferView.byteOffset];
      const float *buf = static_cast<const float *>(dataPtr);
      // anim_sampler.inputs.reserve(accessor.count);
      for (size_t index = 0; index < accessor.count; index++) {
        anim_sampler.inputs.push_back(buf[index]);
        // frames.push_back(buf[index]);  // buf[index] is a frame value in
        // float ?
      }
    }
    for (const auto &input : anim_sampler.inputs) {
      if (input < animation.start_time) animation.start_time = input;
      if (input > animation.end_time) animation.end_time = input;
    }
    // Read sampler output T/R/S values
    {
      const tinygltf::Accessor &accessor = model.accessors[sampler.output];
      const tinygltf::BufferView &bufferView =
          model.bufferViews[accessor.bufferView];
      const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
      const void *dataPtr =
          &buffer.data[accessor.byteOffset + bufferView.byteOffset];
      const auto data_address =
          buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
      const auto byte_stride = accessor.ByteStride(bufferView);
      switch (accessor.type) {
      case TINYGLTF_TYPE_VEC3:
        // const glm::vec3 *buf = static_cast<const glm::vec3 *>(dataPtr);
        anim_sampler.outputs.reserve(accessor.count);
        for (size_t i = 0; i < accessor.count; i++) {
          float x = *(reinterpret_cast<const float *>(
              data_address + 0 * sizeof(float) + i * byte_stride));
          float y = *(reinterpret_cast<const float *>(
              data_address + 1 * sizeof(float) + i * byte_stride));
          float z = *(reinterpret_cast<const float *>(
              data_address + 2 * sizeof(float) + i * byte_stride));
          // std::vector<float> transform = {x,y,z};
          // transforms.push_back(transform);
          
          //anim_sampler.outputs.emplace_back(x, y, z, 0);
          vec4f v = {x, y, z, 0};
          anim_sampler.outputs.emplace_back(v);
        }
        break;
      case TINYGLTF_TYPE_VEC4:
        // const glm::vec3 *buf = static_cast<const glm::vec3 *>(dataPtr);
        anim_sampler.outputs.reserve(accessor.count);
        for (size_t i = 0; i < accessor.count; i++) {
          float x = *(reinterpret_cast<const float *>(
              data_address + 0 * sizeof(float) + i * byte_stride));
          float y = *(reinterpret_cast<const float *>(
              data_address + 1 * sizeof(float) + i * byte_stride));
          float z = *(reinterpret_cast<const float *>(
              data_address + 2 * sizeof(float) + i * byte_stride));
          float w = *(reinterpret_cast<const float *>(
              data_address + 3 * sizeof(float) + i * byte_stride));
          //anim_sampler.outputs.emplace_back(x, y, z, w);
          vec4f v = {x, y, z, w};
          anim_sampler.outputs.emplace_back(v);
        }
        break;
      case TINYGLTF_TYPE_SCALAR:
        // const glm::vec3 *buf = static_cast<const glm::vec3 *>(dataPtr);
        anim_sampler.outputs.reserve(accessor.count);
        for (size_t i = 0; i < accessor.count; i++) {
          float x = *(reinterpret_cast<const float *>(data_address + i * byte_stride));
          //anim_sampler.outputs.emplace_back(x, 0, 0, 0);
          vec4f v = {x, 0, 0, 0};
          anim_sampler.outputs.emplace_back(v);
        }
        break;
      default:
        std::cout << "unknown type" << std::endl;
        break;
      }
    }
    animation.samplers.emplace_back(anim_sampler);
  }
  animation.channels.reserve(anim.channels.size());
  for (auto channel : anim.channels) {
    AnimationChannel anim_channel;
    if (channel.target_path == "rotation")
      anim_channel.path_type = PATH_TYPE::ROTATION;
    else if (channel.target_path == "translation")
      anim_channel.path_type = PATH_TYPE::TRANSLATION;
    else if (channel.target_path == "scale")
      anim_channel.path_type = PATH_TYPE::SCALE;
    else if (channel.target_path == "weights")
      anim_channel.path_type = PATH_TYPE::WEIGHTS;
    else
      anim_channel.path_type = PATH_TYPE::UNKNOWN;
    anim_channel.node_id    = channel.target_node;
    anim_channel.sampler_id = channel.sampler;
    animation.channels.emplace_back(anim_channel);
  }
  return animation;
}
