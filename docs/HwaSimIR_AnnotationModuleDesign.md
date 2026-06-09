# HwaSimIR 实时标注功能模块设计文档

## 1. 文档目的

本文用于记录 HwaSimIR 项目“实时标注”功能的设计思路、实施目标、推荐框架、阶段计划和后续交接上下文。后续新开的 Codex 对话应优先阅读本文，再根据“阶段成果记录”继续实施、测试和修正。

本轮只做标注模块设计与文档沉淀，不执行 `docs/HwaSimIR_InfraredSimulationFramework.md` 中的红外仿真渲染任务，不修改红外相关代码。

## 2. 当前任务背景

协议初始化参数位于：

- `ConsoleApplication1_LLA/ConsoleApplication1/Common/CommonData.h`

其中 `BYHWICD::trackerSensorParam` 已包含开关：

```cpp
bool realtimeAnnotation; //是否实时标注
```

当前运行时初始化流程会把 `cmd.trackingInit.trackerSensor[0]` 缓存到 `HwaSimIR::m_sensorParam`。因此标注功能应以 `m_sensorParam.realtimeAnnotation` 作为总开关：

- `false`：不创建或不刷新标注叠加层，不输出标注文件，保持原有渲染和视频输出路径。
- `true`：在渲染窗口叠加目标型号标签、关重部位标签和目标框，并为每个视频帧生成一组同步标注数据。

当前目标平台相关数据结构：

- `BYHWICD::TargetState`：目标类型、目标 ID、视场有效标志、目标空间状态等实时数据。
- `TargetPlatformData`：保存平台类型、目标 ID、`TargetState`、`NodePath` 和存在标志。
- `HwaSimIR::m_targetPlatformList`：当前 TargetState 目标平台列表。

当前目标类型到平台类型的已有映射在 `HwaSimIR::TargetTypeToPlatformType`：

```cpp
case 0x11: return F35;
case 0x22: return AIM120;
case 0x33: return AIM9;
case 0x44: return MMD;
```

标注输出文字不要强行复用内部枚举名，建议单独建立“协议类型到标注文字”的映射：

```cpp
0x11 -> F35   // 当前飞机类型暂按 F35；后续如需 F22，需要补充区分规则
0x22 -> AIM120D
0x33 -> AIM9X
0x44 -> MMD
```

## 3. 功能目标

实时标注模块需要完成以下目标：

1. 根据 `trackerSensorParam.realtimeAnnotation` 控制标注功能是否启用。
2. 在渲染窗口中为所有可见目标输出标注信息，标注随目标移动；目标是否进入视场以 `TargetState::viewValid` 为第一层判断。
3. 完成目标模型 3D 坐标到窗口 2D 像素坐标的转换。
4. 每个可见目标至少包含三类信息：
   - 目标型号标签：例如 `F35`、`F22`、`AIM120D`、`AIM9X`、`MMD`。
   - 关重部位标签：每个目标暂设两个点，一个“头部”，一个“中间部位”，输出窗口像素坐标 `(x, y)`，位置可调；若该部位到相机之间存在阻挡，相机实际看不见该部位，则该部位标签不显示也不输出。
   - 目标框：用目标模型 3D 包围盒投影到 2D 渲染界面的最大外接矩形框住目标。
5. 所有标注信息按视频帧保存，后续与视频帧图像一一同步。
6. 当前成像分辨率已由 Stage6 SensorGeometry 接入；标注宽高应优先使用 `m_stage6FinalWidth`、`m_stage6FinalHeight`，未 ready 时再 fallback 到 `m_sensorDisplayConfig`、`m_renderTex` 或窗口尺寸，不硬编码 `800x800`。
7. 当前阶段先记录视频帧保存同步方案，实际保存功能与后续视频帧保存模块配合实现。

## 4. 设计边界

本功能是“画面叠加与帧级标注数据生成”模块，不应改动红外辐亮度、红外材质、红外大气、红外着色器等逻辑。

推荐的改动边界：

- 可以新增独立标注模块文件。
- 可以在 `HwaSimIR` 中增加少量初始化、每帧刷新和清理调用。
- 可以读取 `m_targetPlatformList`、相机、镜头、窗口尺寸和实时帧数据。
- 不应重构 `TargetState` 驱动流程。
- 不应改变已有平台模型加载、红外着色器挂载和 TCP/JPEG 输出逻辑。
- 对不确定的功能需求，实施前先向用户确认，不自行扩展业务含义。
- 后续新增或修改 C++ 代码时，需要添加简明中文注释，特别是投影、遮挡检测、帧同步和开关控制等关键逻辑。

### 4.1 本次补充内容设计思路

本次补充把标注模块从“只做 3D 到 2D 投影”扩展为“渲染显示状态、关重点遮挡、输出同步”共同约束的模块。设计上分三层判断：

1. 目标层可见性：`TargetState::viewValid` 决定目标是否在视场内。若为 `false`，目标本身不应渲染显示，标注模块也不生成该目标的型号标签、目标框和关重部位标签。
2. 目标框层可见性：目标 `viewValid == true` 后，再用 3D 包围盒投影到 2D，判断窗口内是否有有效目标框；只有窗口中实际显示的目标才绘制目标框和型号标签。
3. 关重部位层可见性：每个关重点在目标整体可见的基础上单独判断，先投影到窗口，再检测相机到关重点之间是否有阻挡；被挡住的关重点不显示、不输出。

这样做可以避免两个常见问题：

- 目标协议状态已经不在视场，但模型节点仍残留在画面中，导致标注和显示状态不一致。
- 目标整体可见，但头部或中部被遮挡时仍输出错误关重部位坐标。

分辨率规则以 Stage6 最终显示/输出尺寸为准：阶段 1 不再硬编码 `800x800`，优先读取 `m_stage6FinalWidth`、`m_stage6FinalHeight`。如果后续发现窗口尺寸、渲染纹理尺寸、TCP flip 后图像尺寸和保存帧尺寸不一致，需要先确认哪一个是标注坐标的唯一基准。

## 5. 推荐模块框架

建议新建独立目录：

```text
ConsoleApplication1_LLA/ConsoleApplication1/Annotation/
```

推荐文件拆分：

```text
AnnotationTypes.h
AnnotationConfig.h
AnnotationProjector.h
AnnotationProjector.cpp
AnnotationOverlay.h
AnnotationOverlay.cpp
AnnotationManager.h
AnnotationManager.cpp
```

### 5.1 AnnotationTypes

定义纯数据结构，负责表达一帧标注结果，不绑定 Panda3D 渲染节点。

建议结构：

```cpp
struct AnnotationPoint2D {
    std::string name;     // head / middle
    int x = 0;
    int y = 0;
    bool visible = false; // 已投影到窗口内，且相机到该部位之间无遮挡
};

struct AnnotationRect2D {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool visible = false;
};

struct TargetAnnotation {
    int targetID = 0;
    int targetType = 0;
    std::string modelLabel;
    AnnotationRect2D bbox;
    std::vector<AnnotationPoint2D> keyPoints;
};

struct AnnotationFrameRecord {
    unsigned long long frameIndex = 0;
    double simTimeMs = 0.0;
    int sensorID = 0;
    int width = 0;
    int height = 0;
    std::vector<TargetAnnotation> targets;
};
```

正式保存标注时建议只输出 `visible == true` 的关重部位；若为了调试需要保留被遮挡点，可在 debug 输出中增加 `occluded` 或 `reason` 字段，但不要混入最终训练/交付格式，避免把相机看不见的部位误标出来。

### 5.2 AnnotationConfig

负责目标类型标签、关重部位局部坐标和显示样式配置。

建议先使用代码内置表，后续再升级到 JSON 配置：

```cpp
struct TargetAnnotationConfig {
    std::string label;
    LPoint3f headLocalOffset;
    LPoint3f middleLocalOffset;
};
```

初始配置建议：

- `F35`、`F22`、`AIM120D`、`AIM9X`、`MMD` 都各自配置一组 `headLocalOffset` 和 `middleLocalOffset`。
- 当前模型补充前，可以先按目标包围盒自动估算默认点：
  - 头部：局部包围盒前向端点附近。
  - 中间部位：局部包围盒中心。
- 实施阶段 2 前用户补充模型后，再根据模型朝向和比例修正 offsets。

注意：当前 `0x11` 只能确定为飞机类，代码默认映射到 `F35`。如果同一协议值未来可能表示 `F35` 或 `F22`，需要用户提供区分依据，例如新增字段、目标 ID 规则、模型配置表或初始化命令里的模型名。

### 5.3 AnnotationProjector

负责所有 3D 到 2D 计算，不直接绘制。

输入：

- `NodePath targetNode`
- `TargetState targetState`
- `PLATFORM_TYPE type`
- `NodePath cameraNode`
- `Lens* cameraLens`
- 窗口宽高
- 可选遮挡检测场景根节点或碰撞检测器

输出：

- 目标是否可见。
- 目标 2D 包围框。
- 关重部位 2D 像素坐标，以及该部位是否无遮挡可见。
- 型号标签建议显示坐标。

推荐投影流程：

1. 从目标 `NodePath` 取得世界坐标或相对 `m_renderRoot` 的坐标。
2. 将目标点转换到相机坐标系。
3. 调用 Panda3D 镜头投影接口得到标准化设备坐标。
4. 将标准化坐标转换为窗口像素坐标。

推荐伪代码：

```cpp
bool ProjectWorldPointToPixel(
    const NodePath& renderRoot,
    const NodePath& cameraNode,
    Lens* cameraLens,
    const LPoint3f& worldPoint,
    int width,
    int height,
    int& outX,
    int& outY)
{
    LPoint3f cameraPoint = cameraNode.get_relative_point(renderRoot, worldPoint);

    LPoint2f ndc;
    if (!cameraLens->project(cameraPoint, ndc)) {
        return false;
    }

    outX = static_cast<int>((ndc.get_x() + 1.0f) * 0.5f * width);
    outY = static_cast<int>((1.0f - ndc.get_y()) * 0.5f * height);

    return outX >= 0 && outX < width && outY >= 0 && outY < height;
}
```

实际实现时要用当前项目 Panda3D C++ SDK 的准确函数签名验证 `Lens::project` 参数类型。若 `Lens::project` 要求传入相机空间坐标，则必须先做 `cameraNode.get_relative_point(renderRoot, worldPoint)`。

### 5.3.1 关重部位遮挡检测

关重部位标签需要单独做“相机可见性”判断：即使目标整体在视场内，某个部位也可能被其他目标、地形、载机结构、云层近似物或目标自身几何遮挡。若从相机位置到该关重点之间存在更近的阻挡物，则该关重部位标签不显示，也不进入正式标注输出。

推荐检测流程：

1. 先判断目标 `targetState.viewValid == true`，并完成关重点 3D 到 2D 投影。
2. 若关重点投影不在窗口内，直接认为该关重点不可输出。
3. 从相机世界坐标向关重点世界坐标发射一条检测射线。
4. 获取射线方向上的最近碰撞点。
5. 计算最近碰撞点到相机的距离 `hitDistance`，以及关重点到相机的距离 `pointDistance`。
6. 若 `hitDistance < pointDistance - epsilon`，说明关重点前方存在阻挡，该关重点不可输出。
7. 若没有碰撞，或最近碰撞点距离与关重点距离在误差范围内，则认为该关重点无遮挡可见。

建议实现方式：

- Panda3D 原生方案：使用 `CollisionTraverser`、`CollisionRay`、`CollisionHandlerQueue` 做射线检测。
- 如果项目后续引入物理碰撞库，可用更高性能的 ray test，但阶段 1 不建议额外引入新依赖。
- 为避免每帧检测成本过高，阶段 1 只对 `viewValid == true` 且 2D 投影在窗口内的关重点做射线检测。
- 模型自身也可能造成遮挡，因此不要简单忽略整个目标模型；应使用 `epsilon` 容忍“命中关重点所在表面”的正常情况，同时把命中在关重点前方的几何视为遮挡。

阶段 1 可以先搭建接口和基础检测；如果现有模型缺少碰撞几何，可先用模型包围盒或简化碰撞体作为遮挡近似，并在阶段成果中记录精度限制。阶段 2 再根据用户补充模型，为每类模型补齐更准确的碰撞体或遮挡检测配置。

### 5.4 目标框计算

目标框来自模型 3D 包围盒投影后的最大 2D 外接矩形。

推荐流程：

1. 对每个目标 `targetPlat.nodePath` 调用包围盒计算接口，例如 `calc_tight_bounds`。
2. 得到包围盒 8 个角点。
3. 将 8 个角点转换到统一世界坐标或 `m_renderRoot` 坐标。
4. 分别投影到窗口像素。
5. 对所有有效投影点取：
   - `minX`
   - `minY`
   - `maxX`
   - `maxY`
6. 将矩形裁剪到 `[0, width) x [0, height)`。
7. 如果有效投影点过少、矩形面积过小、目标完全在相机后方或完全离屏，则认为该目标当前不可标注。

阶段 1 可以先处理“完全在视场内”的目标；阶段 2 再处理部分出屏、近裁剪面穿越和极小目标过滤。

### 5.5 AnnotationOverlay

负责在 Panda3D 窗口上绘制 2D 标注。

推荐使用独立 2D overlay 根节点，不污染 3D 模型节点：

- 用 `render2d` 或等价 2D 场景节点绘制。
- 用 `TextNode` 绘制文字标签。
- 用 `LineSegs` 或 2D 几何绘制目标框和引导线。

建议每帧刷新方式：

1. `AnnotationManager` 计算出当前 `AnnotationFrameRecord`。
2. `AnnotationOverlay` 清空上一帧 overlay 子节点或复用节点池。
3. 对每个目标绘制：
   - 目标框。
   - 型号标签，建议放在框左上角或上方。
   - 已通过遮挡检测的头部点和中间点坐标文字，例如 `head(315, 210)`、`mid(330, 245)`。
4. 文字和框要做边界保护，避免贴边时超出窗口。

显示样式建议：

- 目标框：绿色或青色细线。
- 型号标签：同色文字，带轻微黑色阴影或半透明底，提升可读性。
- 关重部位：小十字或小圆点加坐标文字；被遮挡的关重部位不画、不输出。
- 字体大小随窗口分辨率做有限档位调整，不要使用过大的文字遮挡目标。

### 5.6 AnnotationManager

负责模块总控。

主要职责：

- 持有 `AnnotationConfig`。
- 持有 `AnnotationProjector`。
- 持有 `AnnotationOverlay`。
- 根据 `m_sensorParam.realtimeAnnotation` 启停。
- 每帧生成 `AnnotationFrameRecord`。
- 向后续视频帧保存模块提供当前帧标注快照。

推荐接口：

```cpp
class AnnotationManager {
public:
    void initialize(NodePath renderRoot, NodePath overlayRoot);
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void clear();

    AnnotationFrameRecord updateFrame(
        unsigned long long frameIndex,
        double simTimeMs,
        int sensorID,
        int width,
        int height,
        const std::vector<TargetPlatformData>& targets,
        const NodePath& renderRoot,
        const NodePath& cameraNode,
        Lens* cameraLens);

    const AnnotationFrameRecord& latestRecord() const;
};
```

## 6. HwaSimIR 集成点建议

### 6.1 初始化命令

在 `handleInitCmd` 或 `ProcessRealSimSceneInitData` 完成 `m_sensorParam` 缓存后：

- 读取 `m_sensorParam.realtimeAnnotation`。
- 调用 `m_annotationManager.setEnabled(m_sensorParam.realtimeAnnotation)`。
- 若关闭，清空 overlay 和未保存的标注缓存。

### 6.2 每帧更新

当前已有任务：

- `Scene_UpdateTask`：有新 UDP 数据时更新场景目标位姿。
- `CaptureTask`：从 `m_renderTex` 取图像并推送 TCP。

标注更新必须发生在目标位姿更新之后、图像采集或保存之前。推荐新增：

```text
Scene_UpdateTask -> Annotation_UpdateTask -> CaptureTask / FrameSaveTask
```

如果 Panda3D 任务排序可控，应显式设置任务 sort 顺序；如果当前 SDK 使用默认顺序，需要在实施时验证任务执行顺序。另一种保守做法是在 `scene_update_task` 调完 `ProcessRealSimSceneDrivenData()` 后立即调用标注刷新，确保标注使用的是最新目标位姿。

### 6.3 复位与停止

在复位和目标平台删除时：

- 清空标注 overlay。
- 清空 `latestRecord`。
- 重置标注帧序号或在新的回合中重新编号。

停止仿真时：

- 可保留最后一帧 overlay 便于观察。
- 如果用户希望停止后清屏，可在控制命令 `simCommand == 3` 时清空，需要后续确认。

### 6.4 成像分辨率接入

当前阶段标注分辨率跟随 Stage6 最终显示/输出尺寸，不再硬编码 `800x800`。标注模块中的像素坐标、目标框和后续 JSONL 示例都应使用同一帧最终显示/输出图像的宽高。

后续需要根据 `CommonData.h` 的成像初始化参数接入真实分辨率：

```cpp
int trackerSensorWidth;   // 成像宽度
int trackerSensorHeight;  // 成像高度
```

推荐策略：

1. 阶段 1：优先使用 `m_stage6FinalWidth`、`m_stage6FinalHeight`；未 ready 时依次使用 `m_sensorDisplayConfig`、`m_renderTex`、窗口尺寸。
2. 阶段 2：继续校验 `m_sensorParam.trackerSensorWidth`、`m_sensorParam.trackerSensorHeight` 与最终显示/输出图像尺寸的一致性。
3. 如果窗口尺寸、渲染纹理尺寸和协议成像尺寸不一致，必须先确认最终以哪个尺寸作为标注坐标基准，再修改代码。

原则上，标注坐标必须与最终保存的视频帧图像使用同一个宽高和同一个坐标原点；在该规则不明确时，需要先向用户确认。

## 7. 帧同步与保存方案

本任务要求“视频帧图像、标注信息相互之间保持数据同步，每帧对应一组标注信息”。当前视频帧保存功能后续再做，但标注模块设计时必须预留帧同步接口。

推荐新增后续模块：

```text
FrameOutputCoordinator
```

它负责把同一帧的图像和标注记录绑定在一起：

```cpp
struct FrameOutputPacket {
    unsigned long long frameIndex;
    double simTimeMs;
    int width;
    int height;
    const unsigned char* rgbData;
    AnnotationFrameRecord annotations;
};
```

推荐同步原则：

1. 同一帧只生成一个 `frameIndex`。
2. 图像保存文件名和标注记录使用同一个 `frameIndex`。
3. 标注记录包含 `imageFile` 或可由 `frameIndex` 推导出图像文件名。
4. 如果未来保存 MP4，同时保存逐帧 JSONL 标注文件，MP4 帧序号与 JSONL 行内 `frameIndex` 保持一致。
5. 如果 `m_renderTex->get_ram_image()` 存在 GPU 到 CPU 延迟，需要用实测确认当前取到的是刚渲染帧还是上一帧；若有一帧延迟，标注记录也应进入同样的队列延迟一帧后再配对。

推荐输出目录：

```text
Output/
  Annotation/
    round_0001/
      frames/
        frame_000001.png
        frame_000002.png
      annotations.jsonl
```

推荐 JSONL 单帧格式：

```json
{
  "frameIndex": 1,
  "simTimeMs": 1234.0,
  "sensorID": 0,
  "imageFile": "frames/frame_000001.png",
  "width": 800,
  "height": 800,
  "targets": [
    {
      "targetID": 101,
      "targetType": "0x22",
      "modelLabel": "AIM120D",
      "bbox": {"x": 310, "y": 220, "width": 80, "height": 24},
      "keyPoints": [
        {"name": "head", "x": 376, "y": 228, "visible": true},
        {"name": "middle", "x": 344, "y": 232, "visible": true}
      ]
    }
  ]
}
```

后续如需要适配训练数据格式，可以从 JSONL 派生 YOLO、COCO 或自定义 txt；不要一开始只写死为某一种训练格式。

## 8. 可见性规则

“窗口中显示的所有目标都要标注”的前提是：目标不在视场内时，目标本身不应被渲染显示。也就是说，`TargetState::viewValid` 是目标显示和目标标注的第一层开关。

建议规则：

1. 目标 `TargetPlatformData::isExist == true`。
2. 目标 `NodePath` 非空。
3. 目标 `targetState.viewValid == true`。
4. 目标节点处于显示状态；如果实时数据中 `viewValid == false`，目标应被隐藏或跳过渲染。
5. 目标 3D 包围盒至少有有效投影点。
6. 目标框与窗口有交集。
7. 目标框面积超过最小阈值，例如 `4 x 4` 像素，避免远距离抖动噪点。

推荐集成方式：

- 在目标位姿更新流程中，若 `targetState.viewValid == false`，对目标 `NodePath` 执行隐藏或让渲染流程跳过该目标。
- `AnnotationManager` 只处理 `viewValid == true` 且当前显示的目标。
- 如果 `viewValid == true` 但投影不可见，说明目标可能在相机裁剪外、被极端姿态影响或尺寸过小，可跳过标注并记录低频诊断日志。
- 如果 `viewValid == false` 但节点仍显示在窗口中，说明渲染显示逻辑与协议状态不一致，应优先修正显示逻辑，而不是让标注模块单独补救。

关重部位还有第二层可见性：目标整体可见不代表每个关重点可见。头部、中间部位需要分别通过投影和遮挡检测；被遮挡的单个关重点不显示、不输出，但目标框和型号标签仍可显示。

## 9. 关重部位坐标设计

每个目标暂设两个关重部位：

- `head`：头部。
- `middle`：中间部位。

推荐配置方式：

```cpp
std::map<PLATFORM_TYPE, TargetAnnotationConfig> configs;
configs[F35] = {"F35", headOffset, middleOffset};
configs[AIM120] = {"AIM120D", headOffset, middleOffset};
configs[AIM9] = {"AIM9X", headOffset, middleOffset};
configs[MMD] = {"MMD", headOffset, middleOffset};
```

坐标来源优先级：

1. 用户提供的模型局部坐标 offsets。
2. 根据模型包围盒估算默认 offsets。
3. 后续通过配置文件调参。

建议把 offsets 放在目标模型局部坐标系中，这样目标移动、旋转、缩放后仍能跟随模型正确投影。

每个关重部位的输出条件：

1. 所属目标 `viewValid == true` 且正在渲染显示。
2. 关重点 3D 坐标成功投影到窗口内。
3. 相机到关重点之间无遮挡。
4. 满足以上条件才在窗口显示该关重部位标签，并写入正式帧标注数据。

如果只有一个关重部位可见，则只输出可见的那个；如果两个关重部位都被遮挡，目标仍可保留型号标签和目标框，但 `keyPoints` 为空或只在 debug 数据中记录被遮挡原因。正式输出采用哪一种格式需要在阶段 3 前确认。

如果模型局部坐标系朝向不统一，需要为每类模型单独配置“头部轴向”。例如：

```cpp
enum class ModelForwardAxis {
    PositiveX,
    NegativeX,
    PositiveY,
    NegativeY,
    PositiveZ,
    NegativeZ
};
```

阶段 1 可先默认以包围盒某一主轴端点为头部；阶段 2 根据用户补充模型后校准。

## 10. 型号标签规则

目标型号标签来自 `TargetState::targetType`，但显示文本需要按业务含义输出：

| targetType | 当前平台枚举 | 推荐显示标签 | 备注 |
| --- | --- | --- | --- |
| `0x11` | `F35` | `F35` | 当前飞机类型暂默认 F35；如要显示 F22，需补充区分规则 |
| `0x22` | `AIM120` | `AIM120D` | 标注文本使用 AIM120D |
| `0x33` | `AIM9` | `AIM9X` | 标注文本使用 AIM9X |
| `0x44` | `MMD` | `MMD` | 直接显示 MMD |

不要为了显示 `AIM120D`、`AIM9X` 直接重命名现有 `PLATFORM_TYPE` 枚举，避免影响模型加载和已有逻辑。推荐新增 `TargetTypeToAnnotationLabel` 或 `PlatformTypeToAnnotationLabel`。

## 11. 推荐实施阶段

### 阶段 0：文档与设计冻结

目标：

- 建立本文档。
- 明确不开启红外仿真渲染相关任务。
- 明确标注模块接口、集成点和后续用户需补充信息。

验收：

- `docs/HwaSimIR_AnnotationModuleDesign.md` 存在。
- 文档包含任务目标、框架、实施建议、帧同步方案和阶段记录区。

### 阶段 1：实时窗口标注最小可用版

目标：

- 新增 `Annotation` 模块骨架。
- 根据 `m_sensorParam.realtimeAnnotation` 启停。
- 按 `TargetState::viewValid` 控制目标显示和标注，只为窗口中实际显示的目标绘制：
  - 型号标签。
  - 目标框。
  - 通过遮挡检测的关重部位点与像素坐标。
- 先不保存图像和标注文件，只保证窗口显示正确。
- 分辨率跟随 Stage6 最终显示/输出尺寸，不硬编码 `800x800`。

建议改动：

- 新增 `AnnotationTypes/Config/Projector/Overlay/Manager`。
- `HwaSimIR.h` 增加 `AnnotationManager` 成员。
- `HwaSimIR.cpp` 在初始化、每帧更新、复位时调用标注模块。
- 更新 VS 工程或 CMake，使新增文件参与编译。
- 新增或修改的关键代码添加中文注释，说明投影、遮挡检测、开关和帧同步逻辑。

阶段 1 验收：

- `realtimeAnnotation=false` 时窗口无标注，性能和原行为不明显变化。
- `realtimeAnnotation=true` 时可见目标有框、有类型；无遮挡的关重部位显示像素坐标。
- 目标移动时标注跟随移动。
- `targetState.viewValid=false` 时目标不渲染、不显示标注。
- 某个关重部位被遮挡时，只隐藏并不输出该部位标签；目标其他可见标注仍正常显示。

### 阶段 2：坐标精度和模型适配

目标：

- 根据用户补充的 `F35`、`F22`、`AIM120D`、`AIM9X`、`MMD` 模型，校准包围盒和关重部位 offsets。
- 处理部分出屏、近裁剪面、窗口缩放和不同分辨率。
- 继续校验标注坐标、Stage6 输出尺寸、TCP/JPEG 最终显示尺寸之间的翻转和同步关系。
- 校准关重部位遮挡检测，包括目标自身遮挡和目标之间互相遮挡。
- 如果 `0x11` 同时可能表示 F35/F22，补充类型区分机制。

阶段 2 验收：

- 各模型头部和中部坐标位置合理。
- 目标框紧贴目标主体，不出现明显偏移。
- 改变窗口分辨率后坐标仍正确。
- 多目标同时出现时标注互不串号。

### 阶段 3：视频帧和标注文件同步保存

目标：

- 实现或接入视频帧保存模块。
- 每帧保存图像时同步保存 `AnnotationFrameRecord`。
- 输出 JSONL 或用户指定格式。

阶段 3 验收：

- 每张帧图像都有一条对应标注记录。
- `frameIndex`、`simTimeMs`、`sensorID`、图像尺寸一致。
- 图像中目标位置与标注框、关重部位坐标匹配。
- 如果保存 MP4，仍保留逐帧标注文件用于数据对齐。

### 阶段 4：测试、性能和交付

目标：

- 增加投影计算的单元测试或调试验证工具。
- 增加低频诊断日志。
- 控制 overlay 节点创建销毁成本，必要时使用节点池。

阶段 4 验收：

- 5 个以内目标实时标注无明显卡顿。
- 复位、开始、停止、多回合运行不会残留旧标注。
- 输出文件可被脚本校验。

## 12. 测试建议

### 12.1 手动测试

1. 启动 HwaSimIR。
2. 初始化时设置 `realtimeAnnotation=false`，确认窗口无标注。
3. 初始化时设置 `realtimeAnnotation=true`，确认窗口出现标注。
4. 使用实时数据驱动目标移动，确认框和文字跟随目标。
5. 将目标实时数据设为 `viewValid=false`，确认目标不渲染且标注消失。
6. 同时显示多个目标，确认每个目标都有独立标注。
7. 制造一个目标或场景物体遮挡关重部位，确认该部位标签不显示也不进入输出记录。
8. 复位后确认 overlay 清空。

### 12.2 坐标验证

建议在调试模式下低频打印：

```text
[Annotation] frame=123 targetID=7 type=AIM120D bbox=(310,220,80,24) head=(376,228) middle=(344,232)
```

打印频率不要每帧全量输出，建议首帧、每 100 帧或 debug 开关控制。

### 12.3 自动检查

后续可补充一个轻量检查脚本：

- 读取 `annotations.jsonl`。
- 检查每行 JSON 可解析。
- 检查 `bbox` 不越界。
- 检查 `keyPoints` 不越界。
- 检查被遮挡或不在窗口内的关重部位没有进入正式输出。
- 检查 `frameIndex` 连续。
- 检查每帧 `targets` 数量不超过协议上限或实际平台数量。

## 13. 需要用户后续提供的信息

为了把阶段 2 和阶段 3 做准确，需要用户补充：

1. `F22` 如何与 `F35` 区分：是否仍使用 `targetType=0x11`，还是会有新字段、新 targetType 或目标 ID 规则。
2. 各模型文件和纹理文件路径，尤其是 `F22`、`AIM120D`、`AIM9X`、`MMD` 的最终模型。
3. 每类模型局部坐标系说明：头部朝哪个轴，模型原点在哪里，单位和缩放是否统一。
4. 两个关重部位的期望位置：头部、中间部位是否要贴近具体部件，是否需要更多部位。
5. 遮挡检测精度要求：是否需要使用精确模型三角面，还是阶段 1 可接受包围盒或简化碰撞体近似。
6. 最终保存阶段是否需要把内存标注坐标转换到 TCP `cv::flip(..., 0)` 后的图像坐标。
7. 标注输出格式偏好：JSONL、COCO、YOLO、自定义 txt，或多格式同时输出。
8. 图像保存格式偏好：PNG、JPG、BMP，是否保存带标注图和无标注原图。
9. 坐标原点约定：当前建议图像左上角为 `(0,0)`，向右为 x 正方向，向下为 y 正方向。
10. 标注样式要求：颜色、字体大小、是否显示中文、是否需要置信度或目标 ID。

## 14. 后续 Codex 执行提示

新对话继续本任务时，请按以下顺序做：

1. 先读本文档。
2. 确认用户是否要求进入阶段 1、阶段 2 或阶段 3。
3. 如果用户没有明确要求，不要执行红外仿真渲染框架文档里的任务。
4. 只在用户要求实施时修改代码。
5. 修改代码前先检查工作树，避免覆盖用户或其他对话的改动。
6. 优先把标注功能做成独立模块，再在 `HwaSimIR` 中做最小集成。
7. 遇到不确定需求先问用户，例如 F35/F22 区分、遮挡精度、输出格式、最终分辨率和停止后是否清屏。
8. 修改或添加代码时添加中文注释，注释重点放在非显然逻辑，不要写空泛注释。
9. 每完成一个阶段，把实际改动、测试结果、遗留问题追加到本文“阶段成果记录”。

## 15. 阶段成果记录

### 2026-05-26 阶段 0：设计文档创建

已完成：

- 梳理 `trackerSensorParam.realtimeAnnotation` 作为实时标注总开关。
- 梳理 `TargetState`、`TargetPlatformData`、`m_targetPlatformList` 作为标注数据来源。
- 明确标注内容：型号标签、两个关重部位像素坐标、目标 2D 外接框。
- 明确推荐模块：`AnnotationTypes`、`AnnotationConfig`、`AnnotationProjector`、`AnnotationOverlay`、`AnnotationManager`。
- 明确后续帧同步保存方案：同一 `frameIndex` 绑定图像和 `AnnotationFrameRecord`。

未执行：

- 未修改任何红外相关代码。
- 未实现标注 C++ 模块。
- 未执行编译或运行测试。

下一步建议：

- 用户补充模型和 F35/F22 区分规则后，进入阶段 1 或阶段 2。
- 如果先做最小可用版，可先用包围盒默认估算头部和中部，不等待精确 offsets。

### 2026-05-26 阶段 0 补充：可见性、遮挡和分辨率规则

已补充：

- 目标是否显示与标注以 `TargetState::viewValid` 为第一层规则；`viewValid=false` 时目标不应渲染显示，也不输出标注。
- “窗口中显示的目标都要标注”解释为：已经进入视场并实际显示的目标需要标注。
- 关重部位标签增加遮挡判断；相机到关重点之间有阻挡时，该关重点不显示、不输出。
- 目标整体可见时，目标框和型号标签仍可显示；关重部位按点分别判断可见性。
- 成像分辨率阶段 1 后续应跟随 Stage6 输出尺寸；2026-06-08 阶段 1 已按该规则更新，不再硬编码 `800x800`。
- 记录执行约束：不确定功能需求先问用户；后续代码修改需要添加中文注释。

后续工作影响：

- 阶段 1 实现时，需同步处理目标节点显示/隐藏和标注跳过逻辑。
- 阶段 1 至少预留关重部位遮挡检测接口；若碰撞几何不足，可先记录近似方案和限制。
- 阶段 2 需要校准模型关重点 offsets、遮挡检测精度，以及 TCP flip/帧保存后的坐标同步关系。

### 2026-06-08 阶段 1：实时窗口标注最小可用版

已完成：

- 新增 `ConsoleApplication1_LLA/ConsoleApplication1/Annotation/` 标注模块，包含 `AnnotationTypes`、`AnnotationConfig`、`AnnotationProjector`、`AnnotationOverlay`、`AnnotationManager`。
- `AnnotationManager` 按 `m_sensorParam.realtimeAnnotation` 启停；关闭时清空 overlay 和 `latestRecord`，开启时只生成窗口 overlay 和内存快照。
- 标注模块只读 `m_targetPlatformList`，过滤 `isExist`、`nodePath`、`targetID`、`targetState.viewValid`、`nodePath.is_hidden()` 和包围盒投影结果，不修改目标 show/hide 主逻辑。
- 新增型号标签映射：`0x11 -> F35`、`0x22 -> AIM120D`、`0x33 -> AIM9X`、`0x44 -> MMD`，未修改 `PLATFORM_TYPE` 枚举。
- 用目标 local tight bounds 的 8 个角点投影生成 2D 外接框，并裁剪到最终显示/输出图像范围；小于 `4x4` 像素跳过。
- Stage 1 先用包围盒估算 `head` 和 `middle` 关重部位，并预留 `forwardAxis` 与 explicit local offset 配置。
- 预留 `isKeyPointVisibleByRay` 遮挡检测接口；当前没有统一 collision solids 时降级为仅投影可见，并低频输出 `[Annotation][WARN] occlusion geometry unavailable`。
- overlay 使用 Panda3D `aspect2d`、`LineSegs`、`TextNode` 绘制目标框、型号标签、关重部位十字和 `head(x,y)`、`middle(x,y)` 像素坐标。
- 标注坐标按最终显示/输出图像左上角坐标系记录，`x` 向右、`y` 向下；Stage 1 不处理 TCP `cv::flip(..., 0)` 后的保存坐标转换。
- 标注宽高优先使用 `m_stage6FinalWidth`、`m_stage6FinalHeight`，未 ready 时 fallback 到 `m_sensorDisplayConfig`、`m_renderTex` 或窗口尺寸，不硬编码 `800x800`。
- `HwaSimIR` 集成点：初始化 `AnnotationManager`，初始化命令后按 `realtimeAnnotation` 设置开关，`ProcessRealSimSceneDrivenData()` 末尾刷新标注，复位/删除目标/析构时清空 overlay 和快照。
- 已更新 VS 工程文件和 CMakeLists，使新增模块参与构建。

验证结果：

- 已运行 `MSBuild.exe ConsoleApplication1_LLA/ConsoleApplication1.sln /p:Configuration=Release /p:Platform=x64 /m`。
- 构建成功，输出 `ConsoleApplication1_LLA/Bin/ConsoleApplication1.exe`。
- 构建仍有 18 个既有/依赖相关 warning，主要来自 Panda3D STL DLL 接口、`math_algorithm.h` 未引用局部变量、`size_t` 到 `int` 转换；无新增编译错误。

降级项和遗留问题：

- Stage 1 未实现精确遮挡检测；缺少统一 collision solids 时只按投影可见输出关重部位，并明确低频告警。
- 未做图片、视频、JSONL 保存，未修改 UDP/TCP 协议，未输出标注网络包。
- 未做运行态人工冒烟测试；需要后续配合 DataDrivenTestQT 验证 `realtimeAnnotation=true/false`、目标移动、`targetNumValid` 减少、`viewValid=false` 和多目标不串号。
- 后续阶段需要校准各模型头部轴向、local offsets，以及 TCP flip/帧保存时的坐标同步规则。

### 2026-06-08 阶段 1.2：目标框和关重点校准 + 配置文件化

已完成：

- 新增 `ConsoleApplication1_LLA/Bin/Config/Annotation/annotation_profiles.json`，采用独立 Annotation profile，不直接依赖 `Config/IRHotspots/target_hotspots.json`，避免把红外亮斑语义和标注关键点语义耦合。
- 在 `ConsoleApplication1_LLA/Bin/Config/HwaSimIRRuntime.ini` 增加 `[Annotation]` 段，支持 `ProfilePath`、`DebugOverlay`、`BBoxMode`、`BBoxMarginPx`、`MinBBoxSizePx`、`DrawKeyPoints`、`DrawModelLabel`、`DrawBBox`。
- `AnnotationConfig` 新增轻量 JSON 读取逻辑，解析 defaults、platforms、bbox、keypoints、localPos、excludeNodeNameContains；配置文件缺失时使用代码 fallback。
- `AnnotationManager` 增加 profile 加载和 runtime option 应用接口，保持 `latestRecord` 仍为内存快照，不输出网络包、不落盘。
- `AnnotationOverlay` 改为 `DebugOverlay=true` 时才显示 `ANNO_TEST`；默认 `DebugOverlay=false` 时不显示自检文字。
- `AnnotationProjector` 的 bbox 主路径改为 `mesh_body`：递归遍历目标节点下 GeomNode，跳过 hidden 节点和名称包含 `EnginePlume`、`Plume`、`Annotation`、`Hotspot` 的节点/子树，逐顶点投影后求 2D bbox。
- bbox 失败回退顺序为：`mesh_body` 顶点投影 -> body-only tight bounds -> legacy whole-node tight bounds，并低频输出 `[AnnotationBBox][WARN] fallback=...`。
- bbox 结果按最终输出图像范围裁剪，应用 `BBoxMarginPx`，小于 `MinBBoxSizePx` 的目标框跳过。
- head/middle 优先使用 profile 中的 explicit `localPos`，当前初始值为 F35 head `[0.0, 2.8, 0.0]`、AIM120D head `[0.0, 1.1, 0.0]`、AIM9X head `[0.0, 0.9, 0.0]`、middle `[0.0, 0.0, 0.0]`；平台缺少配置时回退到内置默认值和 forwardAxis 估算。
- 新增低频日志：`[AnnotationConfig] profilePath=... loaded=... source=...`、`[AnnotationBBox] ... mode=mesh_body vertexCount=... bbox=...`、`[AnnotationKeyPoint] ... headLocal=... middleLocal=... headPixel=... middlePixel=...`。

验证结果：

- 运行 `MSBuild.exe ConsoleApplication1_LLA/ConsoleApplication1.sln /p:Configuration=Release /p:Platform=x64 /m`。
- 第一次编译时新增文件缺少 UTF-8 BOM，VS936 代码页把中文注释后的代码吞入注释，已通过 UTF-8 BOM 转换修复。
- 第二次编译 C++ 通过但链接失败，原因是正在运行的 `ConsoleApplication1.exe` 占用输出文件。
- 进程退出后重新构建成功，Release x64 输出 `ConsoleApplication1_LLA/Bin/ConsoleApplication1.exe`，最终结果 `7 warning / 0 error`；warning 来自 Panda3D STL DLL 接口和 `size_t` 到 `int` 转换。

未变更和边界：

- 未修改 `CommonData.h`、UDP 解析流程、TCP/JPEG 传输格式。
- 未修改红外辐射、材质、大气、AGC、MTF、Stage3/4/5/6/7 物理链路文件和物理逻辑。
- 未新增 JSONL、图片、视频保存功能，未输出标注网络包。
- 未改 Stage1.1 final overlay display region，只复用现有 overlay 显示路径。

遗留问题：

- 需要用实际运行数据手动校验单目标 AIM120D/AIM9X 的 bbox 是否贴近本体且不包含尾焰。
- 需要多目标场景验证框不再被其它目标或尾焰明显拉大，允许真实投影视角下的框重叠。
- 关重点 localPos 仍是初始值，后续需根据用户补充的最终模型坐标系继续微调。
- 遮挡检测仍保留 Stage1 接口，缺少统一 collision solids 时继续降级为 projection-only。
