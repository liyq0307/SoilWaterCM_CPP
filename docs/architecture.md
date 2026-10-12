# SoilWaterCM C++ 后端架构文档

## 1. 系统概述

SoilWaterCM C++ 后端是一个高性能的水土保持监测管理系统后端服务，采用现代 C++ (C++17) 实现，提供遥感数据处理、水土保持分析、异常预警等功能。

### 1.1 设计目标

- **高性能**：利用 C++ 的性能优势，提升遥感数据处理效率
- **跨平台**：支持 Windows 和 Linux 平台
- **可维护性**：采用分层架构，职责清晰，易于维护
- **可扩展性**：模块化设计，易于扩展新功能
- **安全性**：完善的认证授权机制，防止未授权访问

### 1.2 技术栈

| 技术领域 | 技术选型 | 版本要求 |
|---------|---------|---------|
| 编程语言 | C++ | C++17 |
| HTTP框架 | Drogon | ≥ 1.8.0 |
| JSON库 | nlohmann-json | ≥ 3.11.0 |
| 日志库 | spdlog | ≥ 1.11.0 |
| 数据库 | PostgreSQL + PostGIS | ≥ 14.0 |
| MongoDB | mongocxx | ≥ 3.8.0 |
| 缓存 | Redis (hiredis) | ≥ 1.2.0 |
| 空间数据 | GDAL | ≥ 3.8.0 |
| JWT | jwt-cpp | ≥ 0.6.0 |
| 测试框架 | Google Test | ≥ 1.12.0 |
| 构建工具 | CMake | ≥ 3.20.0 |
| 包管理 | vcpkg | Latest |

## 2. 系统架构

### 2.1 分层架构

系统采用经典的分层架构，从上到下分为四层：

```
┌─────────────────────────────────────────┐
│          表现层 (Presentation)           │
│  Controllers + Middleware + Routing     │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│           业务层 (Business)              │
│              Services                    │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│         数据访问层 (Data Access)          │
│               DAOs                       │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│         基础设施层 (Infrastructure)       │
│  Database + Cache + File + GDAL + Utils │
└─────────────────────────────────────────┘
```

### 2.2 目录结构

```
SoilWaterCM_CPP/
├── src/                      # 源代码
│   ├── main.cpp              # 主入口
│   ├── common/               # 公共定义
│   │   ├── constants.h       # 常量定义
│   │   ├── types.h           # 类型定义
│   │   └── exceptions.h      # 异常定义
│   ├── models/               # 数据模型
│   │   ├── common/           # 公共模型
│   │   │   ├── enums.h       # 枚举定义
│   │   │   └── geo_types.h   # 地理类型
│   │   ├── user.h            # 用户模型
│   │   ├── project.h         # 项目模型
│   │   └── ...               # 其他模型
│   ├── dao/                  # 数据访问层
│   │   ├── user_dao.h/cpp
│   │   ├── project_dao.h/cpp
│   │   └── ...
│   ├── services/             # 业务层
│   │   ├── auth_service.h/cpp
│   │   ├── project_service.h/cpp
│   │   └── ...
│   ├── controllers/          # 控制器
│   │   ├── auth_controller.h/cpp
│   │   ├── project_controller.h/cpp
│   │   └── ...
│   ├── middleware/           # 中间件
│   │   ├── jwt_middleware.h/cpp
│   │   ├── auth_middleware.h/cpp
│   │   └── ...
│   ├── infrastructure/       # 基础设施
│   │   ├── config_manager.h/cpp
│   │   ├── thread_pool.h/cpp
│   │   ├── file_manager.h/cpp
│   │   ├── metrics_collector.h/cpp
│   │   ├── gdal/             # GDAL封装
│   │   ├── external/         # 外部服务客户端
│   │   ├── external_tools/   # 外部工具接口
│   │   └── insar/            # InSAR算法
│   ├── repository/           # 数据库连接池
│   │   ├── postgres_repository.h/cpp
│   │   ├── mongo_repository.h/cpp
│   │   └── redis_repository.h/cpp
│   └── utils/                # 工具类
│       ├── logger.h/cpp
│       ├── crypto.h/cpp
│       ├── json_utils.h/cpp
│       ├── time_utils.h/cpp
│       └── string_utils.h/cpp
├── tests/                    # 测试代码
│   ├── unit/                 # 单元测试
│   ├── integration/          # 集成测试
│   └── benchmark/            # 性能测试
├── config/                   # 配置文件
│   ├── config.yaml           # 主配置
│   ├── database.yaml         # 数据库配置
│   └── logging.yaml          # 日志配置
├── scripts/                  # 脚本
│   ├── build.sh              # Linux构建脚本
│   ├── build.bat             # Windows构建脚本
│   ├── install_deps.sh       # 依赖安装脚本
│   ├── run_tests.sh          # 测试运行脚本
│   └── code_quality_check.sh # 代码质量检查
├── docs/                     # 文档
│   ├── architecture.md       # 架构文档
│   ├── api.md                # API文档
│   ├── deployment.md         # 部署文档
│   └── user_manual.md        # 用户手册
├── docker/                   # Docker配置
│   ├── Dockerfile
│   └── docker-compose.yml
├── cmake/                    # CMake模块
│   ├── CompilerOptions.cmake
│   ├── FindGDAL.cmake
│   └── FindPostgreSQL.cmake
├── CMakeLists.txt            # 主CMake配置
├── vcpkg.json                # vcpkg依赖清单
├── .clang-format             # 代码格式配置
└── .clang-tidy               # 静态分析配置
```

## 3. 核心模块

### 3.1 表现层 (Presentation Layer)

#### 3.1.1 控制器 (Controllers)

控制器负责处理 HTTP 请求，调用业务服务，返回响应。

**主要控制器**：

| 控制器 | 职责 | 主要接口 |
|--------|------|---------|
| AuthController | 用户认证 | /api/v1/auth/login, /api/v1/auth/logout |
| UserController | 用户管理 | /api/v1/users |
| ProjectController | 项目管理 | /api/v1/projects |
| ImageController | 影像管理 | /api/v1/images |
| ProcessingController | 数据处理 | /api/v1/processing |
| ExtractionController | 信息提取 | /api/v1/extraction |
| EvaluationController | 综合评价 | /api/v1/evaluation |
| AnomalyController | 异常预警 | /api/v1/anomalies |
| ReportController | 监测报告 | /api/v1/reports |
| TaskController | 任务管理 | /api/v1/tasks |
| DashboardController | 数据看板 | /api/v1/dashboard |
| QgisController | QGIS服务 | /api/v1/qgis |

#### 3.1.2 中间件 (Middleware)

中间件负责请求预处理和响应后处理。

**中间件链**：

```
Request → CorsMiddleware → RateLimitMiddleware → LoggingMiddleware 
       → JwtMiddleware → AuthMiddleware → Controller → Response
```

| 中间件 | 职责 |
|--------|------|
| CorsMiddleware | 跨域请求处理 |
| RateLimitMiddleware | 请求限流 |
| LoggingMiddleware | 请求日志记录 |
| JwtMiddleware | JWT Token 验证 |
| AuthMiddleware | 权限校验 |

### 3.2 业务层 (Business Layer)

业务层实现核心业务逻辑，调用数据访问层和基础设施层。

**主要服务**：

| 服务 | 职责 |
|------|------|
| AuthService | 用户认证、Token生成验证 |
| ProjectService | 项目管理、几何验证 |
| ImageService | 影像上传、质量检查 |
| PreprocessService | 遥感预处理（辐射校正、正射纠正、镶嵌） |
| FvcService | 植被覆盖度反演（NDVI、FVC计算） |
| ErosionService | 土壤侵蚀计算（USLE模型） |
| DisturbanceService | 扰动图斑提取（变化检测、分类） |
| MeasureService | 水保措施解译 |
| EvaluationService | 综合评价（AHP、熵权法） |
| AnomalyService | 异常识别、预警推送 |
| ReportService | 监测报告生成 |
| ThreeDService | 三维空间分析 |
| InsarService | InSAR处理（干涉图、相位解缠、形变计算） |
| TaskService | 异步任务管理 |
| DashboardService | 数据看板统计 |
| QgisService | QGIS服务发布 |

### 3.3 数据访问层 (Data Access Layer)

数据访问层封装数据库操作，提供 CRUD 接口。

**主要 DAO**：

| DAO | 职责 |
|-----|------|
| UserDao | 用户数据访问 |
| ProjectDao | 项目数据访问（含空间数据） |
| MonitoringPeriodDao | 监测期数据访问 |
| ImageDao | 影像数据访问 |
| DisturbanceDao | 扰动图斑数据访问 |
| VegetationCoverageDao | 植被覆盖度数据访问 |
| ErosionResultDao | 土壤侵蚀结果数据访问 |
| MeasureDao | 水保措施数据访问 |
| AnomalyDao | 异常报警数据访问 |
| EvaluationDao | 评价结果数据访问 |
| ReportDao | 监测报告数据访问 |
| TaskDao | 处理任务数据访问 |

### 3.4 基础设施层 (Infrastructure Layer)

基础设施层提供底层服务支撑。

#### 3.4.1 数据库连接池

- **PostgresConnectionPool**：PostgreSQL 连接池，RAII 自动管理连接
- **MongoConnectionPool**：MongoDB 连接池
- **RedisRepository**：Redis 客户端封装

#### 3.4.2 线程池

- **ThreadPool**：异步任务执行，支持任务提交、优雅关闭

#### 3.4.3 文件管理

- **FileManager**：跨平台文件读写，支持流式读写

#### 3.4.4 GDAL 封装

- **GdalWrapper**：栅格数据读写，RAII 管理 GDALDataset
- **RasterProcessor**：栅格处理（重采样、裁剪、镶嵌、计算）
- **VectorProcessor**：矢量处理（读写、几何操作）
- **SpatialOps**：空间操作（查询、距离、面积）

#### 3.4.5 外部工具接口

- **GrassInterface**：GRASS GIS 接口（地形分析、水文分析）
- **SagaInterface**：SAGA GIS 接口（K因子计算）
- **OtbInterface**：OTB 接口（辐射校正、正射纠正、分类）
- **InsarInterface**：InSAR 工具统一接口（ISCE2/SNAP自动选择）
- **Isce2Interface**：ISCE2 接口（精密配准、干涉图生成）
- **SnapInterface**：SNAP 接口（干涉处理）
- **SnaphuWrapper**：SNAPHU 静态库封装（相位解缠）
- **MintPyInterface**：MintPy 接口（时序InSAR分析）
- **Pyaps3Interface**：pyaps3 接口（大气校正）

#### 3.4.6 InSAR 自研算法

- **InterferogramGenerator**：干涉图生成算法
- **PhaseFilter**：相位滤波算法（Goldstein自适应滤波）
- **DeformationCalculator**：形变计算算法
- **ComplexImageProcessor**：复数影像处理工具类

#### 3.4.7 配置管理

- **ConfigManager**：YAML 配置加载、环境变量覆盖、热更新

#### 3.4.8 日志系统

- **Logger**：封装 spdlog，支持结构化 JSON 日志、分级、滚动

#### 3.4.9 指标收集

- **MetricsCollector**：Prometheus 指标收集（请求计数、响应时间）

## 4. 核心流程

### 4.1 用户认证流程

```
1. 用户提交用户名密码 → AuthController::login
2. AuthService::authenticate 验证用户名密码
3. AuthService::generateToken 生成 JWT Token
4. 返回 Token 给客户端
5. 后续请求携带 Token → JwtMiddleware 验证
6. AuthMiddleware 校验权限
```

### 4.2 项目创建流程

```
1. 用户提交项目数据 → ProjectController::createProject
2. ProjectService::createProject
   - 验证项目编码唯一性
   - 验证几何数据有效性（GDAL）
   - 插入数据库（PostGIS）
3. 返回项目 ID
```

### 4.3 影像处理流程

```
1. 用户上传影像 → ImageController::uploadImage
2. ImageService::uploadImage
   - 保存文件到存储
   - 提取元数据（GDAL）
   - 入库
   - 触发质量检查（异步）
3. 用户提交预处理任务 → ProcessingController::preprocess
4. PreprocessService::executePreprocess
   - 辐射校正（OTB优先）
   - 几何校正
   - 正射纠正（OTB优先）
   - 镶嵌
5. 返回处理结果
```

### 4.4 FVC 反演流程

```
1. 用户提交 FVC 任务 → ExtractionController::fvc
2. FvcService::executeFvc
   - 读取影像（GDAL）
   - 计算 NDVI
   - 计算 FVC（NDVI二分法）
   - 分级统计
   - 保存结果（GeoTIFF）
3. 返回结果路径
```

### 4.5 USLE 侵蚀计算流程

```
1. 用户提交侵蚀计算任务 → ExtractionController::erosion
2. ErosionService::executeErosion
   - 计算 R 因子
   - 计算 K 因子（SAGA优先）
   - 计算 L、S 因子（GRASS优先，SAGA备选）
   - 计算 C 因子（基于FVC）
   - 计算 P 因子
   - 计算 USLE: A = R*K*L*S*C*P
   - 侵蚀分级
   - 保存结果
3. 返回结果路径
```

### 4.6 InSAR 处理流程

```
1. 用户提交 InSAR 任务 → ExtractionController::insar
2. InsarService 执行流程
   - 干涉图生成（自研 InterferogramGenerator）
   - 相位滤波（自研 PhaseFilter）
   - 相位解缠（SnaphuWrapper 静态库）
   - 形变计算（自研 DeformationCalculator）
   - 时序分析（MintPy 进程调用）
   - 大气校正（pyaps3）
3. 返回结果路径
```

### 4.7 异步任务处理流程

```
1. 用户提交任务 → TaskController::createTask
2. TaskService::createTask
   - 创建任务记录
   - 提交到线程池
   - 返回任务 ID
3. 线程池执行任务
   - 更新任务状态（运行中）
   - 执行业务逻辑
   - 更新任务状态（完成/失败）
4. 用户查询任务状态 → TaskController::getTaskById
```

## 5. 数据模型

### 5.1 核心实体

| 实体 | 说明 | 主要字段 |
|------|------|---------|
| User | 用户 | id, username, password_hash, role, is_active |
| Project | 项目 | id, code, name, responsibility_area, status |
| MonitoringPeriod | 监测期 | id, period_code, type, start_date, end_date |
| Image | 影像 | id, image_code, type, file_path, quality_status |
| Disturbance | 扰动图斑 | id, type, geometry, area, confidence |
| VegetationCoverage | 植被覆盖度 | id, fvc_raster_path, mean_fvc |
| ErosionResult | 土壤侵蚀结果 | id, modulus_raster_path, mean_modulus |
| Measure | 水保措施 | id, type, geometry, length, area |
| Anomaly | 异常报警 | id, type, geometry, severity, status |
| Evaluation | 评价结果 | id, compliance_score, risk_score |
| Report | 监测报告 | id, type, title, docx_path, pdf_path |
| Task | 处理任务 | id, type, status, progress, result_path |

### 5.2 空间数据

系统使用 PostGIS 存储空间数据：

- **Geometry**：使用 WKT 格式，支持 Point、Polygon、MultiPolygon
- **SRID**：统一使用 EPSG:4326 (WGS84)
- **空间索引**：对空间字段创建 GIST 索引

## 6. 性能优化

### 6.1 数据库优化

- **连接池**：复用数据库连接，避免频繁创建销毁
- **索引**：对常用查询字段创建索引
- **空间索引**：对空间字段创建 GIST 索引
- **批量操作**：批量插入、更新，减少数据库交互

### 6.2 并发优化

- **线程池**：异步任务使用线程池，避免频繁创建线程
- **无锁设计**：使用 std::atomic 实现无锁计数器
- **移动语义**：大对象使用移动语义传递，避免拷贝

### 6.3 内存优化

- **RAII**：所有资源使用 RAII 管理，自动释放
- **智能指针**：使用 unique_ptr、shared_ptr 管理内存
- **流式读写**：大文件使用流式读写，避免一次性加载

### 6.4 算法优化

- **SIMD**：使用 AVX2/SSE 指令优化栅格计算
- **多线程并行**：栅格处理、InSAR处理使用多线程并行
- **缓存**：Redis 缓存热点数据

## 7. 安全设计

### 7.1 认证授权

- **JWT Token**：使用 JWT 进行身份认证
- **角色权限**：基于角色的权限控制（Admin、Monitor、Leader）
- **密码加密**：使用 bcrypt 加密存储密码

### 7.2 数据安全

- **输入验证**：对所有输入进行验证，防止 SQL 注入、XSS
- **参数化查询**：数据库操作使用参数化查询
- **文件验证**：上传文件验证类型、大小

### 7.3 通信安全

- **HTTPS**：生产环境使用 HTTPS
- **CORS**：配置跨域访问白名单
- **限流**：防止 DDoS 攻击

## 8. 可观测性

### 8.1 日志

- **结构化日志**：JSON 格式，包含 Trace ID
- **日志分级**：DEBUG、INFO、WARNING、ERROR、CRITICAL
- **日志滚动**：按日滚动，保留历史日志

### 8.2 指标

- **Prometheus 指标**：请求计数、响应时间、任务计数
- **指标导出**：/metrics 端点导出指标

### 8.3 健康检查

- **健康检查端点**：/health 检查服务状态
- **数据库健康检查**：检查数据库连接

## 9. 外部工具集成

### 9.1 工具调用优先级

| 功能 | 优先级 |
|------|--------|
| 辐射校正 | OTB > GDAL |
| 正射纠正 | OTB > GDAL |
| 影像分类 | OTB > GDAL |
| LS因子计算 | GRASS > SAGA > GDAL |
| K因子计算 | SAGA > GDAL |
| InSAR工具 | ISCE2 > SNAP |
| 相位解缠 | SNAPHU静态库（无进程调用） |

### 9.2 工具检测

系统启动时自动检测外部工具：

- 检测工具路径
- 验证版本号
- 记录可用性到日志
- 不可用时自动降级

### 9.3 InSAR 混合架构

- **C++ 自研算法**：干涉图生成、相位滤波、形变计算（性能优化）
- **SNAPHU 静态库**：直接链接 C 源码，无进程调用开销（性能提升 30%+）
- **MintPy 进程调用**：时序 InSAR 分析，conda 环境隔离

## 10. 部署架构

### 10.1 单机部署

```
┌─────────────────────────────────────┐
│         SoilWaterCM Backend          │
│    (Drogon HTTP Server on :8080)     │
└─────────────────────────────────────┘
         ↓           ↓           ↓
┌──────────┐  ┌──────────┐  ┌──────────┐
│PostgreSQL│  │ MongoDB  │  │  Redis   │
└──────────┘  └──────────┘  └──────────┘
```

### 10.2 Docker 部署

使用 Docker Compose 编排服务：

- soilwatercm-backend：C++ 后端服务
- postgres：PostgreSQL + PostGIS
- mongo：MongoDB
- redis：Redis
- prometheus：指标收集
- grafana：可视化仪表板

### 10.3 高可用部署

- **负载均衡**：Nginx 反向代理，负载均衡多个后端实例
- **数据库主从**：PostgreSQL 主从复制
- **Redis 集群**：Redis Sentinel 或 Redis Cluster
- **监控告警**：Prometheus + Grafana + AlertManager

## 11. 扩展性设计

### 11.1 插件化设计

- **外部工具接口**：统一的 ExternalToolInterface，易于添加新工具
- **算法插件**：算法模块可替换

### 11.2 配置化

- **配置文件**：YAML 配置，无需重新编译
- **环境变量**：支持环境变量覆盖配置

### 11.3 水平扩展

- **无状态设计**：后端服务无状态，可水平扩展
- **分布式任务**：任务队列支持分布式执行

## 12. 开发规范

### 12.1 编码规范

- 遵循 Google C++ Style Guide
- 使用 clang-format 格式化代码
- 使用 clang-tidy 静态分析

### 12.2 资源管理

- 禁止裸指针，使用智能指针
- 禁止手动内存管理，使用 RAII
- 禁止 C 风格数组，使用 std::vector

### 12.3 并发安全

- 共享状态使用 std::atomic 或互斥锁保护
- 避免数据竞争

### 12.4 错误处理

- 使用异常处理错误
- 自定义异常类，包含错误信息
- 异常安全保证

## 13. 测试策略

### 13.1 单元测试

- 测试框架：Google Test
- Mock 框架：Google Mock
- 覆盖率目标：≥ 80%

### 13.2 集成测试

- API 集成测试：测试 API 接口
- 数据库集成测试：测试数据库操作

### 13.3 性能测试

- 基准测试：Google Benchmark
- 并发测试：多线程并发测试
- 内存检测：AddressSanitizer、Valgrind

---

**文档版本**：v1.0  
**最后更新**：2026-06-26
