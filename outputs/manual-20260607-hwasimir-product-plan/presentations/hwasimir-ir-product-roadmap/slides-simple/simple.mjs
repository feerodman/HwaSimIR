const ROOT = "D:/HwaSimIR/outputs/manual-20260607-hwasimir-product-plan/presentations/hwasimir-ir-product-roadmap";
const BG = {
  cover: `${ROOT}/template-inspect/source-slides/source-slide-01.png`,
  contents: `${ROOT}/template-inspect/source-slides/source-slide-02.png`,
  blank: `${ROOT}/template-inspect/source-slides/source-slide-04.png`,
  thanks: `${ROOT}/template-inspect/source-slides/source-slide-10.png`,
};

const C = {
  blue: "#124B98",
  blue2: "#4976D1",
  navy: "#09285C",
  green: "#4CB494",
  red: "#C43D32",
  orange: "#D9812B",
  gray: "#5F6B7A",
  pale: "#EEF4FF",
  white: "#FFFFFF",
  border: "#9DB8E7",
};
const FONT = "微软雅黑";

async function bg(slide, ctx, name = "blank") {
  await ctx.addImage(slide, { path: BG[name], x: 0, y: 0, w: 960, h: 720, fit: "fill", alt: "template" });
}

function text(ctx, slide, value, x, y, w, h, opt = {}) {
  return ctx.addText(slide, {
    text: value,
    x,
    y,
    w,
    h,
    fontSize: opt.size ?? 16,
    color: opt.color ?? C.navy,
    bold: opt.bold ?? false,
    typeface: FONT,
    align: opt.align ?? "left",
    valign: opt.valign ?? "top",
    fill: "#00000000",
    line: ctx.line("#00000000", 0),
    insets: opt.insets ?? { left: 0, right: 0, top: 0, bottom: 0 },
  });
}

function box(ctx, slide, x, y, w, h, fill, stroke = "#00000000", sw = 0) {
  return ctx.addShape(slide, {
    x,
    y,
    w,
    h,
    fill,
    line: ctx.line(stroke, sw),
  });
}

function title(ctx, slide, value, sub = "") {
  text(ctx, slide, value, 95, 74, 760, 28, { size: 21, bold: true, color: C.white });
  if (sub) text(ctx, slide, sub, 96, 106, 720, 18, { size: 10.5, color: C.white });
}

function bigCard(ctx, slide, x, y, w, h, head, body, color) {
  box(ctx, slide, x, y, w, h, C.white, color, 1.5);
  box(ctx, slide, x, y, w, 34, color);
  text(ctx, slide, head, x + 14, y + 8, w - 28, 20, { size: 14, bold: true, color: C.white, align: "center" });
  text(ctx, slide, body, x + 18, y + 54, w - 36, h - 64, { size: 14, color: C.navy, align: "center" });
}

function mini(ctx, slide, x, y, w, h, head, body, color = C.blue2) {
  box(ctx, slide, x, y, w, h, "#F8FBFF", color, 1);
  text(ctx, slide, head, x + 10, y + 10, w - 20, 22, { size: 13, bold: true, color, align: "center" });
  text(ctx, slide, body, x + 12, y + 42, w - 24, h - 50, { size: 11.2, color: C.navy, align: "center" });
}

function pill(ctx, slide, x, y, w, label, color) {
  box(ctx, slide, x, y, w, 34, color, color, 1);
  text(ctx, slide, label, x + 8, y + 8, w - 16, 18, { size: 12.5, bold: true, color: C.white, align: "center" });
}

export async function slide01(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx, "cover");
  box(ctx, slide, 60, 126, 840, 128, C.white, C.border, 1);
  text(ctx, slide, "三维场景红外成像仿真产品规划", 88, 150, 784, 42, {
    size: 32,
    bold: true,
    color: C.blue,
    align: "center",
  });
  text(ctx, slide, "国产化 ARM 板卡 + 开源 3D 引擎 + 行业解决方案", 124, 205, 712, 26, {
    size: 16,
    color: C.navy,
    align: "center",
  });
  box(ctx, slide, 170, 480, 620, 46, C.white, C.blue2, 1);
  text(ctx, slide, "方向判断：产品化平台为主，军工/低空/航天解决方案为抓手", 190, 493, 580, 20, {
    size: 14,
    bold: true,
    color: C.blue,
    align: "center",
  });
  return slide;
}

export async function slide02(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx, "contents");
  text(ctx, slide, "产品定位、市场方向与目标用户", 470, 254, 330, 28, { size: 18, bold: true, color: C.blue });
  text(ctx, slide, "技术基础、创新难点与集团协同", 470, 379, 330, 28, { size: 18, bold: true, color: C.blue });
  text(ctx, slide, "实现路径、投入产出与 AI 化演进", 470, 505, 330, 28, { size: 18, bold: true, color: C.blue });
  return slide;
}

export async function slide03(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx);
  title(ctx, slide, "01 产品定位：平台产品 + 行业方案包", "平台沉淀能力，方案承接场景和预算");
  bigCard(ctx, slide, 76, 190, 238, 250, "平台产品", "红外仿真内核\nGUI 配置\nSDK/API 封装\n国产 ARM 运行时\n材料/目标/场景库", C.blue2);
  bigCard(ctx, slide, 361, 190, 238, 250, "行业方案包", "军工半实物测试\n低空无人机感知\n商业航天载荷验证\n安防应急演训", C.green);
  bigCard(ctx, slide, 646, 190, 238, 250, "长期方向", "红外 + 雷达 + 可见光\nAFSIM/VR/UE5 联合仿真\nAI 数据生成与评估", C.orange);
  box(ctx, slide, 145, 520, 670, 48, C.blue, C.blue, 1);
  text(ctx, slide, "一句话定位：面向低空、航天、军工的国产化传感器仿真底座。", 170, 534, 620, 20, {
    size: 14,
    bold: true,
    color: C.white,
    align: "center",
  });
  return slide;
}

export async function productExpansion(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx);
  title(ctx, slide, "02 产品概述：从红外仿真内核到可交付产品", "产品要能配置、能接入、能部署、能验收，而不是只停留在 demo");
  bigCard(ctx, slide, 62, 175, 250, 250, "产品是什么", "三维场景红外成像仿真平台\n\n输入：场景、目标、材质、温度、大气、传感器参数\n输出：多波段红外图像/序列、状态日志、测试报告", C.blue2);
  bigCard(ctx, slide, 355, 175, 250, 250, "产品细化", "GUI：场景与参数配置\n接口：SDK/API/协议封装\n模型库：目标/材料/大气/温度\n工具链：批量试验、日志、报告\n适配：Windows/Linux/国产 ARM", C.green);
  bigCard(ctx, slide, 648, 175, 250, 250, "扩展方向", "UE5 高保真场景\nAFSIM/VR 联合仿真\n红外 + 雷达多模态\nAI 合成数据与自动评测\n半实物/硬件在环接口", C.orange);
  box(ctx, slide, 130, 505, 700, 58, C.pale, C.blue2, 1);
  text(ctx, slide, "建议边界：V1.0 先做红外仿真平台最小可交付闭环；UE5、雷达、AI 能力分阶段进入，不抢占首版交付节奏。", 160, 522, 640, 22, {
    size: 13.5,
    bold: true,
    color: C.blue,
    align: "center",
  });
  return slide;
}

export async function slide04(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx);
  title(ctx, slide, "03 市场方向：军工先行，低空与航天扩展", "核心痛点是实测成本、场景复现、国产化替代和算法数据");
  mini(ctx, slide, 70, 180, 190, 180, "军工装备", "军工研究所\n总体单位\n红外设备厂商\n靶场/试验基地", C.red);
  mini(ctx, slide, 285, 180, 190, 180, "低空经济", "低空监管平台\n无人机运营商\n机场/园区/港口\n反无人机集成商", C.blue2);
  mini(ctx, slide, 500, 180, 190, 180, "商业航天", "卫星总体单位\n载荷研制单位\n商业航天公司\n地面仿真团队", C.green);
  mini(ctx, slide, 715, 180, 175, 180, "外延用户", "安防应急\n高校科研\n工业检测\n内部测试部门", C.orange);
  box(ctx, slide, 120, 440, 720, 74, C.pale, C.blue2, 1);
  text(ctx, slide, "优先级建议：军工试点客户定义需求和验收标准；低空经济形成场景包；商业航天做载荷/星地仿真接口储备。", 150, 463, 660, 26, {
    size: 14,
    bold: true,
    color: C.blue,
    align: "center",
  });
  return slide;
}

export async function slide05(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx);
  title(ctx, slide, "04 战略契合：低空、航天、军工、自主可控", "公司已有仿真资源可形成组合拳");
  pill(ctx, slide, 108, 190, 170, "低空经济", C.blue2);
  pill(ctx, slide, 292, 190, 170, "商业航天", C.green);
  pill(ctx, slide, 476, 190, 170, "军工装备", C.red);
  pill(ctx, slide, 660, 190, 170, "自主可控", C.orange);
  box(ctx, slide, 95, 290, 770, 160, C.white, C.border, 1);
  text(ctx, slide, "集团资源协同方向", 125, 315, 260, 24, { size: 19, bold: true, color: C.blue });
  text(ctx, slide, "AFSIM：任务/战术场景与联合仿真入口", 140, 365, 330, 22, { size: 13.5, color: C.navy });
  text(ctx, slide, "VR：沉浸式训练、装备操作、复盘评估", 140, 404, 330, 22, { size: 13.5, color: C.navy });
  text(ctx, slide, "虚幻5：下一代高保真场景与大规模渲染路线", 515, 365, 330, 22, { size: 13.5, color: C.navy });
  text(ctx, slide, "雷达成像：与红外形成多模态传感器仿真矩阵", 515, 404, 330, 22, { size: 13.5, color: C.navy });
  box(ctx, slide, 170, 525, 620, 42, C.blue, C.blue, 1);
  text(ctx, slide, "方向：把红外仿真嵌入公司“任务仿真 + 视景 + 传感器”的平台体系。", 190, 537, 580, 18, {
    size: 13,
    bold: true,
    color: C.white,
    align: "center",
  });
  return slide;
}

export async function slide06(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx);
  title(ctx, slide, "05 技术基础：已有 HwaSimIR 工程积累", "壁垒来自红外物理、三维引擎、国产硬件、协议和验证体系");
  mini(ctx, slide, 70, 180, 190, 190, "工程底座", "C++/Qt/Panda3D\nOpenCV/Eigen\n场景加载与图像处理", C.blue2);
  mini(ctx, slide, 285, 180, 190, 190, "红外链路", "多波段仿真\n温度/材质/大气\nMODTRAN tau 受控链路", C.green);
  mini(ctx, slide, 500, 180, 190, 190, "目标状态", "热点/亮点分离\n目标全协议键映射\n日志可追溯", C.orange);
  mini(ctx, slide, 715, 180, 175, 190, "验证体系", "构建脚本\n严格检查\n可视化 smoke 测试", C.red);
  box(ctx, slide, 130, 455, 700, 72, C.pale, C.blue2, 1);
  text(ctx, slide, "产品化重点：GUI、接口封装、跨平台适配、模型库、校准数据、交付文档和验收指标。", 160, 480, 640, 22, {
    size: 14,
    bold: true,
    color: C.blue,
    align: "center",
  });
  return slide;
}

export async function slide07(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx);
  title(ctx, slide, "07 对标与差异化：国产化、低成本、可集成", "不复制国外大平台，优先做公司可控、客户可买的能力");
  mini(ctx, slide, 66, 175, 158, 200, "Presagis/CAE", "视景仿真平台\n学习工具链和生态\n差异：国产可控", C.blue2);
  mini(ctx, slide, 239, 175, 158, 200, "ThermoAnalytics", "热/红外签名\n学习物理建模\n差异：本地交付", C.green);
  mini(ctx, slide, 412, 175, 158, 200, "OKTAL-SE", "EO/IR 传感器仿真\n学习任务场景\n差异：低空/航天场景包", C.orange);
  mini(ctx, slide, 585, 175, 158, 200, "MAK", "DIS/HLA 联合仿真\n学习系统集成\n差异：接 AFSIM/VR", C.red);
  mini(ctx, slide, 758, 175, 138, 200, "Ansys/AGI", "航天任务分析\n学习载荷链路\n差异：红外成像补位", C.blue);
  box(ctx, slide, 135, 465, 690, 58, C.blue, C.blue, 1);
  text(ctx, slide, "差异化主线：国外软件替代不是唯一目标，更重要的是国产化部署、低成本定制和与公司仿真体系融合。", 165, 482, 630, 22, {
    size: 13.5,
    bold: true,
    color: C.white,
    align: "center",
  });
  return slide;
}

export async function slide08(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx);
  title(ctx, slide, "08 实现路径：三阶段推进", "先落产品，再扩行业，最后平台化");
  const stages = [
    ["阶段一\n2026-2027", "V1.0/V1.5\n红外内核产品化\nGUI + SDK/API\nARM 适配\n军工试点"],
    ["阶段二\n2028", "V2.0\n低空与航天方案包\nAFSIM/VR 接口\nUE5 高保真分支\n模型库商品化"],
    ["阶段三\n2029-2030", "V3.0\n红外 + 雷达融合\nAI 数据生成服务\n多模态传感器仿真平台\n规模化复制"],
  ];
  let x = 96;
  for (const [head, body] of stages) {
    box(ctx, slide, x, 200, 220, 255, C.white, C.blue2, 1.5);
    box(ctx, slide, x, 200, 220, 66, C.blue2, C.blue2, 1);
    text(ctx, slide, head, x + 20, 213, 180, 42, { size: 18, bold: true, color: C.white, align: "center" });
    text(ctx, slide, body, x + 25, 300, 170, 120, { size: 13, color: C.navy, align: "center" });
    x += 275;
  }
  box(ctx, slide, 170, 522, 620, 42, C.pale, C.blue2, 1);
  text(ctx, slide, "每阶段验收：产品版本、标杆客户、知识产权、可复用数据/模型资产。", 195, 535, 570, 18, {
    size: 13,
    bold: true,
    color: C.blue,
    align: "center",
  });
  return slide;
}

export async function slide09(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx);
  title(ctx, slide, "09 投入产出：小团队启动，用项目带动产品迭代", "方向性测算，正式立项需财务、销售线索和客户价格校核");
  bigCard(ctx, slide, 62, 180, 245, 255, "投入建议", "人员：首年 5-6 人\n三年 8-10 人\n五年 10-12 人\n\n资产：ARM 板卡、红外相机、测试工位、数据/模型库、校准环境", C.blue2);
  bigCard(ctx, slide, 357, 180, 245, 255, "3 年产出", "第 1 年：内部节支 + 原型验证\n第 2 年：试点销售 + 军工方案\n第 3 年：低空/航天方案复制\n\n目标：形成软著、专利、模型库和标杆案例", C.green);
  bigCard(ctx, slide, 652, 180, 245, 255, "5 年效益", "产品授权\n行业解决方案\n模型库/维护服务\nAI 数据生成服务\n集团平台复用\n\n第 4-5 年追求规模化收入", C.orange);
  box(ctx, slide, 120, 515, 720, 54, C.blue, C.blue, 1);
  text(ctx, slide, "经营逻辑：先用军工/低空项目验证付费能力，再把 GUI、接口、模型库沉淀为可复制产品。", 150, 532, 660, 20, {
    size: 13.2,
    bold: true,
    color: C.white,
    align: "center",
  });
  return slide;
}

export async function innovationDifficulty(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx);
  title(ctx, slide, "06 创新点与难点：自主可控 + 物理可信 + 工程可交付", "真正的壁垒不是画面效果，而是可解释、可校准、可集成、可复用");
  bigCard(ctx, slide, 62, 175, 250, 250, "创新点", "国产 ARM 板卡适配\n开源 3D 引擎红外链路\n多波段/温度/大气/热点亮点\n协议化目标与武器状态联动\n可回归验证的工程流程", C.blue2);
  bigCard(ctx, slide, 355, 175, 250, 250, "难点", "实时帧率与 ARM GPU 优化\n红外物理精度和实测校准\n材料/目标/大气参数库建设\n场景规模与可解释性平衡\n军工交付环境适配", C.orange);
  bigCard(ctx, slide, 648, 175, 250, 250, "壁垒形成", "模型库和数据资产\n客户验收指标体系\n软著/专利/技术秘密\n跨平台运行时\nAFSIM/VR/UE5/雷达接口生态", C.green);
  box(ctx, slide, 130, 505, 700, 58, C.pale, C.blue2, 1);
  text(ctx, slide, "建议投入重点：不要只追求“画面更像”，要同步建设物理模型、数据校准、接口标准和自动化验收。", 160, 522, 640, 22, {
    size: 13.5,
    bold: true,
    color: C.blue,
    align: "center",
  });
  return slide;
}

export async function aiDirection(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx);
  title(ctx, slide, "10 AI 与下一步：把仿真平台升级为数据与评估平台", "AI 不是口号，落点是场景生成、参数校准、数据合成和自动评测");
  mini(ctx, slide, 70, 175, 190, 205, "AI 场景生成", "根据任务描述生成低空/航天/安防场景\n快速形成批量试验工况", C.blue2);
  mini(ctx, slide, 285, 175, 190, 205, "参数反演校准", "用实测红外图像反推材质、温度、大气和传感器参数\n提升可信度", C.green);
  mini(ctx, slide, 500, 175, 190, 205, "数据合成服务", "生成红外目标识别、跟踪、融合感知训练数据\n服务算法团队", C.orange);
  mini(ctx, slide, 715, 175, 175, 205, "自动评测", "批量生成测试用例\n日志诊断\n自动报告和验收指标", C.red);
  box(ctx, slide, 98, 450, 765, 90, C.white, C.border, 1);
  text(ctx, slide, "近期建议", 125, 470, 120, 24, { size: 18, bold: true, color: C.blue });
  text(ctx, slide, "1. 明确 V1.0 必做/不做清单；2. 锁定 2-3 家试点客户；3. 建立跨部门资源池；4. 启动模型库、校准数据和 AI 数据生成预研。", 245, 468, 585, 35, {
    size: 12.8,
    color: C.navy,
  });
  return slide;
}

export async function slide10(presentation, ctx) {
  const slide = presentation.slides.add();
  await bg(slide, ctx, "thanks");
  box(ctx, slide, 160, 265, 640, 118, C.white, C.border, 1);
  text(ctx, slide, "建议立项口径", 315, 286, 330, 30, {
    size: 24,
    bold: true,
    color: C.blue,
    align: "center",
  });
  text(ctx, slide, "国产化三维红外成像仿真产品平台\n以军工、低空经济、商业航天解决方案包为首批商业化方向", 190, 330, 580, 42, {
    size: 15,
    color: C.navy,
    align: "center",
  });
  return slide;
}
