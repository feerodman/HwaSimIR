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

### 2026-06-09 阶段 1.3：收口验收结论

收口结论：

- Stage1 窗口实时标注主体功能已完成，可作为后续 Stage2 的稳定基线。
- 已支持 `trackerSensorParam.realtimeAnnotation` 控制窗口 overlay 与 `latestRecord` 内存快照；关闭时清空 overlay 和 `latestRecord`，不影响原渲染和 TCP/JPEG 输出。
- 已支持目标型号标签、目标 2D 框、`head(x,y)` 和 `middle(x,y)` 关重点像素标签。
- bbox 使用 `mesh_body` 主路径，递归读取目标本体 Geom 顶点投影，排除 `EnginePlume`、`Plume`、`Annotation`、`Hotspot` 等节点/子树，避免尾焰/热点把目标框拉大。
- keypoint 使用 `Config/Annotation/annotation_profiles.json` 中的 `localPos`，当前 F35/AIM120D/AIM9X/MMD 已有初始配置；后续模型坐标系确认后可继续微调。
- `DebugOverlay=false` 为默认状态，不显示 `ANNO_TEST`；只有 `realtimeAnnotation=true` 且 `DebugOverlay=true` 时才显示左上角自检文字。
- 遮挡检测未完成：`AnnotationProjector::isKeyPointVisibleByRay(...)` 当前仅保留 Stage2 接口，未配置统一 collision solids 时降级为 `projection_only`，实际返回可见；不要把当前关重点可见性解释为真实遮挡结果。
- 帧图像保存、JSONL 标注保存、TCP flip 后坐标同步均未完成，留到后续阶段处理。
- 后续需要实现 `FrameOutputCoordinator` 或类似帧输出协调模块，统一管理本地图像帧保存、视频保存、`AnnotationFrameRecord` 逐帧保存、实时 `DisplayC2cObjTrackingData` 逐帧保存，以及对外传输同步。
- 保存/传输阶段必须保证每一帧图像对应一组标注信息和一组实时数据，`frameIndex`、`simTimeMs`、`sensorID`、`width`、`height` 必须作为同步主键或校验字段，避免图像、标注和实时状态错帧。
- TCP 对外传输已有 demo，但尚未集成到当前项目主链路；后续集成时需要同时处理 TCP `cv::flip(..., 0)` 后的图像坐标与标注坐标一致性。
- 保存和传输集成暂缓，等 Stage2A 真实遮挡检测完成后再实施，避免在关键点可见性规则未定时提前固化输出格式。

Stage1.3 验收清单：

1. `realtimeAnnotation=false`：窗口无标注，overlay 和 `latestRecord` 被清空。
2. `realtimeAnnotation=true` 且 `DebugOverlay=false`：可见目标显示目标框、型号、`head/middle`，不显示 `ANNO_TEST`。
3. `DebugOverlay=true`：左上角显示 `ANNO_TEST`，用于确认 final overlay 在 Stage6 最终图像上方。
4. `targetNumValid=1`：单目标 bbox 贴合目标本体，不包含尾焰/羽流。
5. `targetNumValid=3`：多目标 bbox 可按真实投影重叠，但型号标签不串号。
6. 修改 `trackerSensorWidth/Height` 后：标注坐标和 overlay 跟随 Stage6 最终输出尺寸，不硬编码 `800x800`。
7. 遮挡场景：当前不保证关重点遮挡正确，日志应说明 `fallback=projection_only`、`collisionSolids=unconfigured`、`stage2=required`。

工程和配置检查：

- `ConsoleApplication1_LLA/ConsoleApplication1/Annotation/` 已纳入 VS 工程和 `CMakeLists.txt` 编译。
- `ConsoleApplication1_LLA/Bin/Config/Annotation/annotation_profiles.json` 已存在，并由 `[Annotation].ProfilePath=Config/Annotation/annotation_profiles.json` 加载。
- `ConsoleApplication1_LLA/Bin/Config/HwaSimIRRuntime.ini` 已包含 `[Annotation]` 段和 `DebugOverlay/BBoxMode/BBoxMarginPx/MinBBoxSizePx/DrawKeyPoints/DrawModelLabel/DrawBBox` 配置。

Stage1.3 边界：

- 未修改 `CommonData.h`、UDP 解析流程、TCP/JPEG 传输格式。
- 未修改红外辐射、材质、大气、AGC、MTF、Stage3/4/5/6/7 物理链路。
- 未新增图片、视频、JSONL 保存功能。
- 未重构 Stage1.1 final overlay display region。
- 未实现真实遮挡检测。

下一阶段建议：

- Stage2A：实现真实遮挡检测，可评估 Panda3D CollisionRay、模型 collision solids 或深度图采样方案。
- Stage2B：实现 `FrameOutputCoordinator`，完成帧图像、视频、JSONL 标注、实时 `DisplayC2cObjTrackingData` 的逐帧同步保存，并统一 TCP `cv::flip(..., 0)` 后的坐标转换规则。

### 2026-06-09 阶段 2A：keypoint 遮挡检测基线（已被 Stage2A.2 替换）

本轮目标：

- 替换 Stage1.3 中 `AnnotationProjector::isKeyPointVisibleByRay(...)` 的 `projection_only` 固定返回路径，为 `head/middle` 关重点增加相机到关键点的遮挡判断。
- 保持 Stage1 已稳定的 bbox、型号标签、final overlay display region、`latestRecord` 内存快照结构不变。
- 不修改 `CommonData.h`、UDP/TCP 协议，不新增图片/视频/JSONL 保存，不改红外辐射、材质、大气、AGC、MTF、Stage3/4/5/6/7 物理链路。

调研结论：

- 当前工程搜索未发现可直接复用的 Panda3D `CollisionNode`、`CollisionSolid`、`CollisionRay` 碰撞体配置；目标库以 OBJ/EGG/BAM 模型资源为主。
- 因此 Stage2A 不假装已有精确 collision solids，而是选择可验证、可降级的 body-only bounds 近似方案：使用 Stage1.2 已验证的本体节点过滤规则，排除 `EnginePlume`、`Plume`、`Annotation`、`Hotspot` 等节点/子树后，对其它可见目标生成简化本体包围盒作为遮挡几何。

实现方案：

- `[Annotation]` 新增运行配置：
  - `OcclusionEnable=true`
  - `OcclusionMode=collision_ray`
  - `OcclusionEpsilonM=0.25`
- `AnnotationConfig` 新增 `AnnotationOcclusionConfig`，由 `HwaSimIRRuntime.ini` 注入，不经过 UDP 初始化协议结构体。
- `AnnotationManager::updateFrame(...)` 将当前 `m_targetPlatformList` 全量只读传入 `AnnotationProjector`，用于多目标互相遮挡判断。
- `AnnotationProjector` 对每个已投影到视口内的 keypoint 执行相机到 keypoint 的线段测试：
  - 跳过 keypoint 所属目标，避免所属目标本体表面造成自遮挡误判。
  - 仅检查其它 `isExist=true`、`viewValid=true`、`nodePath` 非空且未隐藏的目标。
  - 对候选遮挡目标构建 body-only tight bounds，射线段转换到候选目标局部坐标后与 AABB 相交。
  - 若命中距离满足 `epsilon < hitDistance < keypointDistance - epsilon`，认为该 keypoint 被遮挡，不绘制、不进入 `latestRecord`。
  - 若存在候选遮挡目标但无法取得可用 body-only bounds，则降级 `projection_only`，继续显示 keypoint，并输出低频 WARN。

日志规则：

- 首次启用 `collision_ray` 时输出：
  - `[AnnotationOcclusion][WARN] pandaCollisionSolids=0 geometry=body_bounds_approx mode=collision_ray`
- 低频输出遮挡判断：
  - `[AnnotationOcclusion] targetID=... point=head visible=... mode=collision_ray geometry=body_bounds_approx hit=... hitTargetID=... distance=... keyDistance=...`
- 遮挡几何不可用时输出：
  - `[Annotation][WARN] occlusion geometry unavailable targetID=... point=... fallback=projection_only mode=collision_ray checks=...`

当前能力边界：

- 已支持其它可见目标对 `head/middle` 的近似遮挡判断。
- bbox 和型号标签不受 keypoint 遮挡影响，仍按 Stage1.2 规则显示。
- 当前基线使用 body-only AABB 近似，不是逐三角形、深度图或像素级遮挡；近距离、斜视、复杂凹形模型可能存在保守误判。
- 当前跳过所属目标，因此尚不解决目标自身机体对自身 head/middle 的精细自遮挡；如需自遮挡，后续应接真实 collision solids 或深度图采样。
- 保存、传输、TCP `cv::flip(..., 0)` 后坐标同步仍未实现，继续留到 Stage2B 或后续阶段。

Stage2A 验收清单：

1. `OcclusionEnable=false` 或 `OcclusionMode=projection_only`：关重点按 Stage1 投影规则显示，不做遮挡隐藏。
2. 无其它可见遮挡目标：`head/middle` 正常显示。
3. 其它目标 body-only bounds 位于相机到 keypoint 之间：对应 keypoint 隐藏，并从 `latestRecord` 的 `keyPoints` 中省略。
4. 候选遮挡目标没有可用 body-only bounds：输出 `fallback=projection_only` WARN，不误报遮挡。
5. bbox、型号标签、多目标三元组匹配、Stage6 final overlay 显示不受本轮修改影响。

### 2026-06-09 阶段 2A.2：真实 mesh collision 遮挡检测与 surface keypoint

本轮结论：

- Stage2A 的 body-only AABB 遮挡基线已被 Stage2A.2 替换；遮挡判断不再使用 `body_bounds_approx` 或 AABB 兜底。
- 当前遮挡主路径为 `OcclusionMode=mesh_collision`，从目标真实 body-only mesh 三角面生成 Panda3D `CollisionNode/CollisionPolygon`，使用 `CollisionTraverser + CollisionRay + CollisionHandlerQueue` 做 keypoint 级遮挡判断。
- bbox、型号标签、final overlay、`latestRecord` 主结构保持 Stage1/Stage2A 既有逻辑，不因 keypoint 遮挡而隐藏目标框或型号。

配置更新：

- `[Annotation]` 当前推荐配置：
  - `OcclusionEnable=true`
  - `OcclusionMode=mesh_collision`
  - `OcclusionEpsilonM=0.25`
  - `OcclusionCollisionMaskBit=20`
  - `SurfaceKeyPointEnable=true`
  - `SurfaceSnapEnable=true`
- `annotation_profiles.json` 中 `head/middle` 增加表面点配置：
  - `surface=true`
  - `snapToMeshSurface=true`
  - `surfaceSearchMode=nearest_mesh_vertex`
- `localPos` 现在作为表面点 seed 使用；开启 snap 后，最终参与投影和遮挡判断的是吸附到 body-only mesh 顶点后的 `surfaceLocal`，不再把模型内部中心点当作真实关重点。

mesh collision 构建规则：

- 遍历 `targetPlat.nodePath` 下的 `GeomNode`，跳过 hidden 节点。
- 继续沿用 Stage1.2 body-only 过滤，跳过名称包含 `EnginePlume`、`Plume`、`Annotation`、`Hotspot` 的节点/子树。
- 对每个 `Geom` 调用 `decompose()` 统一三角化，再读取 `GeomPrimitive` 三角索引和 `GeomVertexData` 顶点。
- 每个有效三角面创建一个 `CollisionPolygon`，挂到目标节点下的 `AnnotationCollision_Target`。
- collision 节点使用专用 mask bit，当前默认 `BitMask32::bit(20)`。
- 每个目标 collision mesh 按 `targetType + targetPlatID + targetID` key 构建并缓存；同一目标不会每帧重建。

surface keypoint 规则：

- `head/middle` 必须先通过 body-only mesh surface snap 得到外表面点。
- 当前实现支持 `nearest_mesh_vertex`：在目标局部坐标系下查找距离 seed 最近的 body-only mesh 顶点作为 `surfaceLocal`。
- 如果无法吸附到 mesh 表面，keypoint 不显示，并输出 `[AnnotationSurfacePoint][WARN]`；不会把内部 seed 当作真实表面点继续做遮挡。
- `axis_surface` 作为后续可扩展模式预留，当前配置文件使用 `nearest_mesh_vertex`。

遮挡判断规则：

- 射线从相机世界位置指向 keypoint 世界位置。
- 检测所有当前可见目标的 annotation collision mesh，包括 keypoint 所属目标自身。
- 若最近命中距离 `hitDistance < keyDistance - OcclusionEpsilonM`，认为 keypoint 被自身或其它目标遮挡，不绘制、不进入 `latestRecord`。
- 若最近命中距离 `hitDistance >= keyDistance - OcclusionEpsilonM`，认为命中的是 keypoint 所在近侧表面，keypoint 可见。
- 如果 collision mesh 构建失败，不使用 AABB 兜底，只输出 `[AnnotationOcclusion][WARN] mesh_collision_unavailable ... noAabbFallback=1`，并按 projection-only 可见处理，避免误隐藏。

日志：

- collision mesh 构建：
  - `[AnnotationCollision] targetID=... platform=... triangles=... solids=... built=1`
- surface keypoint：
  - `[AnnotationSurfacePoint] targetID=... point=head seedLocal=... surfaceLocal=... snapped=...`
- mesh collision 遮挡：
  - `[AnnotationOcclusion] targetID=... point=head visible=... mode=mesh_collision hit=... hitTargetID=... hitDistance=... keyDistance=...`
- mesh collision 不可用：
  - `[AnnotationOcclusion][WARN] mesh_collision_unavailable targetID=... noAabbFallback=1`

Stage2A.2 验收清单：

1. Release x64 编译通过。
2. `head/middle` 来自 mesh 外表面吸附点，不再直接使用内部中心点。
3. keypoint 位于自身近侧表面时，应显示。
4. keypoint 位于自身背侧时，可被自身 mesh 更近表面遮挡。
5. 其它目标真实挡在相机到 keypoint 之间时，对应 keypoint 消失。
6. collision mesh 构建失败时，不使用 AABB 兜底，不误隐藏 keypoint。
7. bbox 和型号标签不受遮挡影响。

当前遗留：

- 当前 surface snap 使用最近 mesh 顶点，精度高于内部点但仍受模型顶点密度影响；若需要更高精度，可后续实现 triangle nearest point、depth/ID mask 或专门的 keypoint locator 节点。
- `axis_surface` 尚未作为独立算法实现，当前配置保持 `nearest_mesh_vertex`。
- 未做帧图像/视频/JSONL 保存，未处理 TCP `cv::flip(..., 0)` 后的坐标同步。

### 2026-06-09 阶段 2A.3：mesh collision 正确性和性能修复

本轮目标：

- 保留 Stage2A.2 的 Panda3D `CollisionPolygon / CollisionRay / CollisionTraverser` 真实 mesh collision 主路径。
- 修复 keypoint 误隐藏和帧率下降问题，优先保证其它目标遮挡正确、窗口实时标注可用。
- 不修改 `CommonData.h`、UDP/TCP、图片/视频/JSONL 保存、红外辐射、材质、大气、AGC、MTF、Stage3/4/5/6/7 链路。

误隐藏根因判断：

- Stage2A.2 默认 `SurfaceSnapEnable=true` 时，`nearest_mesh_vertex` 每帧可能把 profile seed 吸附到不符合人工语义的模型表面顶点，导致下方目标未被其它目标遮挡时 `head/middle` 仍可能落到不期望的位置。
- Stage2A.2 默认允许所属目标自身 mesh 参与遮挡，表面点不够稳定时容易被自身 mesh 更近表面误判为背侧遮挡。
- Stage2A.3 将默认策略改为 `profile_surface + OcclusionSelfTarget=false`：默认认为 `annotation_profiles.json` 中的 `localPos` 已是模型外表面点，先只判断其它目标遮挡；自身遮挡后续再用更可靠的表面点、法线或 depth/ID mask 处理。

帧率下降根因判断：

- Stage2A.2 每个 keypoint 都会新建 `CollisionRay / CollisionHandlerQueue / CollisionTraverser`。
- Stage2A.2 每个 keypoint 都 `traverse(renderRoot)`，遍历根包含完整场景树。
- Stage2A.2 每个 keypoint 都可能 `ensure` 所有目标 collision mesh。
- `SurfaceSnapEnable=true` 时，`nearest_mesh_vertex` 可能每帧全模型搜索。
- 真实渲染 mesh 三角面直接生成 `CollisionPolygon`，目标模型三角数较高时 collision solids 数量很大；Stage2A.3 先加高三角日志，不在本轮做降采样。

配置更新：

- `[Annotation]` 当前默认：
  - `OcclusionEnable=true`
  - `OcclusionMode=mesh_collision`
  - `OcclusionEpsilonM=0.25`
  - `OcclusionCollisionMaskBit=20`
  - `OcclusionSelfTarget=false`
  - `SurfaceKeyPointEnable=true`
  - `SurfaceSnapEnable=false`
  - `SurfaceSnapMode=profile_surface`
- `SurfaceSnapEnable=false` 时，keypoint 直接使用 profile `localPos`，日志输出：
  - `[AnnotationSurfacePoint] targetID=... point=head mode=profile_surface local=... pixel=...`
- `SurfaceSnapEnable=true` 且 `SurfaceSnapMode!=profile_surface` 时，才走 `nearest_mesh_vertex`，并按 `targetType + targetPlatID + targetID + pointName` 缓存；snap 失败不会隐藏 keypoint，只输出 WARN 并继续使用 profile 点。

性能修复：

- `AnnotationProjector::beginFrame(...)` 每帧只准备一次 collision root、可见目标 collision mesh 和候选列表。
- `AnnotationCollision_Target` 不再挂在目标节点下参与 `renderRoot` 遍历，而是挂到专用 `AnnotationCollisionRoot` 下；每帧同步目标相对 `renderRoot` 的 transform。
- 遮挡检测只 `traverse(AnnotationCollisionRoot)`，不再 `traverse(renderRoot)`。
- `CollisionRay / CollisionHandlerQueue / CollisionTraverser` 复用，不再每个 keypoint 反复 new/delete。
- collision mesh 继续按 `targetType + targetPlatID + targetID` 缓存，不每帧重建 `CollisionPolygon`。
- keypoint 遮挡前用目标 2D bbox 缓存做候选预筛，明显不覆盖 keypoint 像素的目标不激活 collision mesh。
- `OcclusionSelfTarget=false` 时，所属目标 collision mesh 不参与当前 keypoint 遮挡；命中处理阶段也按完整 key 过滤 self hit。

日志：

- 性能低频日志：
  - `[AnnotationPerf] frame=... targets=... keypoints=... bboxMs=... collisionBuildMs=... surfaceMs=... occlusionMs=... overlayMs=... totalMs=...`
- 遮挡命中隐藏：
  - `[AnnotationOcclusion] targetID=... point=head visible=0 reason=mesh_hit hitTargetID=... selfHit=0 hitDistance=... keyDistance=...`
- 无遮挡：
  - `[AnnotationOcclusion] targetID=... point=head visible=1 reason=no_hit ...`
- 三角面过多：
  - `[AnnotationCollision][WARN] highTriangleCount targetID=... triangles=...`

Stage2A.3 验收清单：

1. Release x64 编译通过。
2. `OcclusionEnable=false` 时，遮挡检测不运行，帧率应接近 Stage1.2。
3. `OcclusionEnable=true`、3 个目标、6 个 keypoint 下，重点观察 `[AnnotationPerf] totalMs` 是否稳定在 3-5 ms 目标范围内。
4. 下方目标未被其它目标真实遮挡时，`head/middle` 不应因自身 mesh 或错误 snap 消失。
5. 其它目标真实挡住 keypoint 时，对应 keypoint 消失。
6. bbox、型号标签、overlay、`latestRecord` 主结构不变。
7. `CommonData.h`、UDP/TCP、红外物理链路无 diff。

当前遗留：

- 当前默认关闭自身遮挡；自身遮挡建议后续结合精确 surface point、法线朝向或 depth/ID mask 单独开启。
- 当前 collision mesh 仍来自完整渲染 mesh；RK3588 60fps 目标下，后续应制作低模 collision mesh 或改用 depth/ID mask。
- 本轮只完成编译验证，未在真实窗口/UDP 场景中采集 `[AnnotationPerf]` 前后数值。

### 2026-06-09 阶段 2B：TCP 视频帧 + 实时数据 + 标注信息同步传输

本轮目标：

- 将 `HwaSimIR_TCP传输demo` 中“JPEG 视频帧随包携带 `DisplayC2cObjTrackingData`”的方法迁移到当前 HwaSimIR 主工程。
- 在同一个 TCP 显示帧包中同步发送三类数据：`DisplayC2cObjTrackingData`、`AnnotationFrameRecord` 转换得到的 UTF-8 JSON、JPEG 图像。
- 修改 `HwaSim_IR_VideoDisplay` 接收端，使其兼容旧的 `trackingData + JPEG` 显示帧包，并解析新的 `trackingData + annotationJson + JPEG` 显示帧包。
- 接收端保存视频时新增 `target_annotations.txt`，每保存一帧视频就追加一行 UTF-8 JSON 标注；原 `annotations.txt` 实时数据 CSV 保持不改名、不混入标注 JSON。
- 不修改 `CommonData.h`、UDP 0x41/0x36/0x38 解析流程、TCP 初始化/控制命令格式、红外辐射/材质/大气/AGC/MTF/Stage3-7 物理链路，不做 HwaSimIR 本地图像/视频/JSONL 保存。

TCP 新显示帧包格式：

```text
totalLen:uint32 big-endian
structLen1:uint32 big-endian
struct1: BYHWICD::DisplayC2cObjTrackingData
structLen2:uint32 big-endian
struct2: annotationJson UTF-8
structLen3:uint32 big-endian
struct3: JPEG bytes
```

说明：

- `totalLen` 继续沿用 demo 的总长度语义，包含自身 4 字节长度头。
- `struct1.flag` 仍为 `0x38`，不向 `CommonData.h` 新增字段。
- TCP 后台线程只使用主线程复制好的 RGB 帧、实时数据和标注快照，不访问 Panda3D `NodePath`、`AnnotationManager` 或 HwaSimIR 场景对象。
- 发送使用 `sendAll` 循环，避免大 JPEG/JSON 包被单次 `send` 截断。

`annotationJson` 字段：

```json
{
  "version": 1,
  "enabled": true,
  "frameIndex": 123,
  "simTimeMs": 456.0,
  "sensorID": 0,
  "width": 800,
  "height": 800,
  "targets": [
    {
      "targetType": 34,
      "targetTypeHex": "0x22",
      "modelLabel": "AIM120D",
      "targetPlatID": 1,
      "targetID": 2,
      "bboxCorners": [
        {"x": 310, "y": 220},
        {"x": 390, "y": 220},
        {"x": 390, "y": 246},
        {"x": 310, "y": 246}
      ],
      "keyPoints": [
        {"name": "head", "x": 376, "y": 228, "visible": true},
        {"name": "middle", "x": 344, "y": 232, "visible": true}
      ]
    }
  ]
}
```

标注关闭或本帧无标注目标时：

```json
{"version":1,"enabled":false,"frameIndex":123,"simTimeMs":456.0,"sensorID":0,"width":800,"height":800,"targets":[]}
```

坐标与 `cv::flip(..., 0)` 结论：

- `AnnotationFrameRecord` 的坐标已定义为最终显示/输出图像左上角坐标，`x` 向右，`y` 向下。
- 当前 TCP 发送前的 `cv::flip(rawFrame, flippedFrame, 0)` 用于把 Panda3D RAM 图像翻成接收端 JPEG 正常显示方向；翻转后的 JPEG 与 `AnnotationFrameRecord` 使用同一个左上角坐标系。
- 因此 Stage2B 发送 `annotationJson` 时不再对 `y` 做二次翻转。
- 若未来去掉 TCP flip 或改变 capture RAM 图像方向，必须同步复核此结论，避免接收端图像和标注上下颠倒。
- 若 `AnnotationFrameRecord.width/height` 与最终 TCP JPEG 宽高不同，发送端按最终 JPEG 宽高对 bbox/keypoint 做等比例坐标缩放后输出 JSON。

接收端保存规则：

- `HwaSim_IR_VideoDisplay` 在每个回合目录中继续生成：
  - `output.mp4`
  - 原实时数据文件 `annotations.txt`
  - 新增标注文件 `target_annotations.txt`
- `target_annotations.txt` 使用 `.txt` 扩展名，但每行内容为 UTF-8 JSON。
- 每写入一帧视频，就写入一行标注 JSON；行数应与视频帧数一一对应。
- 新三段式包携带的 JSON 优先原样保存；旧包未携带 JSON 时，接收端生成 `enabled=false` 占位 JSON，保证行数同步。
- 停止、复位和析构时通过 `CloseStorage()` flush 并 close 视频、原实时数据文件和 `target_annotations.txt`。

兼容性：

- 0x36 初始化命令和 0x41 控制命令解析逻辑保持不变。
- 0x38 显示帧支持新三段式包。
- 0x38 显示帧尽量兼容旧 `trackingData + JPEG` 两段式包。
- 接收端额外兼容最老的 `4 字节 JPEG 长度 + JPEG` 纯图像包；此时实时数据为空快照，标注文件写 `enabled=false` 占位 JSON。

当前遗留：

- 本轮只做 TCP 对外传输和接收端随视频保存标注，不实现 HwaSimIR 本地 `FrameOutputCoordinator`。
- HwaSimIR 本地图像帧、视频、逐帧标注、逐帧实时数据统一保存仍留到后续阶段。
- 未做真实联机人工验收；需要启动接收端和 HwaSimIR 后检查视频显示、实时表格更新、`output.mp4`、`annotations.txt`、`target_annotations.txt` 三者是否同步生成。

### 2026-06-09 Stage2B 补充修复：TCP 初始化/控制命令转发

测试暴露的问题：

- HwaSimIR 能接收激励程序的 0x36 初始化命令和 0x41 控制命令，但 Stage2B 初版只在 TCP 显示帧包中发送 `DisplayC2cObjTrackingData + annotationJson + JPEG`。
- 0x36/0x41 没有同步转发给 `HwaSim_IR_VideoDisplay`，因此接收端不会触发初始化界面更新，也不会收到开始/停止命令去创建保存目录、打开/关闭 `output.mp4`、原实时数据文件和 `target_annotations.txt`。

修复内容：

- `TcpCommThread` 增加单结构体 TCP 包发送接口，格式保持 demo 方式：

```text
totalLen:uint32 big-endian
structLen:uint32 big-endian
struct: InitP2cObjectTrackingCmd(flag=0x36) 或 ControlP2cX1ObjTrackingCmd(flag=0x41)
```

- `HwaSimIR::handleInitCmd(...)` 在完成本端初始化并发送 UDP 初始化应答后，调用 `TcpCommThread::sendInitCmd(cmd)` 转发 0x36 给接收端。
- `HwaSimIR::handleControlCmd(...)` 在复位、开始、停止分支中调用 `TcpCommThread::sendControlCmd(cmd)` 转发 0x41 给接收端。
- 复位和开始分支同步调用 `resetInitCompleted()`，保证下一回合初始化命令可以重新转发。
- TCP socket 发送增加互斥保护，避免控制/初始化单结构体包与视频帧三段式包在同一 TCP 连接上交叉写入。

边界保持：

- 不修改 `CommonData.h` 中任何协议结构体字段、顺序、类型和 `sizeof`。
- 不修改 UDP 0x41/0x36/0x38 解析流程。
- 不修改红外辐射、材质、大气、AGC、MTF、Stage3-7 物理链路。
- 接收端既能解析 0x36/0x41 单结构体包，也继续解析 0x38 显示帧包。

### 2026-06-10 Stage2B 补充修复：接收端延迟开始保存

测试暴露的问题：

- `HwaSim_IR_VideoDisplay` 的录制开关只由 TCP 转发的 0x41 控制命令驱动；如果 HwaSimIR 未转发 0x41，接收端不会进入保存流程。
- 0x41 开始命令到达后，接收端可能先收到若干帧实时数据为空的 0x38 显示帧，此时 `TargetState targetState[5]` 仍是清零状态。
- 若开始命令一到就创建目录并写视频/标注/实时数据，会把空实时帧也保存进去，造成保存数据开头无目标、视频帧和有效目标数据不同步。

修复内容：

- 接收端从 0x36 初始化命令读取 `trackerSensor[0].saveMP4En`，作为本回合是否允许保存 MP4/数据/标注的开关。
- 收到 0x41 开始命令时：
  - 若 `saveMP4En=false`，只更新运行状态，不创建保存目录，不写文件。
  - 若 `saveMP4En=true`，只进入 `recordingPending` 待录制状态，暂不创建 `MP4/round_xxx_timestamp` 目录。
- 每帧 0x38 显示帧到达后，先判断 `targetState[5]` 是否已有有效目标数据；当前以 `targetType != 0` 作为“该目标槽有数据”的判断条件，与目标表格显示逻辑一致。
- 第一帧有效目标数据到达时，才创建：
  - `output.mp4`
  - `annotations.txt`
  - `target_annotations.txt`
- 从第一帧有效目标数据开始，视频、实时数据和标注 JSON 同步逐帧写入。
- 复位、停止、析构时清理 `recordingPending` 和已打开的保存资源。

验收关注：

1. `saveMP4En=false`：收到开始命令后不生成保存目录。
2. `saveMP4En=true`：收到开始命令但实时 `targetState[5]` 为空时，不生成保存目录、不写文件。
3. 第一帧 `targetState[5]` 有目标数据后，才生成保存目录并从该帧开始写 `output.mp4`、`annotations.txt`、`target_annotations.txt`。
4. `target_annotations.txt` 行数应等于实际写入视频的帧数。
5. 停止命令后保存文件 flush 并 close。
