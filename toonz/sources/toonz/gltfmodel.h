#ifndef GLTFMODEL_H
#define GLTFMODEL_H
#include <QString>

//#define TINYGLTF_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
//#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

//avoiding include glm...
typedef struct vec2f {
  float x, y;
} vec2f;
typedef struct vec3f {
  float x, y, z;
} vec3f;
typedef struct vec4f {
  float x, y, z, w;
} vec4f;
typedef struct vec4ui {
  unsigned int x, y, z, w;
} vec4ui;

class Vertex {
public:
  Vertex();
  Vertex(const Vertex& other);

public:
  vec3f position;
  vec3f normal;
  vec2f texcoord;
  vec4ui joint;
  vec4f weight;
};
constexpr unsigned int MAX_NUM_JOINTS = 128U;
class Mesh {
public:
  Mesh();
  Mesh(const Mesh& other);

public:
  //void SetupMesh();
  //void CleanupMesh();
  //void Render(unsigned int shader_program, bool is_animation);

public:
  std::string name;
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  int material_id;
  std::vector<std::vector<double>> matrix; // should we use TAffine ? (see tgeometry.h)
  std::array<std::vector<std::vector<float>>, MAX_NUM_JOINTS> joint_matrices;

private:
  unsigned int vao;
  unsigned int vbo;
  unsigned int ebo;
};
enum class INTERPOLATION_TYPE {
  UNKNOWN,
  LINEAR,
  STEP,
  CUBICSPLINE
};  // enum class INTERPOLATION_TYPE

class AnimationSampler {
public:
  AnimationSampler();
  AnimationSampler(const AnimationSampler& other);

public:
  INTERPOLATION_TYPE interpolation;
  std::vector<float> inputs;
  std::vector<vec4f> outputs;
};

enum class PATH_TYPE {
  UNKNOWN,
  TRANSLATION,
  ROTATION,
  SCALE,
  WEIGHTS
};  // enum class PATH_TYPE

class AnimationChannel {
public:
  AnimationChannel();
  AnimationChannel(const AnimationChannel& other);

public:
  PATH_TYPE path_type;
  int node_id;
  int sampler_id;
};  // class AnimationChannel

class Animation {
public:
  Animation();
  Animation(const Animation& other);

public:
  std::string name;
  std::vector<AnimationSampler> samplers;
  std::vector<AnimationChannel> channels;
  float start_time;
  float end_time;
};  // class Animation
class Node {
public:
  Node();
  Node(const Node& other);

public:
  //orca::mat4<float> LocalMatrix() const;

public:
  std::string name;
  std::vector<int> child_ids;

  std::vector<std::vector<double>> matrix;
  // should we use TAffine ? (see tgeometry.h)
  vec3f translate;
  vec3f rotate;
  vec3f scale;

  int node_id;
  int parent_id;
  int mesh_id;
  int skin_id;
  int camera_id;
};

class Image {
public:
  Image();
  Image(const Image& other);

public:
  int width;
  int height;
  int component;
  std::vector<unsigned char> data;
  std::string name;
};

class Texture {
public:
  Texture();
  Texture(const Texture& other);

public:
  //void SetupTexture();
  //void CleanupTexture();
  //void BindTexture();

public:
  std::string name;
  Image image;
  //TextureSampler sampler;

private:
  unsigned int tbo;
};
enum class CAMERA_TYPE {
  UNKNOWN,
  ORTHOGRAPHIC,
  PERSPECTIVE
};  // enum class CAMERA_TYPE

class Camera {
public:
  Camera();
  Camera(const Camera& other);
  CAMERA_TYPE camera_type;
  float znear, zfar;
  float aspectRatio, yfov;
  float xmag, ymag;
};

class GltfModel {
public:
  bool exportGltf(QString path);
  bool importGltf(QString path);
  Camera loadCamera(const tinygltf::Camera& gltf_camera);
  Mesh loadMesh(const tinygltf::Mesh& gltf_mesh);
  void loadNode(const tinygltf::Node& gltf_node, int parent_id,
                    int current_id, std::map<int, Node>& nodes);
  Texture loadTexture(const tinygltf::Texture& gltf_texture);
  Animation loadAnimation(const tinygltf::Animation& gltf_animation);

  tinygltf::Model model;
  std::map<int, Mesh> meshes;
  std::map<int, Node> nodes;
  std::map<int, Animation> animations;
  std::map<int, Texture> textures;
  std::map<int, Camera> cameras;
  // std::map<int, Skin> skins;
  // std::map<int, Material> materials;
private:
};
#endif  // !GLTFMODEL_H
