# 水土保持遥感动态监测系统 C++ 后端 (SoilWaterCM-CPP)

> 面向在建矿山的多源遥感水土保持动态监测平台 — C++ 后端服务
> 基于 [Drogon](https://github.com/drogonframework/drogon) 框架, 功能对齐 [E:\WorkSpace\SoilWaterCM](https://github.com/) 的 Python FastAPI 版
> 覆盖项目/期次管理、影像接入、预处理、信息提取、综合评价、异常预警、报告生成、InSAR 形变、三维图层全流程

---

## 目录
- [1. 项目简介](#1-项目简介)
- [2. 系统架构](#2-系统架构)
- [3. 技术栈](#3-技术栈)
- [4. 目录结构](#4-目录结构)
- [5. 快速启动](#5-快速启动)
- [6. 首次使用流程](#6-首次使用流程)
- [7. API 概览](#7-api-概览)
- [8. 关键模块说明](#8-关键模块说明)
- [9. 数据库设计](#9-数据库设计)
- [10. 测试](#10-测试)
- [11. 已知限制](#11-已知限制)
- [12. 相关文档](#12-相关文档)

---

## 1. 项目简介

对于在建矿山, 施工过程中存在大范围土地扰动、弃渣堆置、边坡开挖等行为, 水土流失风险突出。传统水保监测以人工巡查为主, 周期长、覆盖面有限, 难以满足动态监管要求。

本仓库是监测平台的 **C++ 后端实现**, 与 Python 版后端 (FastAPI) 保持业务对齐: 相同的 REST API 形态、相同的数据模型, 面向对服务性能和部署体积有更高要求的场景。

### 核心能力

| 能力 | 说明 | C++ 实现状态 |
|------|------|------------|
| 项目与期次管理 | 项目 CRUD、防治责任范围边界、监测期生命周期、成果归档 (manifest) | 完整 |
| 影像管理 | 上传 (单文件/分块/目录/路径登记)、质量检查、下载删除 | 完整 |
| 数据采集 | 无人机航线规划 (GSD/航高/重叠 → 航带航点, 纯几何); 卫星检索采集 | 航线规划完整; 卫星客户端为桩 |
| 预处理 | 辐射校正、几何校正、正射纠正 (RPC+DEM)、镶嵌、重投影 | 走 GDAL/OTB 真实处理 |
| 信息提取 | FVC 反演、USLE 土壤侵蚀、双时相扰动提取、措施、弃渣场、分类/分割 | 核心算法真实; AI 训练/推理为任务占位 |
| InSAR | D-InSAR/SBAS 流程: 干涉图 → 相位滤波 → SNAPHU 解缠 → 形变计算; 时序 MintPy | 算法链完整, 依赖外部引擎 |
| 防治分区 | 分区边界上传 (GeoJSON/矢量文件)、扰动/措施空间套合、分区设计指标 | 完整 (PostGIS 空间 SQL) |
| 综合评价 | 指标体系、AHP/熵权法权重、合规分 + 风险分、结果持久化 | 完整 |
| 异常预警 | 违规扰动/形变等异常检测、派单、处置闭环 | 完整 |
| 地面验证 | 实测点录入、遥感值对比、相对误差精度验证 | 完整 |
| 报告 | 报告生成、备案、审核意见、预览下载 | 完整 (文件渲染类除外) |
| 可视化支撑 | 看板汇总/趋势、动态变化聚合、三维 GeoJSON 图层、底图管理、QGIS WMS 发布 | 完整 (渲染类端点除外) |

## 2. 系统架构

```
┌────────────────────────────────────────────────────────────┐
│  controllers   HTTP 层: 路由注册、参数解析、JWT 上下文、响应组装 │
├────────────────────────────────────────────────────────────┤
│  services      业务层: 提取算法编排、评价计算、空间套合、归档    │
├────────────────────────────────────────────────────────────┤
│  dao           数据访问层: 每张表一个 DAO, SQL 全部集中在此     │
├────────────────────────────────────────────────────────────┤
│  repository    连接层: PostgreSQL 连接池 / Redis / MongoDB     │
└────────────────────────────────────────────────────────────┘
      │ models (实体/枚举)        │ infrastructure          │ middleware
      │ struct 对应表             │ ├── gdal/      GDAL 栅格/矢量/空间运算
      │ enum 对应状态             │ ├── insar/     干涉/滤波/解缠/形变
      │                          │ ├── external/  卫星/QGIS/ODM 客户端
      │                          │ ├── external_tools/ GRASS/SAGA/OTB/SNAP...
      │                          │ └── (根)       配置/文件/线程池/指标
      │                          │ (日志、CORS、限流、JWT、鉴权)
                          utils: 日志/JSON/GeoJSON/加密/时间/字符串
```

请求链路: `middleware (JWT/CORS/限流) → controller → service → dao → PostgreSQL/PostGIS`,
重计算任务由 service 提交到 `TaskService` + 线程池异步执行, 前端轮询 `/tasks/{id}` 取进度与结果。

## 3. 技术栈

| 方面 | 选型 | 说明 |
|------|------|------|
| 语言 | C++20 (MSVC /WSL-GCC) | 命名空间 `soilwatercm::*` |
| Web 框架 | Drogon | 异步 HTTP, 控制器宏注册路由 |
| 主数据库 | PostgreSQL 14+ + PostGIS 3 | 空间几何列 SRID 4326, `libpqxx` 访问 |
| 缓存 | Redis 7 (hiredis) | 任务取消标志、限流计数 |
| 元数据库 | MongoDB (mongocxx) | 可选 |
| 空间处理 | GDAL / OGR (OSGeo4W 或 QGIS 自带) | 栅格/矢量读写、空间运算 |
| 外部 GIS 工具 | GRASS、SAGA、OTB、SNAP、ISCE2、MintPy、PyAPS3、SNAPHU | 子进程调用, 可选安装 |
| 认证 | JWT (jwt-cpp) + bcrypt | 中间件统一鉴权 |
| 序列化 | nlohmann/json (新代码) / jsoncpp (老代码) | |
| 日志 | spdlog | |
| 依赖管理 | vcpkg (manifest 模式, 配置时自动安装) | |
| 构建 | CMake 3.20+ / Visual Studio 2022 | |

## 4. 目录结构

```
SoilWaterCM_CPP/
├── README.md                          本文件
├── BUILD.md                           构建环境详细说明 (vcpkg/QGIS/GDAL 配置)
├── CLAUDE.md                          协作规则 (注释/提交规范)
├── CMakeLists.txt                     根构建脚本 (GLOB 源文件 + VS 工程分组)
├── vcpkg.json                         vcpkg 依赖清单
├── .clang-format / .clang-tidy        格式与静态检查配置
├── docs/
│   ├── api.md                         接口详细定义
│   ├── architecture.md                架构说明
│   ├── deployment.md                  部署指南
│   └── user_manual.md                 使用手册
├── config/
│   ├── .env.template                  环境变量模板 (GIS 工具路径/数据库/存储)
│   ├── config.yaml                    应用配置 (上传目录/指标阈值等)
│   ├── drogon.json                    HTTP 服务配置 (监听端口/线程数)
│   ├── database.yaml                  数据库连接
│   └── logging.yaml                   日志配置
├── scripts/
│   ├── run.ps1 / run.bat              运行脚本 (自动设置 GDAL/PROJ 环境变量)
│   ├── run_tests.bat / run_tests.sh   测试脚本
│   ├── install_deps.sh                依赖安装 (Linux)
│   └── installer.nsi                  NSIS 安装包脚本
├── tests/                             GoogleTest + benchmark 骨架
│   └── CMakeLists.txt                 unit/integration/benchmark 三个目标
└── src/
    ├── main.cpp                       入口: 装配各层并注册控制器
    ├── common/                        异常体系 (业务异常基类 + 9 类派生)
    ├── controllers/                   22 个 HTTP 控制器 (auth/users/projects/...)
    ├── services/                      25 个业务服务
    ├── dao/                           18 个 DAO + DaoBase 模板基类
    ├── models/                        19 个实体文件 (struct 对应表, enum 对应状态)
    ├── repository/                    PostgreSQL 连接池 / Redis / MongoDB
    ├── middleware/                    日志、CORS、限流、JWT、鉴权
    ├── infrastructure/
    │   ├── gdal/                      GdalWrapper / RasterProcessor / VectorProcessor / SpatialOps
    │   ├── insar/                     干涉图生成 / 相位滤波 / SNAPHU 解缠 / 形变计算 / 复影像处理
    │   ├── external/                  卫星 / QGIS / ODM / InSAR 服务客户端
    │   ├── external_tools/            GRASS/SAGA/OTB/SNAP/ISCE2/MintPy/PyAPS3 进程封装
    │   └── (根)                       ConfigManager / FileManager / ThreadPool / MetricsCollector
    └── utils/                         日志、JSON、GeoJSON 转换 (WKT↔GeoJSON/球面面积)、加密、时间、字符串
```

## 5. 快速启动

### 5.1 前置条件

| 组件 | 要求 |
|------|------|
| 编译器 | Visual Studio 2022 (C++20) 或 GCC 11+ |
| CMake | 3.20+ |
| vcpkg | 需设置 `VCPKG_ROOT`; 首次配置时自动按 `vcpkg.json` 安装依赖 |
| PostgreSQL | 14+, 需安装 PostGIS 扩展 |
| Redis / MongoDB | 可选, 未启动时相关功能降级 |
| GIS 工具 | GDAL 必需 (OSGeo4W 或 QGIS 自带); GRASS/SAGA/OTB/SNAP/ISCE2 等可选 |

### 5.2 数据库初始化

```sql
CREATE DATABASE soilwatercm;
\c soilwatercm
CREATE EXTENSION postgis;
```

本仓库不含迁移脚本, 表结构需自行初始化 (可参照 Python 版 Alembic 生成的表, 详见 [9. 数据库设计](#9-数据库设计))。
`zone_stats`、`zone_boundaries`、`indicators`、`indicator_values`、`ground_truths`、`basemap_images`、`project_documents` 等新模块表需手动建。

### 5.3 配置

```powershell
copy config\.env.template config\.env
# 按需修改: 数据库连接、GIS 工具路径 (GRASS_HOME/SNAP_HOME/OTB_HOME...)、存储目录
```

### 5.4 构建与运行

详细的 vcpkg/QGIS/GDAL 环境配置见 [BUILD.md](BUILD.md)。Windows 快速路径:

```powershell
# 配置 + 编译
cmake -S . -B build
cmake --build build --config RelWithDebInfo

# 运行 (自动设置 GDAL/PROJ 环境变量)
.\scripts\run.ps1 RelWithDebInfo
```

服务默认监听 `8080` (可在 `config/drogon.json` 修改)。

> 提示: 本项目 Drogon + C++20 的编译单元较重, 全量重编时若内存不足 (C1060),
> 可设环境变量 `CL_MPCount=4` 限制 MSVC 并行度。

## 6. 首次使用流程

接口统一前缀 `/api/v1`, 除登录外均需 JWT (`Authorization: Bearer <token>`)。

```
① 登录           POST /auth/login                          拿 token
② 建项目         POST /projects                            编码/名称/防治责任范围/中心经纬度
③ 建监测期       POST /periods
④ 传影像         POST /images/upload                       → 触发质量检查
⑤ 预处理         POST /processing/preprocess/{id}          辐射/几何/正射 → 镶嵌
⑥ 信息提取       POST /extraction/fvc                      NDVI → 像元二分 FVC
                 POST /extraction/erosion                  USLE (需 DEM/降雨/土壤栅格)
                 POST /extraction/disturbances/auto-extract 双时相扰动图斑
⑦ 分区套合       POST /zone-boundaries/upload              上传分区边界
                 POST /assign-zones                        回填扰动/措施的所属分区
⑧ 综合评价       POST /evaluation/run                      AHP/熵权法权重 → 合规分+风险分
⑨ 报告           POST /reports/generate                    → 备案 → 下载
⑩ 异常处置       POST /anomalies/{id}/dispatch             派单 → 地面验证 → 处置
```

重操作 (预处理/提取/InSAR/评价/报告) 均为异步任务, 返回任务 id,
通过 `GET /tasks/{id}` 轮询进度, 支持取消/重试/强杀。

## 7. API 概览

| 资源 | 路径 | 说明 |
|---|---|---|
| 认证 | `/auth` | 登录、注册、当前用户 |
| 用户 | `/users` | 用户管理、改密 |
| 项目 | `/projects` | 项目 CRUD、边界上传/下载、项目资料文档 |
| 监测期 | `/periods` | 期次管理、归档 (manifest) |
| 影像 | `/images` | 上传 (含分块/目录/路径登记)、质量检查、下载 |
| 数据采集 | `/acquisition` | 卫星检索/采集、无人机航线规划 |
| 处理 | `/processing` | 预处理、镶嵌、三维、InSAR、地形、流域、淹没、视域等 |
| 信息提取 | `/extraction` | FVC、侵蚀、扰动、措施、弃渣场、分类/分割 |
| 综合评价 | `/evaluation` | 评价执行与结果查询 |
| 指标 | `/indicators` | 指标定义、权重 (AHP)、阈值、指标值 |
| 水保措施 | `/measures` | 措施详情、删除 |
| 异常 | `/anomalies` | 检测、派单、通知、处置 |
| 地面验证 | `/ground-truth` | 实测点、精度验证 |
| 防治分区 | `/zone-stats` `/zone-boundaries` `/assign-zones` | 分区统计、边界上传、空间套合 |
| 报告 | `/reports` | 生成、下载、审核意见、预览 |
| 任务 | `/tasks` | 异步任务查询、重试、取消、恢复 |
| 看板 | `/dashboard` | 汇总、趋势、多指标趋势、期指标 |
| 动态变化图 | `/dynamic` | 期图层、时间序列、对比、动画 |
| 三维图层 | `/scene-layers` | 扰动/措施/异常 GeoJSON 图层、可用性 |
| 底图 | `/basemaps` | 三维底图上传、列表、删除 |
| QGIS 发布 | `/qgis` | 发布、服务列表、WMS 地址 |
| 特殊时期监测 | `/monitoring` | 汛期排期、快速变化识别 |

常用端点示例 (完整定义见 [docs/api.md](docs/api.md)):

```
POST   /api/v1/auth/login                          登录
GET    /api/v1/projects/                           项目列表
POST   /api/v1/images/upload                       上传影像
POST   /api/v1/processing/preprocess/{id}          提交预处理
POST   /api/v1/extraction/fvc                      FVC 反演
POST   /api/v1/extraction/erosion                  USLE 侵蚀
POST   /api/v1/extraction/disturbances/auto-extract  扰动提取
POST   /api/v1/zone-boundaries/upload              上传分区边界 (GeoJSON/Shapefile/DXF)
POST   /api/v1/assign-zones                        空间套合 (回填 zone 字段)
GET    /api/v1/zone-stats                          分区统计
POST   /api/v1/evaluation/run                      综合评价
GET    /api/v1/tasks/{id}                          任务进度/结果
POST   /api/v1/processing/insar/{id}               InSAR 形变 (dinsar/sbas)
GET    /api/v1/scene-layers/disturbances           扰动图斑 GeoJSON (三维场景)
POST   /api/v1/qgis/publish                        成果发布 WMS
GET    /api/v1/dashboard/summary                   看板汇总
```

## 8. 关键模块说明

### 8.1 FVC 植被覆盖度反演 (`services/fvc_service`)

像元二分法: NDVI → 端元估计 → 覆盖度。裸土/植被端元可显式指定 (默认 0.05/0.70),
也可按 5%/95% 分位自动估计。输出 FVC 栅格、平均覆盖度与分级面积统计, 结果落 `vegetation_coverages` 表。

### 8.2 USLE 土壤侵蚀 (`services/erosion_service`)

```
A = R × K × LS × C × P
```

输入 DEM、降雨、土壤质地、FVC 四类栅格; LS 因子可选 GRASS (`r.watershed`) 或 SAGA 计算
(`ls_method=grass/saga`), 外部工具不可用时退 GDAL。输出侵蚀模数栅格 + 六级分类栅格与分级面积。

### 8.3 扰动图斑提取 (`services/disturbance_service`)

双时相 (before/after) 比较, 图斑按最小面积 (默认 0.1 公顷) 和置信度阈值 (默认 0.7) 过滤,
矢量化后落 `disturbances` 表。空间运算走 GDAL/OGR, 部分算法可调 OTB/GRASS。

### 8.4 InSAR 形变监测 (`services/insar_service` + `infrastructure/insar`)

- **算法链 (自研 C++)**: `InterferogramGenerator` (相位/幅度/相干性) → `PhaseFilter`
  (Goldstein) → `SnaphuWrapper` (SNAPHU 解缠) → `DeformationCalc` (相位 → 毫米位移)
- **处理流程**: `process_type=dinsar` (单对) / `sbas` (时序)
- **外部引擎**: ISCE2 / SNAP 经 `external_tools` 接口探测调用; 时序走 MintPy conda 环境,
  大气校正走 PyAPS3, 解缠依赖 SNAPHU (`SNAPHU_PATH` 或内置库)

### 8.5 GDAL 封装 (`infrastructure/gdal`)

| 类 | 职责 |
|----|------|
| `GdalWrapper` | 数据集读写、元信息提取 |
| `RasterProcessor` | 重采样、裁剪、镶嵌、逐像元波段运算 (内存版部分为桩, 文件版走 GDAL 真实处理) |
| `VectorProcessor` | 矢量读写、格式转换 |
| `SpatialOps` | 缓冲区、相交等空间运算 |

### 8.6 外部 GIS 工具 (`infrastructure/external_tools`)

统一 `ExternalToolInterface` + `ProcessManager` (子进程执行、进度回调、超时/取消)。
GRASS / SAGA / OTB / SNAP / ISCE2 / MintPy / PyAPS3 各有独立接口类, `ExternalToolDetector`
按环境变量与常见安装路径探测。工具未安装时相关任务失败并给出明确原因, 不影响其他功能。

### 8.7 防治分区 (`services/zone_service` + `dao/zone_dao`)

- 分区边界: GeoJSON FeatureCollection 或矢量文件 (Shapefile/DXF) 解析结果, 同分区多边形合并为 MultiPolygon 入库 (SRID 4326)
- 空间套合: 扰动图斑/水保措施几何与分区边界做 PostGIS 空间判定 — 质心落入优先, 否则取相交面积最大的分区, 回填 `zone` 字段
- 分区代码解析: 标准代码或中文名 (如 `waste_dump`/`排土场区`) 均可识别
- 设计指标: 按分区代码建档初始化, 面积优先取入参, 其次取边界面积

### 8.8 综合评价 (`services/evaluation_service` + `services/indicator_service`)

指标加权求和得合规得分, 反推风险得分并分级; 权重支持 AHP 主观法
(`indicator_service` 幂迭代求判断矩阵特征向量) 或熵权客观法, 也可外部直接指定。
结果持久化到 `evaluations` 表, 重复评价覆盖旧快照。

### 8.9 异步任务 (`services/task_service` + `infrastructure/thread_pool`)

任务记录持久化于 `tasks` 表, 生命周期 Pending → Running → Succeeded/Failed,
支持重试、取消 (Redis 标志 + 检查点)、强杀、中断恢复、终态删除。
服务启动时扫描中断任务, 可自动恢复执行。

### 8.10 QGIS 发布 (`services/qgis_service` + `infrastructure/external/qgis_client`)

将成果图层注册到 QGIS Server 发布为 WMS; 支持按工程名精确查找, 找不到时回退模糊匹配,
返回 WMS 基地址与 GetCapabilities 地址, 供三维场景 (Cesium WebMapServiceImageryProvider) 叠加。

## 9. 数据库设计

19 个实体文件对应 20 张表 + PostGIS 空间扩展 (边界/图斑类表含 `geometry` 列, SRID 4326):

| 表 | 说明 | 空间字段 |
|----|------|---------|
| users | 系统用户 | - |
| projects | 监测项目 (编码/名称/状态/中心经纬度) | responsibility_area (防治责任范围) |
| monitoring_periods | 监测期 | - |
| project_documents | 项目资料附件 (方案/审批/设计/监理/验收) | - |
| images | 遥感/无人机影像 (传感器/采集日期/云量) | footprint |
| disturbances | 扰动地块 (类型/面积/置信度/合规/所属分区) | geom |
| vegetation_coverages | FVC 成果 (均值/分级面积) | region_geom |
| erosion_results | 侵蚀成果 (模数/分级面积) | region_geom |
| measures | 水保措施 (类别/实施率/所属分区) | geom |
| anomalies | 异常事件 (类型/严重度/状态/处置) | geom |
| indicators | 指标定义 (维度/权重/阈值) | - |
| indicator_values | 每期实测指标值 | - |
| ground_truths | 地面验证记录 (实测值 vs 遥感值/误差) | location |
| evaluations | 综合评价结果 (合规分/风险分/权重快照) | - |
| reports | 监测报告 (类型/文件/备案状态) | - |
| tasks | 后台处理任务 (类型/状态/进度/参数) | - |
| basemap_images | 三维场景底图 (CRS/范围/元数据) | footprint |
| zone_stats | 分区统计 (设计指标 vs 实际扰动/措施) | - |
| zone_boundaries | 分区边界 (代码/中文名/面积) | geom |

> 枚举列存 int (C++ `enum class`), 与 Python 版的字符串枚举 schema 不同, 不建议混用同一套表。

## 10. 测试

```powershell
cd build
ctest -C RelWithDebInfo --output-on-failure
```

`tests/CMakeLists.txt` 已配置三个测试目标 (GoogleTest 单元测试 / 集成测试 / benchmark 基准),
测试用例待补充, 欢迎按模块贡献。

## 11. 已知限制

与 Python 版对齐过程中, 以下能力为占位/桩实现, 端点可用但不产出真实结果:

- **AI 训练/推理类**: FLAIR 微调、变化检测模型、开放词汇提取、UNet 分割训练等为任务占位, 未移植 Python 运行时。
- **图像渲染类**: PNG/GIF/MP4 专题图、动态变化动画未移植, 相关端点返回数据或占位任务。
- **卫星客户端**: Sentinel/Landsat/CRESDA 检索下载为桩实现。
- **部分 InSAR / 地形分析**: GRID 差值、流域提取等算法为任务占位。
- **内存版栅格操作**: `RasterProcessor` 的 clip/mosaic/fillNodata 内存版为桩 (文件版接口真实处理)。
- **scene-layers 部分图层**: 弃渣场、InSAR 成果、淹没、视域图层因缺底层数据表返回空集合。

## 12. 相关文档

| 文档 | 内容 |
|------|------|
| [BUILD.md](BUILD.md) | 构建环境: vcpkg、QGIS/GDAL (OSGeo4W)、InSAR 工具链配置 |
| [docs/api.md](docs/api.md) | 全部接口的路径/参数/响应定义 |
| [docs/architecture.md](docs/architecture.md) | 架构设计说明 |
| [docs/deployment.md](docs/deployment.md) | 部署指南 |
| [docs/user_manual.md](docs/user_manual.md) | 使用手册 |
| [CLAUDE.md](CLAUDE.md) | 代码协作规则 (注释/提交规范) |
| `E:\WorkSpace\SoilWaterCM` | Python FastAPI 版后端 (功能参照与数据库表结构来源) |

实施依据: SL/T 277-2024《水土保持监测技术规范》
