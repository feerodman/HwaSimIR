const ROOT = "D:/HwaSimIR/outputs/manual-20260607-hwasimir-product-plan/presentations/hwasimir-ir-product-roadmap";
const BG = {
  cover: `${ROOT}/template-inspect/source-slides/source-slide-01.png`,
  contents: `${ROOT}/template-inspect/source-slides/source-slide-02.png`,
  blank: `${ROOT}/template-inspect/source-slides/source-slide-04.png`,
  thanks: `${ROOT}/template-inspect/source-slides/source-slide-10.png`,
};

const COLORS = {
  blue: "#124B98",
  blue2: "#4976D1",
  navy: "#09285C",
  green: "#4CB494",
  red: "#C43D32",
  orange: "#D9812B",
  gray: "#5F6B7A",
  pale: "#EAF1FB",
  pale2: "#F6FAFF",
  border: "#9DB8E7",
  white: "#FFFFFF",
};

const FONT = "微软雅黑";

async function addBackground(slide, ctx, name = "blank") {
  await ctx.addImage(slide, {
    path: BG[name],
    x: 0,
    y: 0,
    w: 960,
    h: 720,
    fit: "fill",
    alt: "template background",
  });
}

function addText(ctx, slide, text, x, y, w, h, options = {}) {
  return ctx.addText(slide, {
    text,
    x,
    y,
    w,
    h,
    fontSize: options.size ?? 18,
    color: options.color ?? COLORS.navy,
    bold: options.bold ?? false,
    typeface: options.face ?? FONT,
    align: options.align ?? "left",
    valign: options.valign ?? "top",
    fill: options.fill ?? "#00000000",
    line: options.line ?? ctx.line("#00000000", 0),
    insets: options.insets ?? { left: 0, right: 0, top: 0, bottom: 0 },
  });
}

function rect(ctx, slide, x, y, w, h, fill, lineFill = "#00000000", lineWidth = 0) {
  return ctx.addShape(slide, {
    x,
    y,
    w,
    h,
    fill,
    line: ctx.line(lineFill, lineWidth),
  });
}

function slideTitle(ctx, slide, title, subtitle) {
  addText(ctx, slide, title, 95, 72, 710, 34, {
    size: 24,
    bold: true,
    color: COLORS.white,
  });
  if (subtitle) {
    addText(ctx, slide, subtitle, 96, 111, 730, 24, {
      size: 12,
      color: COLORS.white,
    });
  }
}

function pageNote(ctx, slide, text) {
  addText(ctx, slide, text, 720, 664, 170, 18, {
    size: 8,
    color: "#6B7280",
    align: "right",
  });
}

function metric(ctx, slide, x, y, w, h, value, label, color = COLORS.blue2) {
  rect(ctx, slide, x, y, w, h, COLORS.white, color, 2);
  addText(ctx, slide, value, x + 10, y + 12, w - 20, 30, {
    size: 24,
    bold: true,
    color,
    align: "center",
  });
  addText(ctx, slide, label, x + 12, y + 48, w - 24, h - 54, {
    size: 12,
    color: COLORS.gray,
    align: "center",
  });
}

function card(ctx, slide, x, y, w, h, title, body, color = COLORS.blue2) {
  rect(ctx, slide, x, y, w, h, COLORS.white, color, 1.5);
  rect(ctx, slide, x, y, w, 28, color);
  addText(ctx, slide, title, x + 12, y + 6, w - 24, 20, {
    size: 13,
    bold: true,
    color: COLORS.white,
  });
  addText(ctx, slide, body, x + 14, y + 42, w - 28, h - 50, {
    size: 11.5,
    color: COLORS.navy,
    insets: { left: 0, right: 0, top: 0, bottom: 0 },
  });
}

function miniCard(ctx, slide, x, y, w, h, title, body, color = COLORS.blue2) {
  rect(ctx, slide, x, y, w, h, COLORS.pale2, color, 1);
  addText(ctx, slide, title, x + 10, y + 10, w - 20, 20, {
    size: 12.5,
    bold: true,
    color,
  });
  addText(ctx, slide, body, x + 10, y + 35, w - 20, h - 42, {
    size: 10.7,
    color: COLORS.navy,
  });
}

function tableCell(ctx, slide, x, y, w, h, text, options = {}) {
  rect(ctx, slide, x, y, w, h, options.fill ?? COLORS.white, options.border ?? COLORS.border, 0.8);
  addText(ctx, slide, text, x + 6, y + 6, w - 12, h - 10, {
    size: options.size ?? 9.8,
    bold: options.bold ?? false,
    color: options.color ?? COLORS.navy,
    align: options.align ?? "left",
    valign: "top",
  });
}

async function sectionSlide(presentation, ctx, no, title, subtitle) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx);
  rect(ctx, slide, 98, 263, 220, 195, "#F2F4F8", "#E2E8F0", 1);
  addText(ctx, slide, no, 157, 315, 100, 70, {
    size: 42,
    bold: true,
    color: COLORS.blue2,
    align: "center",
  });
  rect(ctx, slide, 378, 230, 3, 260, "#111111");
  addText(ctx, slide, title, 430, 285, 430, 48, {
    size: 30,
    bold: true,
    color: COLORS.blue,
  });
  addText(ctx, slide, subtitle, 432, 345, 410, 80, {
    size: 15,
    color: COLORS.gray,
  });
  return slide;
}

export async function slide01(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx, "cover");
  rect(ctx, slide, 54, 126, 852, 128, "#FFFFFF", COLORS.border, 1);
  addText(ctx, slide, "三维场景红外成像仿真产品规划", 80, 148, 800, 42, {
    size: 32,
    bold: true,
    color: COLORS.blue,
    align: "center",
  });
  addText(ctx, slide, "基于国产化 ARM 板卡与开源 3D 引擎的产品化平台及行业解决方案", 95, 200, 770, 30, {
    size: 16,
    color: COLORS.navy,
    align: "center",
  });
  rect(ctx, slide, 130, 475, 700, 44, "#FFFFFF", COLORS.blue2, 1);
  addText(ctx, slide, "方向判断：核心产品平台 + 军工/低空/航天行业解决方案包 + AI 驱动的仿真数据能力", 148, 488, 664, 20, {
    size: 13,
    bold: true,
    color: COLORS.blue,
    align: "center",
  });
  addText(ctx, slide, "产品规划讨论稿 | HwaSimIR | 2026.06", 650, 630, 230, 24, {
    size: 11,
    color: COLORS.gray,
    align: "right",
  });
  return slide;
}

export async function slide02(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx, "contents");
  addText(ctx, slide, "产品/方案定位与市场判断", 470, 254, 330, 30, {
    size: 18,
    bold: true,
    color: COLORS.blue,
  });
  addText(ctx, slide, "技术基础、行业壁垒与集团协同", 470, 379, 330, 30, {
    size: 18,
    bold: true,
    color: COLORS.blue,
  });
  addText(ctx, slide, "实现路径、投入产出与 AI 化演进", 470, 505, 330, 30, {
    size: 18,
    bold: true,
    color: COLORS.blue,
  });
  return slide;
}

export async function slide03(presentation, ctx) {
  return sectionSlide(
    presentation,
    ctx,
    "01",
    "产品/方案定位与市场判断",
    "先判断做什么、卖给谁、为什么现在值得做。",
  );
}

export async function slide04(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx);
  slideTitle(ctx, slide, "定位建议：不是单一项目交付，而是产品化平台 + 行业解决方案包", "V1.0 聚焦红外成像仿真内核，后续向多模态传感器仿真平台演进");

  rect(ctx, slide, 348, 212, 264, 118, COLORS.blue, COLORS.blue, 1);
  addText(ctx, slide, "HwaSimIR\n三维红外成像仿真平台", 370, 232, 220, 58, {
    size: 20,
    bold: true,
    color: COLORS.white,
    align: "center",
  });
  addText(ctx, slide, "仿真内核 + 运行时 + SDK/API + 数据/模型库", 372, 292, 216, 22, {
    size: 10.5,
    color: COLORS.white,
    align: "center",
  });

  card(ctx, slide, 70, 176, 230, 205, "产品形态", "• GUI 场景配置与批量试验\n• SDK/API 与协议封装\n• 国产 ARM 运行时适配包\n• 红外材料、温度、大气、目标库\n• Windows/Linux/ARM 跨平台交付", COLORS.blue2);
  card(ctx, slide, 660, 176, 230, 205, "解决方案形态", "• 军工半实物/实验室测试\n• 低空无人机红外感知验证\n• 商业航天载荷/在轨场景仿真\n• 安防与应急红外监控演训\n• 多模态雷达 + 红外融合验证", COLORS.green);

  rect(ctx, slide, 150, 436, 660, 82, "#EEF4FF", COLORS.blue2, 1);
  addText(ctx, slide, "结论", 170, 453, 56, 24, { size: 18, bold: true, color: COLORS.blue });
  addText(ctx, slide, "用“可复用平台”沉淀壁垒，用“行业包”贴近客户预算与场景；先做军工确定性订单，再向低空经济和商业航天扩展。", 240, 450, 540, 42, {
    size: 15,
    color: COLORS.navy,
  });
  return slide;
}

export async function slide05(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx);
  slideTitle(ctx, slide, "战略契合：把公司已有仿真能力推向低空经济与商业航天传感器验证", "红外成像是低空安全、航天载荷、军工装备测试中的关键感知通道");

  metric(ctx, slide, 70, 170, 185, 98, "低空经济", "无人机监管、反制、航线安全、园区/机场低空感知", COLORS.blue2);
  metric(ctx, slide, 275, 170, 185, 98, "商业航天", "卫星载荷算法验证、在轨红外场景、星地仿真", COLORS.green);
  metric(ctx, slide, 480, 170, 185, 98, "军工装备", "导引头/红外设备实验室测试、半实物仿真", COLORS.red);
  metric(ctx, slide, 685, 170, 185, 98, "自主可控", "国产 ARM、开源引擎、可封装接口，减少外部依赖", COLORS.orange);

  rect(ctx, slide, 90, 330, 780, 156, COLORS.white, COLORS.border, 1);
  addText(ctx, slide, "和公司现有方向的连接方式", 112, 348, 260, 28, {
    size: 18,
    bold: true,
    color: COLORS.blue,
  });
  addText(ctx, slide, "AFSIM：接任务/战术场景与 DIS/HLA 联合仿真", 125, 392, 330, 24, { size: 13, color: COLORS.navy });
  addText(ctx, slide, "VR：接沉浸式训练、装备操作和态势复盘", 125, 427, 330, 24, { size: 13, color: COLORS.navy });
  addText(ctx, slide, "虚幻5：作为下一代高保真视觉与大规模场景路线", 505, 392, 330, 24, { size: 13, color: COLORS.navy });
  addText(ctx, slide, "雷达成像：与红外成像形成多模态传感器仿真矩阵", 505, 427, 330, 24, { size: 13, color: COLORS.navy });

  rect(ctx, slide, 160, 540, 640, 48, COLORS.blue, COLORS.blue, 1);
  addText(ctx, slide, "战略价值：从单点红外 demo，升级为面向任务体系的“传感器仿真底座”", 180, 554, 600, 22, {
    size: 14,
    bold: true,
    color: COLORS.white,
    align: "center",
  });
  return slide;
}

export async function slide06(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx);
  slideTitle(ctx, slide, "市场方向与具体用户：军工是首批确定性入口，低空与航天打开增量空间", "先从高价值、强国产化约束客户切入，再形成可复制行业方案");

  const x = 54;
  const y = 168;
  const widths = [120, 225, 245, 245];
  const heads = ["市场方向", "具体用户", "核心痛点", "推荐切入产品/方案"];
  let cx = x;
  for (let i = 0; i < heads.length; i += 1) {
    tableCell(ctx, slide, cx, y, widths[i], 34, heads[i], {
      fill: COLORS.blue,
      color: COLORS.white,
      bold: true,
      size: 11.5,
      align: "center",
      border: COLORS.blue,
    });
    cx += widths[i];
  }
  const rows = [
    ["军工装备", "军工研究所、总体单位、红外设备厂商、靶场/试验基地", "实地试验昂贵、场景不可复现、国产化替代压力高", "红外设备实验室测试平台、半实物仿真、导引/识别算法数据生成"],
    ["低空经济", "低空监管平台、机场/园区/港口、无人机运营商、反无人机集成商", "低空目标复杂、红外/可见光/雷达协同验证不足", "无人机红外感知与反制仿真包、低空安全数字孪生"],
    ["商业航天", "卫星总体单位、载荷研制单位、商业航天公司、地面运控/仿真团队", "载荷算法上星前验证不足，空间/地物红外场景构建成本高", "星地红外场景仿真、载荷算法闭环测试、星座任务评估接口"],
    ["安防应急", "边海防、能源电力、消防应急、重点设施安防", "夜间、烟雾、复杂天气下数据缺少，实测组织成本高", "红外监控场景库、应急演训仿真、算法评测数据集"],
    ["科研教育", "高校、科研院所、实验室、公司内部测试部门", "教学/科研缺少低成本可复现实验环境", "教学版/科研版、二次开发 SDK、材料与目标模型库"],
  ];
  let cy = y + 34;
  for (const row of rows) {
    cx = x;
    for (let i = 0; i < row.length; i += 1) {
      tableCell(ctx, slide, cx, cy, widths[i], 67, row[i], {
        fill: i === 0 ? "#EEF4FF" : COLORS.white,
        bold: i === 0,
        size: i === 0 ? 11 : 9.2,
        color: i === 0 ? COLORS.blue : COLORS.navy,
      });
      cx += widths[i];
    }
    cy += 67;
  }
  pageNote(ctx, slide, "客户分层为规划建议，需结合销售线索校准");
  return slide;
}

export async function slide07(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx);
  slideTitle(ctx, slide, "主要对标：不是找完全同类，而是拆分为平台、红外物理、任务集成三类对标", "断网环境下未做实时版本/价格核验，以下用于产品规划参照");

  const x = 62;
  const y = 162;
  const widths = [142, 166, 245, 245];
  const heads = ["对标公司", "代表方向", "可学习点", "HwaSimIR 差异化机会"];
  let cx = x;
  for (let i = 0; i < heads.length; i += 1) {
    tableCell(ctx, slide, cx, y, widths[i], 34, heads[i], {
      fill: COLORS.blue,
      color: COLORS.white,
      bold: true,
      size: 11,
      align: "center",
      border: COLORS.blue,
    });
    cx += widths[i];
  }
  const rows = [
    ["Presagis / CAE", "Vega Prime / Creator 等视景仿真", "成熟视景工具链、场景资产、仿真生态", "国产 ARM + 开源可控 + 红外专用链路，降低授权与禁运风险"],
    ["ThermoAnalytics", "MuSES 等热/红外签名仿真", "热模型、材料参数、红外签名工程化", "与国产场景引擎和半实物测试流程深度集成"],
    ["OKTAL-SE / SOGECLAIR", "传感器与 EO/IR 场景仿真", "传感器级仿真、任务场景、工程验证流程", "低空/航天/军工本地化场景库与交付服务"],
    ["MAK Technologies", "VR-Forces / VR-Vantage 联合仿真", "DIS/HLA、兵棋/任务级仿真集成", "承接公司 AFSIM/VR 资源，补齐红外感知通道"],
    ["Ansys / AGI", "STK 任务分析与传感器链路", "航天任务建模、星座/载荷/链路分析", "商业航天场景中补充国产红外成像仿真与数据生成"],
  ];
  let cy = y + 34;
  for (const row of rows) {
    cx = x;
    for (let i = 0; i < row.length; i += 1) {
      tableCell(ctx, slide, cx, cy, widths[i], 70, row[i], {
        fill: i === 0 ? "#EEF4FF" : COLORS.white,
        bold: i === 0,
        size: i === 0 ? 10.5 : 9.2,
        color: i === 0 ? COLORS.blue : COLORS.navy,
      });
      cx += widths[i];
    }
    cy += 70;
  }
  pageNote(ctx, slide, "竞品清单建议后续由市场/售前补充国内厂商与价格信息");
  return slide;
}

export async function slide08(presentation, ctx) {
  return sectionSlide(
    presentation,
    ctx,
    "02",
    "技术基础、行业壁垒与集团协同",
    "把近期 HwaSimIR 工程积累转成可出售、可交付、可持续演进的产品能力。",
  );
}

export async function slide09(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx);
  slideTitle(ctx, slide, "已有技术基础：HwaSimIR 已具备从物理模型到运行验证的产品化雏形", "壁垒来自“红外物理 + 三维引擎 + 国产硬件 + 协议联调 + 自动化验证”的组合");

  miniCard(ctx, slide, 58, 162, 198, 158, "工程底座", "C++ 仿真与图形项目集合，包含 Qt、Visual Studio/CMake、Panda3D、OpenCV、Eigen；具备 UI、三维场景、图像处理和原生性能优化基础。", COLORS.blue2);
  miniCard(ctx, slide, 275, 162, 198, 158, "红外物理链路", "支持多波段红外仿真、温度场、大气效应；Stage 3 已形成 MODTRAN tau-only 受控加载与调试链路。", COLORS.green);
  miniCard(ctx, slide, 492, 162, 198, 158, "目标与协议联动", "Stage 4 将发动机尾焰 ThermalHotspot 与武器命中 BrightSpot 分离；目标解析使用 targetType + targetPlatID + targetID 全协议键。", COLORS.orange);
  miniCard(ctx, slide, 709, 162, 198, 158, "验证工具链", "已形成 stage0 构建、Stage3/Stage4 严格检查、可视化 smoke、日志诊断；后续 Stage5 辐亮度/可视性校准可纳入产品验证。", COLORS.red);

  rect(ctx, slide, 75, 370, 810, 120, COLORS.white, COLORS.border, 1);
  addText(ctx, slide, "可构建的行业壁垒", 98, 390, 180, 25, { size: 18, bold: true, color: COLORS.blue });
  addText(ctx, slide, "1. 全国产化/ARM 端部署经验：比通用 X86 视景软件更贴近信创与嵌入式交付。", 115, 432, 720, 20, { size: 12.5, color: COLORS.navy });
  addText(ctx, slide, "2. 可解释红外链路：大气、温度、材质、热点/亮点、灰度输出都有日志与回归检查。", 115, 456, 720, 20, { size: 12.5, color: COLORS.navy });
  addText(ctx, slide, "3. 任务系统连接能力：协议键、目标状态、武器状态为接入 AFSIM/VR/半实物系统留下接口。", 115, 480, 720, 20, { size: 12.5, color: COLORS.navy });

  rect(ctx, slide, 145, 540, 670, 48, COLORS.blue, COLORS.blue, 1);
  addText(ctx, slide, "产品化关键：把“可跑、可调、可查”升级为“可配置、可封装、可验收、可复用”。", 166, 554, 628, 20, {
    size: 14,
    bold: true,
    color: COLORS.white,
    align: "center",
  });
  return slide;
}

export async function slide10(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx);
  slideTitle(ctx, slide, "平台架构：以红外仿真内核为中心，整合 AFSIM、VR、UE5、雷达成像资源", "不要局限在现有 demo，把集团资源组织成可交付的仿真产品线");

  const lanes = [
    ["应用层", "GUI 场景编辑器\n批量试验管理\n报告/图像序列导出"],
    ["接口层", "SDK/API\nUDP/JSON/二进制协议\nDIS/HLA/AFSIM 适配"],
    ["仿真内核", "多波段红外\n温度/材质/大气\n热点/亮点/辐亮度链路"],
    ["运行层", "Windows/Linux\n国产 ARM 板卡\nGPU/FPGA 加速预留"],
    ["资源层", "三维模型库\n材料/目标库\n实测校准数据/AI 数据集"],
  ];
  let y = 160;
  for (const [label, body] of lanes) {
    rect(ctx, slide, 92, y, 128, 58, COLORS.blue, COLORS.blue, 1);
    addText(ctx, slide, label, 102, y + 17, 108, 20, { size: 14, bold: true, color: COLORS.white, align: "center" });
    rect(ctx, slide, 230, y, 405, 58, COLORS.white, COLORS.border, 1);
    addText(ctx, slide, body, 246, y + 9, 372, 42, { size: 12.2, color: COLORS.navy });
    y += 76;
  }

  rect(ctx, slide, 680, 160, 190, 360, "#EEF8F5", COLORS.green, 1.5);
  addText(ctx, slide, "集团协同资源", 700, 178, 150, 24, { size: 17, bold: true, color: COLORS.green, align: "center" });
  addText(ctx, slide, "AFSIM\n任务级场景、战术推演、联合仿真入口", 700, 224, 150, 46, { size: 11.2, color: COLORS.navy, align: "center" });
  addText(ctx, slide, "VR\n训练、操作、复盘与沉浸式交互", 700, 286, 150, 42, { size: 11.2, color: COLORS.navy, align: "center" });
  addText(ctx, slide, "虚幻5\n下一代高保真视景与大规模场景", 700, 346, 150, 42, { size: 11.2, color: COLORS.navy, align: "center" });
  addText(ctx, slide, "雷达成像\n多模态传感器融合、目标识别评估", 700, 406, 150, 42, { size: 11.2, color: COLORS.navy, align: "center" });
  addText(ctx, slide, "硬件/渠道\n国产板卡、项目客户、试验资源", 700, 466, 150, 42, { size: 11.2, color: COLORS.navy, align: "center" });

  pageNote(ctx, slide, "架构为产品规划图，不代表已全部实现");
  return slide;
}

export async function slide11(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx);
  slideTitle(ctx, slide, "创新点、难点与 AI 化：红外仿真产品要从“会渲染”走向“会生成、会校准、会评估”", "AI 不是单独包装，而是嵌入场景生成、参数反演、数据合成和测试自动化");

  card(ctx, slide, 62, 170, 260, 265, "核心创新点", "• 国产 ARM 板卡 + 开源 3D 引擎的红外全链路适配\n• 多波段红外、温度场、大气效应、热点/亮点独立建模\n• 面向半实物/任务级系统的协议化封装\n• 低成本替代国外软件许可并可按客户定制", COLORS.blue2);
  card(ctx, slide, 350, 170, 260, 265, "关键难点", "• ARM 端 GPU 性能与实时帧率\n• 红外物理精度与实测数据校准\n• 材质/温度/大气参数库持续建设\n• 开源协议、军工合规、交付环境适配\n• UE5 高保真路线与轻量 ARM 路线的边界划分", COLORS.orange);
  card(ctx, slide, 638, 170, 260, 265, "AI 化方向", "• 文本/模板驱动的低空与航天场景生成\n• 实测图像反推材质、温度和大气参数\n• 合成红外数据集支撑识别算法训练\n• 自动生成测试用例、异常日志诊断与报告\n• 红外 + 雷达 + 可见光多模态评估闭环", COLORS.green);

  rect(ctx, slide, 120, 498, 720, 58, COLORS.blue, COLORS.blue, 1);
  addText(ctx, slide, "产品演进主线：Panda3D/ARM 轻量化交付负责“能落地”，UE5/AI/多模态负责“拉开体验与数据壁垒”。", 150, 515, 660, 22, {
    size: 13.5,
    bold: true,
    color: COLORS.white,
    align: "center",
  });
  return slide;
}

export async function slide12(presentation, ctx) {
  return sectionSlide(
    presentation,
    ctx,
    "03",
    "实现路径、投入产出与 AI 化演进",
    "用五年路线把 V1.0、客户试点、集团协同和商业化收益串起来。",
  );
}

export async function slide13(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx);
  slideTitle(ctx, slide, "五年实现路径：先产品内核，再行业包，最后形成多模态传感器仿真平台", "路径不局限于现有资源，建议同步整合集团任务仿真、VR、UE5、雷达成像能力");

  const years = [
    ["2026\nV1.0", "红外内核产品化\nGUI 原型 / SDK 初版\n国产 ARM 适配验证\n2 家试点客户定义"],
    ["2027\nV1.5", "军工半实物方案\n低空无人机场景包\nAFSIM/VR 接口联调\n软著/专利布局"],
    ["2028\nV2.0", "UE5 高保真分支\n商业航天载荷仿真\n材料/目标/大气库商品化\n批量授权交付"],
    ["2029\nV2.5", "红外 + 雷达融合评估\nAI 数据集生成服务\n跨平台 Linux/ARM 工程化\n渠道复制"],
    ["2030\nV3.0", "多模态传感器仿真平台\n行业标准/生态伙伴\n集团级仿真底座\n规模化营收"],
  ];
  let x = 60;
  for (let i = 0; i < years.length; i += 1) {
    const [year, body] = years[i];
    rect(ctx, slide, x, 210, 150, 250, COLORS.white, i % 2 === 0 ? COLORS.blue2 : COLORS.green, 1.5);
    rect(ctx, slide, x, 210, 150, 60, i % 2 === 0 ? COLORS.blue2 : COLORS.green);
    addText(ctx, slide, year, x + 12, 221, 126, 40, { size: 17, bold: true, color: COLORS.white, align: "center" });
    addText(ctx, slide, body, x + 12, 292, 126, 132, { size: 10.8, color: COLORS.navy, align: "center" });
    if (i < years.length - 1) {
      rect(ctx, slide, x + 154, 326, 20, 4, COLORS.blue2);
    }
    x += 170;
  }

  rect(ctx, slide, 115, 520, 730, 52, "#EEF4FF", COLORS.blue2, 1);
  addText(ctx, slide, "阶段门建议：每年以“产品版本 + 可验收指标 + 标杆客户 + 知识产权/数据资产”四类成果验收。", 145, 537, 670, 20, {
    size: 13,
    bold: true,
    color: COLORS.blue,
    align: "center",
  });
  return slide;
}

export async function slide14(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx);
  slideTitle(ctx, slide, "投入产出测算：前两年以研发和试点为主，第三年形成可复制收入曲线", "以下为规划口径估算，需结合正式预算、客户线索和财务模型校核");

  const x = 48;
  const y = 160;
  const widths = [76, 110, 126, 126, 160, 190, 110];
  const heads = ["年份", "人员投入", "固定资产", "研发/交付投入", "收入/节支", "主要产出", "经营贡献"];
  let cx = x;
  for (let i = 0; i < heads.length; i += 1) {
    tableCell(ctx, slide, cx, y, widths[i], 34, heads[i], {
      fill: COLORS.blue,
      color: COLORS.white,
      bold: true,
      size: 9.8,
      align: "center",
      border: COLORS.blue,
    });
    cx += widths[i];
  }
  const rows = [
    ["2026", "5-6 人\n核心研发", "80 万\nARM/相机/工位", "260 万", "50 万\n内部节支", "V1.0 原型、红外内核、GUI/SDK 初版、2 家试点", "50 万"],
    ["2027", "7-8 人\n产品+交付", "50 万\n测试环境", "420 万", "300 万", "军工半实物方案、低空场景包、5 套授权/项目", "180 万"],
    ["2028", "9-10 人\n平台化", "40 万\n数据/存储", "520 万", "800 万", "UE5 分支、航天载荷方案、材料/目标库商品化", "520 万"],
    ["2029", "10-11 人\n多模态", "50 万\n融合测试", "650 万", "1500 万", "红外+雷达融合、AI 数据服务、20 套级交付", "975 万"],
    ["2030", "12 人\n产品线", "60 万\n规模化环境", "780 万", "2500 万", "多模态传感器仿真平台、集团级复用、生态伙伴", "1625 万"],
  ];
  let cy = y + 34;
  for (const row of rows) {
    cx = x;
    for (let i = 0; i < row.length; i += 1) {
      tableCell(ctx, slide, cx, cy, widths[i], 62, row[i], {
        fill: i === 0 ? "#EEF4FF" : COLORS.white,
        bold: i === 0 || i === 4 || i === 6,
        size: i === 5 ? 8.8 : 8.9,
        color: i === 6 ? COLORS.green : (i === 0 ? COLORS.blue : COLORS.navy),
        align: i === 0 || i === 4 || i === 6 ? "center" : "left",
      });
      cx += widths[i];
    }
    cy += 62;
  }

  rect(ctx, slide, 105, 565, 750, 42, COLORS.pale2, COLORS.blue2, 1);
  addText(ctx, slide, "测算假设：产品授权 + 行业解决方案 + 年度维护/模型库服务；第 3 年开始进入规模化复制，第 4 年累计经营贡献覆盖前期投入。", 124, 577, 710, 18, {
    size: 10.8,
    color: COLORS.blue,
    align: "center",
    bold: true,
  });
  return slide;
}

export async function slide15(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx);
  slideTitle(ctx, slide, "风险与近期决策：先锁定 V1.0 边界和试点客户，再推进集团资源协同", "产品方向要保持清晰：平台可复用，行业包可销售，AI/UE5/雷达融合分阶段进入");

  card(ctx, slide, 58, 162, 260, 265, "主要风险", "• ARM 端性能不达标，实时帧率不足\n• 红外精度缺实测数据校准\n• 开源协议、军工合规、客户保密要求\n• 客户把产品当项目定制，复用性被稀释\n• AFSIM/VR/UE5/雷达资源协同节奏不一致", COLORS.red);
  card(ctx, slide, 350, 162, 260, 265, "应对措施", "• LOD、GPU 并行、FPGA 接口预留\n• 建材料/目标/大气标定库，保留可解释日志\n• 形成开源合规与国产化适配清单\n• 核心平台与项目定制分层报价\n• 建跨部门路线图和联合验收机制", COLORS.green);
  card(ctx, slide, 638, 162, 260, 265, "近期动作", "• 明确 V1.0 必做/不做清单\n• 确定 2-3 家军工/低空试点客户\n• 完成 GUI、接口封装、跨平台适配计划\n• 启动软著/专利/标准化素材库\n• 设立 AI 场景生成与数据合成预研小组", COLORS.blue2);

  rect(ctx, slide, 95, 500, 770, 60, COLORS.blue, COLORS.blue, 1);
  addText(ctx, slide, "建议立项口径：以“国产化红外成像仿真产品平台”为主线，以“军工/低空/航天解决方案包”为首批商业化抓手。", 125, 516, 710, 24, {
    size: 14,
    bold: true,
    color: COLORS.white,
    align: "center",
  });
  return slide;
}

export async function slide16(presentation, ctx) {
  const slide = presentation.slides.add();
  await addBackground(slide, ctx, "thanks");
  addText(ctx, slide, "HwaSimIR 三维红外成像仿真产品规划", 238, 300, 485, 38, {
    size: 19,
    bold: true,
    color: COLORS.blue,
    align: "center",
  });
  addText(ctx, slide, "产品平台 + 行业解决方案 + AI 数据能力", 290, 350, 380, 28, {
    size: 14,
    color: COLORS.navy,
    align: "center",
  });
  return slide;
}
