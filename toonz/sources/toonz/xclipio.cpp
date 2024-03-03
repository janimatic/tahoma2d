#include <iostream>
#include <string>
#include <QString>

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

#include "xclip/xclip.h"

//#define USE_QUAT

static void addKeyframes(const std::vector<TDoubleKeyframe> &ks,
                         TDoubleParam *param) {
  for (auto k : ks) {
    bool isBezierKey = k.m_type == TDoubleKeyframe::Type::SpeedInOut ||
                       k.m_type == TDoubleKeyframe::Type::EaseInOut ||
                       k.m_type == TDoubleKeyframe::Type::EaseInOutPercentage;
    TDoubleKeyframe dkf;
    dkf.m_value    = k.m_value;
    dkf.m_speedIn  = isBezierKey ? k.m_speedIn : TPointD(0, 0);
    dkf.m_speedOut = isBezierKey ? k.m_speedOut : TPointD(0, 0);
    dkf.m_type     = k.m_type;
    dkf.m_frame    = k.m_frame;
    param->setKeyframe(dkf);
  }
}

static void tDataToXTrack(std::vector<TDoubleKeyframe> &keys,
                               XTrack *track, float fps) {
  for (int i = 0; i < keys.size(); i++) {
    auto kf = keys[i];
    if (kf.m_type == TDoubleKeyframe::Type::SpeedInOut) {
      auto xkey          = new XBezierFloatKey();
      xkey->time         = float(kf.m_frame - 1) / fps;
      xkey->value        = kf.m_value;
      xkey->inTangent.x  = kf.m_speedIn.x;
      xkey->inTangent.y  = kf.m_speedIn.y;
      xkey->outTangent.x = kf.m_speedOut.x;
      xkey->outTangent.y = kf.m_speedOut.y;
      track->addKey(xkey);
    } else if (kf.m_type == TDoubleKeyframe::Type::EaseInOut ||
               kf.m_type == TDoubleKeyframe::Type::EaseInOutPercentage) {
      auto xkey          = new XBezierFloatKey();
      xkey->time         = float(kf.m_frame - 1) / fps;
      xkey->value        = kf.m_value;
      xkey->inTangent.x  = kf.m_speedIn.x;
      xkey->inTangent.y  = 0;
      xkey->outTangent.x = kf.m_speedOut.x;
      xkey->outTangent.y = 0;
      track->addKey(xkey);
    } else if (kf.m_type == TDoubleKeyframe::Type::Linear) {
      auto xkey   = new XLinearFloatKey();
      xkey->time  = float(kf.m_frame - 1) / fps;
      xkey->value = kf.m_value;
      track->addKey(xkey);
    } else if (kf.m_type == TDoubleKeyframe::Type::Constant) {
      auto xkey   = new XStepFloatKey();
      xkey->time  = float(kf.m_frame - 1) / fps;
      xkey->value = kf.m_value;
      track->addKey(xkey);
    }
  }
}
static void tParamToXTrack(TDoubleParam *param, XTrack *track, float fps) {
  track->value = param->getDefaultValue();
  std::vector<TDoubleKeyframe> keys;
  for (int i = 0; i < param->getKeyframeCount(); i++) {
    auto kf = param->getKeyframe(i);
    keys.push_back(kf);
  }
  tDataToXTrack(keys, track, fps);
}

// Build a vector of TDoubleKeyframe from XTrack keys
// use with addKeyframes(keys, stageObject->getParam(channel));
static void xTrackToTData(XTrack *track, std::vector<TDoubleKeyframe> &keys,
                               float fps) {
  for (auto key : track->getKeys()) {
    TDoubleKeyframe keyframe;
    keyframe.m_frame = (key->time * fps) + 1;
    if (key->TAG.find("KBFL") != std::string::npos) {
      keyframe.m_type       = TDoubleKeyframe::Type::SpeedInOut;
      auto xKey             = dynamic_cast<XBezierFloatKey *>(key);
      keyframe.m_value      = xKey->value;
      keyframe.m_speedIn.x  = xKey->inTangent.x;
      keyframe.m_speedIn.y  = xKey->inTangent.y;
      keyframe.m_speedOut.x = xKey->outTangent.x;
      keyframe.m_speedOut.y = xKey->outTangent.y;
      keys.push_back(keyframe);
    } else if (key->TAG.find("KLFL") != std::string::npos) {
      keyframe.m_type  = TDoubleKeyframe::Type::Linear;
      auto xKey        = dynamic_cast<XLinearFloatKey *>(key);
      keyframe.m_value = xKey->value;
      keys.push_back(keyframe);
    } else if (key->TAG.find("KSFL") != std::string::npos) {
      keyframe.m_type  = TDoubleKeyframe::Type::Constant;
      auto xKey        = dynamic_cast<XLinearFloatKey *>(key);
      keyframe.m_value = xKey->value;
      keys.push_back(keyframe);
    } else {
      DVGui::info("ImportXClipCommand: unsupported keyframe type: " +
                  QString::fromStdString(key->TAG) + " skipping...");
      continue;
    }
  }
}

// Find TStageObjectId by level name. Use with :
// auto stageObject = xsheet->getStageObject(columnObjId);
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
  if (name.find("Camera") != std::string::npos) {
    id = TStageObjectId::CameraId(xsheet->getCameraColumnIndex());
    return true;
  }
  return false;
}

static std::map<std::string, TStageObject *> getTNodes() {
  auto xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
  std::map<std::string, TStageObject *> result;
  TStageObject *stageObject;
  for (int col = 0; col < xsheet->getColumnCount(); col++) {
    stageObject = xsheet->getStageObject(TStageObjectId::ColumnId(col));
    result[stageObject->getName()] = stageObject;
  }
  auto cameraId     = xsheet->getStageObjectTree()->getCurrentCameraId();
  auto cameraObject = xsheet->getStageObject(cameraId);
  result[cameraObject->getName()] = cameraObject;
  return result;
}

class ExportXClipCommand final : public MenuItemHandler {
public:
  ExportXClipCommand() : MenuItemHandler(MI_ExportXClip) {}
  void execute() override;
} exportXClipCommand;

/// <summary>
/// xclip export
/// 
/// DONE :
/// * export keyframes
/// * export transformations (used when no keys)
/// 
/// TODO :
/// * export camera data (to XClip userdata and/or to tracks ?)
/// * object name remapping (popup if some nodes are not found in the scene ?)
/// * parameter name remapping (can be done internally if we know the naming
/// rules from other toolsets : the application name should be stored on export)
/// * auto params = stageObject->getParams(); usefull in case we want a raw
/// export of everything... we structured export for now : tx ty tz rw rx ry rz
/// scale sx sy (no sz)
/// 
/// </summary>

void ExportXClipCommand::execute() {
  auto scene        = TApp::instance()->getCurrentScene()->getScene();
  auto props        = scene->getProperties()->getOutputProperties();
  float fps         = props->getFrameRate();
  TFilePath fp      = scene->getScenePath().withType("xcl");
  auto xsheet       = TApp::instance()->getCurrentXsheet()->getXsheet();
  auto levelSet     = scene->getLevelSet();
  TStageObject *stageObject;
  static GenericSaveFilePopup *savePopup = 0;
  savePopup                              = new GenericSaveFilePopup(
      QObject::tr("Export Keyframe Animations (xcl)"));
  savePopup->addFilterType("xcl");
  fp = savePopup->getPath();
  if (fp.isEmpty()) return;
  XClip xclip;

  // export all columns (cell + camera)
  auto tNodes = getTNodes();
  for (auto tNode : tNodes) {
    auto node   = xclip.addNode(tNode.first);
    stageObject = tNode.second;
    tParamToXTrack(stageObject->getParam(TStageObject::Channel::T_X),
                       node->addTrack("tx"), fps);
    tParamToXTrack(stageObject->getParam(TStageObject::Channel::T_Y),
                       node->addTrack("ty"), fps);
    tParamToXTrack(stageObject->getParam(TStageObject::Channel::T_Z),
                       node->addTrack("tz"), fps);
    tParamToXTrack(stageObject->getParam(TStageObject::Channel::T_Scale),
                        node->addTrack("scale"), fps);
    tParamToXTrack(stageObject->getParam(TStageObject::Channel::T_ScaleX),
                        node->addTrack("sx"), fps);
    tParamToXTrack(stageObject->getParam(TStageObject::Channel::T_ScaleY),
                        node->addTrack("sy"), fps);
    tParamToXTrack(stageObject->getParam(TStageObject::Channel::T_Angle),
                        node->addTrack("angle"), fps);
#ifdef USE_QUAT
    // rotation should be converted to quaternion...
    // rollDegreeToQuaternion()
    std::vector<TDoubleKeyframe> angle;
    std::vector<TDoubleKeyframe> rw;
    std::vector<TDoubleKeyframe> rx;
    std::vector<TDoubleKeyframe> ry;
    std::vector<TDoubleKeyframe> rz;
    auto angleParam = stageObject->getParam(TStageObject::Channel::T_Angle);
    for (int i = 0; i < angleParam->getKeyframeCount(); i++) {
      auto kf = angleParam->getKeyframe(i);
      TDoubleKeyframe rwk, rxk, ryk, rzk;
      rwk = rxk = ryk = rzk = kf; // copy all members (time, tangents, etc...)
      auto q                = rollDegreeToQuaternion(kf.m_value);
      rwk.m_value           = q.w;
      rxk.m_value           = q.x;
      ryk.m_value           = q.y;
      rzk.m_value           = q.z;
      rw.push_back(rwk);
      rx.push_back(rxk);
      ry.push_back(ryk);
      rz.push_back(rzk);
    }
    tDataToXTrack(rw, node->addTrack("rw"), fps);
    tDataToXTrack(rx, node->addTrack("rx"), fps);
    tDataToXTrack(ry, node->addTrack("ry"), fps);
    tDataToXTrack(rz, node->addTrack("rz"), fps);
    // finally, store the default value, used when there is no keyframe
    auto q = rollDegreeToQuaternion(
        stageObject->getParam(TStageObject::Channel::T_Angle)->getDefaultValue());
    node->getTrack("rw")->setValue(q.w);
    node->getTrack("rx")->setValue(q.x);
    node->getTrack("ry")->setValue(q.y);
    node->getTrack("rz")->setValue(q.z);
#endif
  }

  xclip.updateTimeRange();
  if (!xclip.write(fp.getQString().toStdString())) {
    DVGui::warning("ExportXClipCommand: failed to write to " + fp.getQString());
    return;
  }
  auto tokens = QString::fromStdString(xclip.dump()).split('\n');
  DVGui::info("ExportXClipCommand xclip hierarchy:");
  for (auto token : tokens) DVGui::info(token);
}


class ImportXClipCommand final : public MenuItemHandler {
public:
  ImportXClipCommand() : MenuItemHandler(MI_ImportXClip) {}
  void execute() override;
} importXClipCommand;

/// <summary>
/// xclip import
/// DONE :
/// * import keyframes
/// * import transformations (used when no keys)
/// TODO :
/// * import camera data (from XClip userdata and/or from tracks ?)
/// * object name remapping
/// * parameter name remapping (can be done internally if we know the naming
/// rules from other toolsets : the application name should be stored on export)
/// </summary>
void ImportXClipCommand::execute() {
  auto scene   = TApp::instance()->getCurrentScene()->getScene();
  auto props   = scene->getProperties()->getOutputProperties();
  float fps    = props->getFrameRate();
  TFilePath fp = scene->getScenePath().withType("xcl");
  auto xsheet  = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObject *stageObject;
  static GenericLoadFilePopup *loadPopup = 0;
  loadPopup =
      new GenericLoadFilePopup(QObject::tr("Import Keyframe Animations (xcl)"));
  loadPopup->addFilterType("xcl");
  fp = loadPopup->getPath();
  if (fp.isEmpty()) return;

  XClip xclip;
  if (!xclip.read(fp.getQString().toStdString())) return;
  for (auto node : xclip.nodes) {
    TStageObjectId columnObjId;
    if (findColumnId(node->name, columnObjId)) {
      DVGui::info("ImportXClipCommand: node found in the scene: " +
                  QString::fromStdString(node->name));
      stageObject = xsheet->getStageObject(columnObjId);
    } else {
      DVGui::info("ImportXClipCommand: node not found in the scene: " +
                  QString::fromStdString(node->name));
      continue;
    }
    TStageObject::Channel channel;
    for (auto track : node->tracks) {
      if (track.first == "tx") channel = TStageObject::Channel::T_X;
      if (track.first == "ty") channel = TStageObject::Channel::T_Y;
      if (track.first == "tz") channel = TStageObject::Channel::T_Z;
      if (track.first == "scale") channel = TStageObject::Channel::T_Scale;
      if (track.first == "sx") channel = TStageObject::Channel::T_ScaleX;
      if (track.first == "sy") channel = TStageObject::Channel::T_ScaleY;
      if (track.first == "sz") continue;
      if (track.first == "angle")
        channel = TStageObject::Channel::T_Angle;  // in case a 2d angle was
                                                   // stored, use it
      // xTrackToTParam(track.second, stageObject->getParam(channel), fps);
      std::vector<TDoubleKeyframe> keys;
      // set value, used when there is not keyframe
      stageObject->getParam(channel)->setDefaultValue(track.second->value);
      xTrackToTData(track.second, keys, fps);
      addKeyframes(keys, stageObject->getParam(channel));
    }
#ifdef USE_QUAT
    // rotation : if no angle was found,
    // convert quaternion to roll :
    // the 4 quaternion channels must have the same number of keyframes !
    // (The exporters should take care of this)
    // XClip doesn't group the channels in typed variables : all channels are
    // storing one float value. In that case, we must combine four rotation
    // tracks in one
    if (node->tracks.find("angle") == node->tracks.end() &&
        node->tracks.find("rw") != node->tracks.end() &&
        node->tracks.find("rx") != node->tracks.end() &&
        node->tracks.find("ry") != node->tracks.end() &&
        node->tracks.find("rz") != node->tracks.end()) {
      channel = TStageObject::Channel::T_Angle;
      // set value, used when there is not keyframe
      stageObject->getParam(channel)->setDefaultValue(quaternionToRollDegree(
          node->tracks["rw"]->getValue(), node->tracks["rx"]->getValue(),
          node->tracks["ry"]->getValue(), node->tracks["rz"]->getValue()));
      std::vector<TDoubleKeyframe> angle;
      std::vector<TDoubleKeyframe> rw;
      std::vector<TDoubleKeyframe> rx;
      std::vector<TDoubleKeyframe> ry;
      std::vector<TDoubleKeyframe> rz;
      xTrackToTData(node->tracks["rw"], rw, fps);
      xTrackToTData(node->tracks["rx"], rx, fps);
      xTrackToTData(node->tracks["ry"], ry, fps);
      xTrackToTData(node->tracks["rz"], rz, fps);
      for (int i = 0; i < rw.size(); i++) {
        TDoubleKeyframe keyframe;
        keyframe.m_frame      = rw[i].m_frame;
        keyframe.m_type       = rw[i].m_type;
        keyframe.m_speedIn.x  = rw[i].m_speedIn.x;
        keyframe.m_speedIn.y  = rw[i].m_speedIn.y;
        keyframe.m_speedOut.x = rw[i].m_speedOut.x;
        keyframe.m_speedOut.y = rw[i].m_speedOut.y;
        // convert full quaternion rotation to euler roll only
        keyframe.m_value = quaternionToRollDegree(rw[i].m_value, rx[i].m_value,
                                                  ry[i].m_value, rz[i].m_value);
        angle.push_back(keyframe);
        addKeyframes(angle, stageObject->getParam(channel));
      }
    }
#endif
  }
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  auto tokens = QString::fromStdString(xclip.dump()).split('\n');
  DVGui::info("ImportXClipCommand xclip hierarchy:");
  for (auto token : tokens) DVGui::info(token);
}





