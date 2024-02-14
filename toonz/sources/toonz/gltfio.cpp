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
#include "toonz/tstageobjecttree.h"
#include "toonz/tframehandle.h"
#include "toutputproperties.h"

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
  int width    = scene->getCurrentCamera()->getRes().lx;
  int height    = scene->getCurrentCamera()->getRes().lx;
  int depth    = 16; //?
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

  // apply camera settings, transformations and keyframes
  // auto camera       = scene->getCurrentCamera();
  // see SceneViewer::getPreviewRect()
  auto xsheet        = app->getCurrentXsheet()->getXsheet();
  auto cameraId      = xsheet->getStageObjectTree()->getCurrentCameraId();
  auto cameraObject = xsheet->getStageObject(cameraId);
  int cameraColumnId = xsheet->getCameraColumnIndex();
  int row = app->getCurrentFrame()->getFrame();
  // apply camera TRS transformations
  int cameraNodeId = -1;
  for (auto node : model.nodes) {
    if (node.second.camera_id > -1 &&
        node.second.camera_id < model.cameras.size()) {
      auto cameraNode = node.second;
      std::string message = "Found camera node: " + cameraNode.name;
      DVGui::info(QString::fromStdString(message));
      cameraNodeId = cameraNode.node_id;
      cameraObject->setParam(TStageObject::Channel::T_X, row,
                             cameraNode.translate.x * width);
      cameraObject->setParam(TStageObject::Channel::T_Y, row,
                             cameraNode.translate.y * height);
      cameraObject->setParam(TStageObject::Channel::T_Z, row,
                             cameraNode.translate.z);
      cameraObject->setParam(TStageObject::Channel::T_Angle, row,
                             cameraNode.rotate.z);
      break; // only use the first camera found ???
    }
  }
  // apply camera animations
  // see KeyframeSetter, void TDoubleParam::setKeyframe(int index, const TDoubleKeyframe &k) setKeyframes void insertFrame(TDoubleParam *param, int frame) 
  // void TStageObject::setKeyframeWithoutUndo(int frame, const TStageObject::Keyframe &k)
  // TDoubleParam *curve = m_sheet->getCurve(m_editCol); FunctionSheetCellViewer KeyframeSetter KeyframeSetter(curve).createKeyframe(row);

  // glTF uses a right-handed coordinate system. glTF defines +Y as up, +Z as forward, and -X as right; the front of a glTF asset faces +Z.
  // The units for all linear distances are meters. All angles are in radians.
  // opentoonz unit could be changed, but not tahoma. From the TCamera::getStageToCameraRef() code, it looks like stage inch is used to convert from camera to screen coordinates ?
  for (auto anim : model.animations) {
    if (anim.second.channels[0].node_id == cameraNodeId) {
      auto animation = anim.second;
      //std::map<int, TDoubleKeyframe> keyframes;
      //std::map<int, TStageObject::Keyframe> keyframes;
      for (auto channel : animation.channels) {
        auto sampler = animation.samplers[channel.sampler_id];
        if (sampler.inputs.size() > sampler.outputs.size()) continue;
        for (size_t i = 0; i < sampler.inputs.size() - 1; i++) {
          // TDoubleKeyframe k; TDoubleKeyframe has no channel information...
          TDoubleKeyframe::Type kfType;
          switch (sampler.interpolation) {
          case INTERPOLATION_TYPE::LINEAR:
            kfType = TDoubleKeyframe::Type::Linear;
            break;
          case INTERPOLATION_TYPE::CUBICSPLINE:
            kfType = TDoubleKeyframe::Type::EaseInOut;
            break;
          case INTERPOLATION_TYPE::STEP:
            kfType = TDoubleKeyframe::Type::Constant;
            break;
          default:
            throw std::runtime_error("UNKNOWN keyframe interpolation");
            continue;
          }
          float frame = sampler.inputs[i] * fps;
          TStageObject::Keyframe kf;
          switch (channel.path_type) { 
          case PATH_TYPE::TRANSLATION:
            kf.m_channels[TStageObject::T_X].m_value =
                sampler.outputs[i].x * width;
            kf.m_channels[TStageObject::T_X].m_frame    = frame;
            kf.m_channels[TStageObject::T_X].m_type     = kfType;
            kf.m_channels[TStageObject::T_Y].m_value =
                sampler.outputs[i].y * height;
            kf.m_channels[TStageObject::T_Y].m_frame = frame;
            kf.m_channels[TStageObject::T_Y].m_type  = kfType;
            kf.m_channels[TStageObject::T_Z].m_value = sampler.outputs[i].z;
            kf.m_channels[TStageObject::T_Z].m_frame    = frame;
            kf.m_channels[TStageObject::T_Z].m_type  = kfType;
            break;
          case PATH_TYPE::ROTATION:
            kf.m_channels[TStageObject::T_Angle].m_value =
                sampler.outputs[i].z * depth;
            kf.m_channels[TStageObject::T_Angle].m_frame = frame;
            kf.m_channels[TStageObject::T_Angle].m_type  = kfType;
            break;
          case PATH_TYPE::SCALE:
            kf.m_channels[TStageObject::T_ScaleX].m_value =
                sampler.outputs[i].x;
            kf.m_channels[TStageObject::T_ScaleY].m_value =
                sampler.outputs[i].y;
            kf.m_channels[TStageObject::T_Angle].m_frame = frame;
            kf.m_channels[TStageObject::T_Angle].m_type  = kfType;
            break;
          }
          //keyframes.insert(keyframes.begin(), std::make_pair(i, kf));
          
          std::string message = 
              "setkeyframe on camera " + cameraObject->getName() 
              + " at frame " + std::to_string(frame) 
              + " animation name: " + animation.name 
              + " values: ";
          for (auto c : kf.m_channels)
            message += std::to_string(c.m_value) + " ";
          DVGui::info(QString::fromStdString(message));
          cameraObject->setKeyframeWithoutUndo(frame, kf);
        }
      }
    }
  }
  app->getCurrentXsheet()->notifyXsheetChanged();
}
