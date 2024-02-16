/*

* DONE
- ImportGltfCommand... (wip)
- apply keyframes animations to columns (camera and levels) with matching node name
- EaseInOut, Linear and Constant interpolations
- convert full quaternion rotation to euler roll only

* TODO
- apply transformations to camera & levels when there is no keyframe animation
- apply camera settings...
- ExportGltfCommand...
- define lengthMultiplier applies to translations (why 16 ? 16 fields ? 16 inches ?)

* REFERENCE
> gltf animations
sampler inputs : time in second
sampler output : value in step & linear interpolations, intangnent,
value, outtangent in cubicstep interpolation see
https://github.com/KhronosGroup/glTF/issues/1812
https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_animation_sampler_interpolation
EaseInOut only use flatten handles
Blender's bezier spline equivalent would be reproduced by
selecting all key and doing keys>snap>flatten handles SpeedInOut
can use non flat tangent, so it's probably a better equivalent to
Blender's bezier spline
But gltf cubic splines only use 1 value per tangent : only x, no y ,
so only flat tangent ? If so, we should use EaseInOut
but what about y ??
> we should stick to flat tangent and thus easeInOut ...
https://github.com/KhronosGroup/glTF-Blender-IO/issues/960
https://github.com/KhronosGroup/glTF/issues/1778
https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-c-interpolation

> gltf coordinates
glTF uses a right-handed coordinate system. glTF defines +Y as up, +Z as
forward, and -X as right; the front of a glTF asset faces +Z.
The units for all linear distances are meters. All angles are in radians.
opentoonz unit could be changed, but not tahoma. From the
TCamera::getStageToCameraRef() code, it looks like stage inch is used to convert
from camera to screen coordinates ?



* tracks to explore in toonz code:
> keyframes
// tests...
cameraObject->setKeyframeWithoutUndo(frame, kf); // not working
// WORKING! set one keyframe:
auto param = cameraObject->getParam(TStageObject::Channel::T_X);
TDoubleKeyframe dkf;
dkf.m_value = 0.2;
dkf.m_type  = TDoubleKeyframe::Type::Linear;
dkf.m_frame = 1;
param->setKeyframe(dkf);
// added a new methods :
addKeyframe(0.2, TDoubleKeyframe::Type::Linear, 1, cameraObject, TStageObject::Channel::T_Y);

- tstageobject.h : class DVAPI TStageObject
- class DVAPI TStageObjectParams
- void TStageObject::updateKeyframes(LazyData &ld)
- return pointer to TStageObject params :
TDoubleParam *TStageObject::getParam(Channel channel)
- wtf
void TStageObject::setParam(Channel type, double frame, double val) {
  assert(0);
}
- used this, that did nothing ;
void TStageObject::setKeyframeWithoutUndo(int frame,
                                          const TStageObject::Keyframe &k)
- TStageObject::LazyData
- doubleparamcmd.h
class DVAPI KeyframeSetter :
  static void setValue(TDoubleParam *curve, double frame, double value) {
    KeyframeSetter setter(curve);
    setter.createKeyframe(frame);
    setter.setValue(value);
  }
// see KeyframeSetter, void TDoubleParam::setKeyframe(int index, const
TDoubleKeyframe &k) setKeyframes void insertFrame(TDoubleParam *param, int
frame)
  // void TStageObject::setKeyframeWithoutUndo(int frame, const
TStageObject::Keyframe &k)
  // TDoubleParam *curve = m_sheet->getCurve(m_editCol); FunctionSheetCellViewer
KeyframeSetter KeyframeSetter(curve).createKeyframe(row);

> camera
  // auto camera       = scene->getCurrentCamera();
  // see SceneViewer::getPreviewRect()
*/

#include "gltfio.h"
#include <iostream>
#include <string>
#include <QString>
#include "gltfmodel.h"

#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "filebrowserpopup.h"

#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/tcamera.h"
#include "toonz/txsheethandle.h"
#include "toonz/tstageobject.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tframehandle.h"
#include "toutputproperties.h"
#include "toonz/levelset.h"

// keyframes
static void addKeyframe(double value, TDoubleKeyframe::Type type, int frame,
                        TStageObject *object, TStageObject::Channel channel) {
  TDoubleKeyframe dkf;
  dkf.m_value = value;
  dkf.m_type  = type;
  dkf.m_frame = frame;
  auto param  = object->getParam(channel);
  param->setKeyframe(dkf);
  std::string message = "addKeyframe: " + std::to_string(value) + " " +
                        std::to_string(frame) + " " + object->getName();
  DVGui::info(QString::fromStdString(message));
}
static void addKeyframes(const std::vector<TDoubleKeyframe> &ks,
                         TStageObject *object, TStageObject::Channel channel) {
  auto param = object->getParam(channel);
  for (auto k : ks) {
    addKeyframe(k.m_value, k.m_type, k.m_frame, object, channel);
  }
}
static void setKeyframes(const std::map<int, TDoubleKeyframe> &ks,
                         TStageObject *object, TStageObject::Channel channel) {
  auto param = object->getParam(channel);
  for (auto k : ks) {
    if (param->getKeyframeCount() < k.first) {
      DVGui::info("setKeyframes only changes existing keyframes...");
      return;
    }
  }
  param->setKeyframes(ks);
}
// rotations
struct Quaternion {
  double w, x, y, z;
};

struct EulerAngles {
  double roll, pitch, yaw;
};
EulerAngles ToEulerAngles(Quaternion q) {
  EulerAngles angles;

  // roll (x-axis rotation)
  double sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
  double cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
  angles.roll      = std::atan2(sinr_cosp, cosr_cosp);

  // pitch (y-axis rotation)
  double sinp  = std::sqrt(1 + 2 * (q.w * q.y - q.x * q.z));
  double cosp  = std::sqrt(1 - 2 * (q.w * q.y - q.x * q.z));
  angles.pitch = 2 * std::atan2(sinp, cosp) - M_PI / 2;

  // yaw (z-axis rotation)
  double siny_cosp = 2 * (q.w * q.z + q.x * q.y);
  double cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
  angles.yaw       = std::atan2(siny_cosp, cosy_cosp);

  return angles;
}
float radToDeg(float radians) {
  return radians * (180.0 / M_PI);
}

float quaternionToRollDegree(float w, float x, float y, float z) {
  Quaternion q = {w, x, y, z};
  EulerAngles angles = ToEulerAngles(q);
  return radToDeg(angles.roll);
}
// stageObjects. 
// Use with :
// auto stageObject = xsheet->getStageObject(columnObjId);
// not to be used with camera !
bool findColumnId(std::string name, TStageObjectId &id) {
  auto xsheet    = TApp::instance()->getCurrentXsheet()->getXsheet();
  auto scene    = TApp::instance()->getCurrentScene()->getScene();
  auto levelSet = scene->getLevelSet();
  auto levelName = QString::fromStdString(name).toStdWString();
  int result    = -1;
  for (int columnIndex = 0; columnIndex < xsheet->getColumnCount();
       columnIndex++) {
    // auto level  = levelSet->getLevel(nodeName);
    auto level = levelSet->getLevel(columnIndex); 
    if (level->getName() == levelName) {
      auto column = xsheet->getColumn(columnIndex);
      TStageObjectId columnObjId =
          (columnIndex >= 0)
              ? TStageObjectId::ColumnId(columnIndex)
              : TStageObjectId::CameraId(xsheet->getCameraColumnIndex());
      id = columnObjId;
      return true;
    }
  }
  return false;
}

class ExportGltfCommand final : public MenuItemHandler {
public:
  ExportGltfCommand() : MenuItemHandler(MI_ExportGltf) {}
  void execute() override;
} exportGltfCommand;

void ExportGltfCommand::execute() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath fp      = scene->getScenePath().withType("gltf");
  static GenericSaveFilePopup *savePopup = 0;
  savePopup                              = new GenericSaveFilePopup(
      QObject::tr("Export GL Transmission Format (glTF)"));
  savePopup->addFilterType("gltf");
  savePopup->addFilterType("glb");
  fp = savePopup->getPath();
  if (fp.isEmpty()) return;
  GltfModel model;
  // TStageObject::KeyframeMap keyframes;
  // cameraObject->getKeyframes(keyframes);
  model.exportGltf(fp.getQString());
}


class ImportGltfCommand final : public MenuItemHandler {
public:
  ImportGltfCommand() : MenuItemHandler(MI_ImportGltf) {}
  void execute() override;
} importGltfCommand;


void ImportGltfCommand::execute() {
  auto app     = TApp::instance();
  auto scene   = app->getCurrentScene()->getScene();
  auto props   = scene->getProperties()->getOutputProperties();
  float fps    = props->getFrameRate();
  TFilePath fp = scene->getScenePath().withType("gltf");
  static GenericLoadFilePopup *loadPopup = 0;
  loadPopup                              = new GenericLoadFilePopup(
      QObject::tr("Import GL Transmission Format (glTF)"));
  loadPopup->addFilterType("gltf");
  loadPopup->addFilterType("glb");
  fp = loadPopup->getPath();
  if (fp.isEmpty()) return;
  GltfModel model;
  model.importGltf(fp.getQString());

  // DONE : apply camera & levels keyframes
  // TODO : apply camera & levels transformations when there is no animation
  // TODO : apply camera settings...
  auto xsheet = app->getCurrentXsheet()->getXsheet();
  TStageObject *stageObject;
  auto cameraId      = xsheet->getStageObjectTree()->getCurrentCameraId();
  auto cameraObject = xsheet->getStageObject(cameraId);
  int cameraColumnId = xsheet->getCameraColumnIndex();
  int row = app->getCurrentFrame()->getFrame();
  auto levelSet      = scene->getLevelSet();
  int cameraNodeId = -1;
  std::map<std::string, TStageObject *> tColumns;
  for (auto node : model.nodes) {
    if (node.second.camera_id > -1 &&
        node.second.camera_id < model.cameras.size()) {
      auto cameraNode = node.second;
      DVGui::info("ImportGltfCommand: node found in the scene: " +
                  QString::fromStdString(cameraNode.name));
      cameraNodeId = cameraNode.node_id;
      // TODO: apply camera TRS transformations here...
      // don't use ! void TStageObject::setParam(Channel type, double frame,
      // double val) { assert(0); }
      break; // only use the first camera found ???
    }
  }
  for (auto anim : model.animations) {
    if (anim.second.channels[0].node_id == cameraNodeId) {
      stageObject = cameraObject;
    } else {
      // find the node this anim belongs to
      auto animNode = model.nodes[anim.second.channels[0].node_id];
      // find scene element (most likely, a level) with matching name
      auto nodeName = QString::fromStdString(animNode.name).toStdWString();
      TStageObjectId columnObjId;
      if (findColumnId(animNode.name, columnObjId)) {
        DVGui::info("ImportGltfCommand: node found in the scene: " +
                    QString::fromStdString(animNode.name));
        stageObject = xsheet->getStageObject(columnObjId);
      }
      else {
        DVGui::info("ImportGltfCommand: node not found in the scene: " +
                    QString::fromStdString(animNode.name));
        continue;
      }
    }
    auto animation = anim.second;
    std::vector<TDoubleKeyframe> tx;
    std::vector<TDoubleKeyframe> ty;
    std::vector<TDoubleKeyframe> tz;
    std::vector<TDoubleKeyframe> angle;
    std::vector<TDoubleKeyframe> scale;
    for (auto channel : animation.channels) {
      auto sampler = animation.samplers[channel.sampler_id];
      if (sampler.inputs.size() > sampler.outputs.size()) continue;
      // sampler inputs : time in second
      // sampler output : value in step & linear interpolations, intangnent,
      // value, outtangent in cubicstep interpolation see
      // https://github.com/KhronosGroup/glTF/issues/1812
      // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_animation_sampler_interpolation
      float lengthMultiplier = 16.0;  // why 16 ? 16 fields ? 16 inches ?
      for (size_t i = 0; i < sampler.inputs.size(); i++) {
        int outputIndex = i;
        float frame     = (sampler.inputs[i] * fps) - 1;
        TDoubleKeyframe keyframe;
        keyframe.m_frame = frame;
        if (channel.path_type == PATH_TYPE::TRANSLATION) {
          if (sampler.interpolation == INTERPOLATION_TYPE::CUBICSPLINE) {
            keyframe.m_type = TDoubleKeyframe::Type::EaseInOut;
            outputIndex     = (i * 3) + 1;
            // x
            keyframe.m_value =
                sampler.outputs[outputIndex].x * lengthMultiplier;
            if (outputIndex > 0)
              keyframe.m_speedIn.x = sampler.outputs[outputIndex - 1].x;
            else
              keyframe.m_speedIn.x = 0;
            if (outputIndex < sampler.outputs.size() - 1)
              keyframe.m_speedOut.x = sampler.outputs[outputIndex + 1].x;
            else
              keyframe.m_speedOut.x = 0;
            tx.push_back(keyframe);
            // y
            keyframe.m_value =
                sampler.outputs[outputIndex].y * lengthMultiplier;
            if (outputIndex > 0)
              keyframe.m_speedIn.x = sampler.outputs[outputIndex - 1].y;
            else
              keyframe.m_speedIn.x = 0;
            if (outputIndex < sampler.outputs.size() - 1)
              keyframe.m_speedOut.x = sampler.outputs[outputIndex + 1].y;
            else
              keyframe.m_speedOut.x = 0;
            ty.push_back(keyframe);
            // z
            keyframe.m_value =
                sampler.outputs[outputIndex].z * lengthMultiplier;
            if (outputIndex > 0)
              keyframe.m_speedIn.x = sampler.outputs[outputIndex - 1].z;
            else
              keyframe.m_speedIn.x = 0;
            if (outputIndex < sampler.outputs.size() - 1)
              keyframe.m_speedOut.x = sampler.outputs[outputIndex + 1].z;
            else
              keyframe.m_speedOut.x = 0;
            tz.push_back(keyframe);
          } else {
            if (sampler.interpolation == INTERPOLATION_TYPE::LINEAR)
              keyframe.m_type = TDoubleKeyframe::Type::Linear;
            else if (sampler.interpolation == INTERPOLATION_TYPE::STEP)
              keyframe.m_type = TDoubleKeyframe::Type::Constant;
            outputIndex = i;
            keyframe.m_value =
                sampler.outputs[outputIndex].x * lengthMultiplier;
            tx.push_back(keyframe);
            keyframe.m_value =
                sampler.outputs[outputIndex].y * lengthMultiplier;
            ty.push_back(keyframe);
            keyframe.m_value =
                sampler.outputs[outputIndex].z * lengthMultiplier;
            tz.push_back(keyframe);
          }
        } else if (channel.path_type == PATH_TYPE::ROTATION) {
          if (sampler.interpolation == INTERPOLATION_TYPE::CUBICSPLINE) {
              // using only z axis tangents ?
            keyframe.m_type = TDoubleKeyframe::Type::EaseInOut;
            outputIndex     = (i * 3) + 1;
            if (outputIndex > 0)
              keyframe.m_speedIn.x = sampler.outputs[outputIndex - 1].z;
            else
              keyframe.m_speedIn.x = 0;
            if (outputIndex < sampler.outputs.size() - 1)
              keyframe.m_speedOut.x = sampler.outputs[outputIndex + 1].z;
            else
              keyframe.m_speedOut.x = 0;
          } else {
            if (sampler.interpolation == INTERPOLATION_TYPE::LINEAR)
              keyframe.m_type = TDoubleKeyframe::Type::Linear;
            else if (sampler.interpolation == INTERPOLATION_TYPE::STEP)
              keyframe.m_type = TDoubleKeyframe::Type::Constant;
            outputIndex = i;
          }
          // convert full quaternion rotation to euler roll only
          keyframe.m_value = quaternionToRollDegree(
              sampler.outputs[outputIndex].w, sampler.outputs[outputIndex].x,
              sampler.outputs[outputIndex].y, sampler.outputs[outputIndex].z);
          angle.push_back(keyframe);
        } else if (channel.path_type == PATH_TYPE::SCALE) {
          if (sampler.interpolation == INTERPOLATION_TYPE::CUBICSPLINE) {
            keyframe.m_type = TDoubleKeyframe::Type::EaseInOut;
            outputIndex     = (i * 3) + 1;
            if (outputIndex > 0)
              keyframe.m_speedIn.x = sampler.outputs[outputIndex - 1].x;
            else
              keyframe.m_speedIn.x = 0;
            if (outputIndex < sampler.outputs.size() - 1)
              keyframe.m_speedOut.x = sampler.outputs[outputIndex + 1].x;
            else
              keyframe.m_speedOut.x = 0;
          } else {
            if (sampler.interpolation == INTERPOLATION_TYPE::LINEAR)
              keyframe.m_type = TDoubleKeyframe::Type::Linear;
            else if (sampler.interpolation == INTERPOLATION_TYPE::STEP)
              keyframe.m_type = TDoubleKeyframe::Type::Constant;
            outputIndex = i;
          }
          keyframe.m_value = sampler.outputs[outputIndex].x * lengthMultiplier;
          tx.push_back(keyframe);
          scale.push_back(keyframe);
        }
      }
      addKeyframes(tx, stageObject, TStageObject::Channel::T_X);
      addKeyframes(ty, stageObject, TStageObject::Channel::T_Y);
      addKeyframes(tz, stageObject, TStageObject::Channel::T_Z);
      addKeyframes(angle, stageObject, TStageObject::Channel::T_Angle);
    }
  }
  app->getCurrentXsheet()->notifyXsheetChanged();
}





