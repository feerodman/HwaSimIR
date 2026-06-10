# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概况

红外仿真图像接收器（HwaSim_IR_VideoDisplay），Qt 5.12.12 C++ 桌面应用。作为乙方（边缘端红外仿真软件），通过 TCP 端口 5555 接收甲方（激励数据软件）的仿真数据包，显示红外图像、平台/目标运动数据，并存储为 MP4 + 标注文件。

## 构建

- **IDE**: Qt Creator 5.0.2，使用 Qt 5.12.12 MSVC2019 64bit 套件
- **项目文件**: `HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.vcxproj`（Visual Studio 格式，由 Qt VS Tools 生成）
- **OpenCV**: 位于 `HwaSim_IR_VideoDisplay/Thirdparty library/opencv2-440/`，Release|x64 配置中 include/lib 路径指向 `opencv_x64\vc15\lib\opencv_world440.lib`
- 直接在 Visual Studio 或 Qt Creator 中打开 `.vcxproj` 构建
- **运行时部署**：必须将以下 DLL 复制到 exe 同目录（`x64/Release/`），否则 VideoWriter 无法打开：
  - `opencv_world440.dll`
  - `opencv_videoio_ffmpeg440_64.dll`
  - `opencv_videoio_msmf440_64.dll`
  - 来源：`Thirdparty library\opencv2-440\opencv_x64\vc15\bin\`

## 架构

```
main.cpp → HwaSim_IR_VideoDisplay (QWidget, 主窗口/UI)
                ├── InitTables()     → 配置 tableWidget_platData / tableWidget_targetData（Stretch 模式）
                ├── InitQss()        → 从 appDir/qss/style.css 加载全局样式
                └── 启动工作线程
                        └── TcpServerWorker : QObject (moveToThread)
                                ├── QTcpServer 监听 IP/端口（从 NetworkConfig.ini 读取）
                                ├── loadConfig() → QSettings 读取 [Network] ip/port
                                ├── 解析协议包 (CommonData.h)，大端序
                                ├── 丢弃积压的旧 0x38 帧（peek 防误吞 0x36/0x41）
                                ├── emit dataReceived(QImage, DisplayC2cObjTrackingData)
                                ├── emit initCommandReceived(InitP2cObjectTrackingCmd)
                                └── emit controlCmdReceived(ControlP2cX1ObjTrackingCmd)
```

**线程安全**：析构时先 stop→quit→wait 停线程，再 CloseStorage 释放资源，避免队列中残留信号访问已释放指针。

## 通信协议 (CommonData.h)

数据包格式：4字节总长（大端） + 4字节结构体长度 + 结构体数据。`#pragma pack(1)` 紧凑对齐。

| Flag | 方向 | 结构体 | 说明 |
|------|------|--------|------|
| 0x36 | 甲→乙 | InitP2cObjectTrackingCmd | 成像初始化（传感器参数 + platParam[2] 平台初始状态） |
| 0x37 | 乙→甲 | InitAckC2pObjectTrackingCmd | 初始化应答（**已注释，故意不发送**） |
| 0x38 | 甲→乙 | DisplayC2cObjTrackingData + JPEG | 实时成像数据（双结构体：跟踪数据 + JPEG 图像） |
| 0x41 | 甲→乙 | ControlP2cX1ObjTrackingCmd | 控制命令：simCommand 1=复位 2=开始 3=停止 |

## UI 结构

- `Wgt_title` — 标题栏
- `dockWidget_dataShow` (QDockWidget, left dock, stretch=2) — 可悬浮/隐藏面板
  - `groupBox_dataShow` — 场景类型 + lineEdit_12 系统控制参数
  - `groupBox_4` — 传感器探测成像参数（29 个 lineEdit）
  - `groupBox_platData` → `tableWidget_platData` — 平台数据 9 列
  - `groupBox_targetData` → `tableWidget_targetData` — 目标数据 11 列
- `widget_video` (stretch=5) → `m_Label_Video` (QLabel) — 绝对定位，`centerVideoLabel()` 居中+保持宽高比

### 表格配置

两个表格均使用 `QHeaderView::Stretch` 模式（列均分占满行宽），`minimumSectionSize=28`，表头 QSS: `bold 13px` / `min-height: 28px`。

## 数据存储

收到开始命令(simCommand=2) 时创建 `{appDir}/MP4/round_NNN_时间戳/` 目录：
- `output.mp4` — cv::VideoWriter，编码器降级策略：H264 → XVID → MJPG（Windows 兼容），文件扩展名固定 .mp4
- `annotations.txt` — CSV 标注，首行 header，每帧每个目标一行

VideoWriter 首帧延迟创建（此时才知晓图像实际分辨率）。收到停止命令(3) 或复位(1) 时释放。

## 关键实现细节

- **平台数据更新**：`updatePlatDataTable(platID, platLoc)` 按第 0 列平台 ID 精确匹配行，不覆盖其他平台
- **目标遍历**：遍历 `targetState[5]`，`targetType==0` 时 break（不使用 targetNumValid）
- **QImage→cv::Mat**：`convertToFormat(RGB888)` → `cv::Mat` 只读引用 → `cvtColor` 到独立 `dst` → write，避免写 QImage 缓冲区
- **QSS 加载**：从运行目录 `qss/style.css` 加载，失败时 qWarning 不崩溃

## 编码约定

- 全部中文注释和 UI 文本
- QSS 绿色主题：rgb(135,244,165) / rgb(36,192,100) / rgb(4,87,50)
- 工作线程 signal/slot 跨线程通信（Qt::QueuedConnection 默认）
- `#pragma execution_character_set("utf-8")` 确保中文日志不乱码
