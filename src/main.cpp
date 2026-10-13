#include <iostream>
#include <memory>

#include <drogon/Drogon.h>

#include "controllers/anomaly_controller.h"
#include "controllers/auth_controller.h"
#include "controllers/basemap_controller.h"
#include "controllers/dashboard_controller.h"
#include "controllers/dynamic_controller.h"
#include "controllers/evaluation_controller.h"
#include "controllers/extraction_controller.h"
#include "controllers/acquisition_controller.h"
#include "controllers/ground_truth_controller.h"
#include "controllers/image_controller.h"
#include "controllers/indicator_controller.h"
#include "controllers/monitoring_controller.h"
#include "controllers/period_controller.h"
#include "controllers/processing_controller.h"
#include "controllers/project_controller.h"
#include "controllers/qgis_controller.h"
#include "controllers/report_controller.h"
#include "controllers/scene_layer_controller.h"
#include "controllers/task_controller.h"
#include "controllers/user_controller.h"
#include "controllers/zone_controller.h"
#include "dao/anomaly_dao.h"
#include "dao/basemap_dao.h"
#include "dao/disturbance_dao.h"
#include "dao/erosion_result_dao.h"
#include "dao/evaluation_dao.h"
#include "dao/ground_truth_dao.h"
#include "dao/image_dao.h"
#include "dao/indicator_dao.h"
#include "dao/measure_dao.h"
#include "dao/monitoring_period_dao.h"
#include "dao/project_dao.h"
#include "dao/project_document_dao.h"
#include "dao/report_dao.h"
#include "dao/result_dao.h"
#include "dao/spoil_dump_dao.h"
#include "dao/task_dao.h"
#include "dao/terrain_result_dao.h"
#include "dao/trained_model_dao.h"
#include "dao/user_dao.h"
#include "dao/vegetation_coverage_dao.h"
#include "dao/zone_dao.h"
#include "infrastructure/config_manager.h"
#include "infrastructure/external/qgis_client.h"
#include "infrastructure/external/satellite_client.h"
#include "infrastructure/external_tools/grass_interface.h"
#include "infrastructure/external_tools/otb_interface.h"
#include "infrastructure/external_tools/saga_interface.h"
#include "infrastructure/file_manager.h"
#include "infrastructure/metrics_collector.h"
#include "infrastructure/gdal/gdal_wrapper.h"
#include "infrastructure/gdal/raster_processor.h"
#include "infrastructure/gdal/vector_processor.h"
#include "infrastructure/thread_pool.h"
#include "middleware/auth_middleware.h"
#include "middleware/cors_middleware.h"
#include "middleware/jwt_middleware.h"
#include "middleware/logging_middleware.h"
#include "middleware/rate_limit_middleware.h"
#include "repository/mongo_repository.h"
#include "repository/postgres_repository.h"
#include "repository/redis_repository.h"
#include "services/anomaly_service.h"
#include "services/acquisition_service.h"
#include "services/auth_service.h"
#include "services/basemap_service.h"
#include "services/dashboard_service.h"
#include "services/disturbance_service.h"
#include "services/dynamic_service.h"
#include "services/erosion_service.h"
#include "services/evaluation_service.h"
#include "services/fvc_service.h"
#include "services/ground_truth_service.h"
#include "services/image_service.h"
#include "services/insar_service.h"
#include "services/indicator_service.h"
#include "services/measure_service.h"
#include "services/monitoring_service.h"
#include "services/period_service.h"
#include "services/preprocess_service.h"
#include "services/project_service.h"
#include "services/qgis_service.h"
#include "services/report_service.h"
#include "services/scheduler_service.h"
#include "services/scene_layer_service.h"
#include "services/task_service.h"
#include "services/zone_service.h"
#include "utils/json_utils.h"
#include "utils/logger.h"

using namespace soilwatercm;

namespace
{
    std::shared_ptr<services::AuthService> g_authService;
    std::shared_ptr<repository::PostgresRepository> g_pgRepo;
}

void initLogging()
{
    utils::Logger::init("logs/soilwatercm.log", "INFO", true);
    utils::Logger::info("Logging system initialized");
}

void initConfig()
{
    infrastructure::ConfigManager::getInstance().load("config/config.yaml");
    utils::Logger::info("Configuration loaded from config/config.yaml");
}

void initDatabase()
{
    try
    {
        auto& config = infrastructure::ConfigManager::getInstance();

        auto pgConfig = config.getDatabaseConfig("postgresql");
#pragma warning(push)
#pragma warning(disable : 4996)
        drogon::app().createDbClient(
            pgConfig.count("type") ? pgConfig.at("type") : "postgresql",
            pgConfig.count("host") ? pgConfig.at("host") : "localhost",
            pgConfig.count("port") ? static_cast<unsigned short>(std::stoi(pgConfig.at("port")))
                                   : static_cast<unsigned short>(5432),
            pgConfig.count("database") ? pgConfig.at("database") : "soilwatercm",
            pgConfig.count("username") ? pgConfig.at("username") : "postgres",
            pgConfig.count("password") ? pgConfig.at("password") : "",
            pgConfig.count("connection_count") ? std::stoi(pgConfig.at("connection_count")) : 10);
#pragma warning(pop)

        auto redisConfig = config.getDatabaseConfig("redis");
        auto redisHost = redisConfig.count("host") ? redisConfig.at("host") : "127.0.0.1";
        (void)redisHost;
        drogon::app().enableSession(3600);

        utils::Logger::info("Database connection pools initialized successfully");
    }
    catch (const std::exception& e)
    {
        utils::Logger::warn("Database initialization: {}", e.what());
    }
}

void initServices()
{
    auto& config = infrastructure::ConfigManager::getInstance();

    auto pgCfg = repository::PostgresConfig();
    pgCfg.host = config.getPostgresHost();
    pgCfg.port = config.getPostgresPort();
    pgCfg.database = config.getPostgresDatabase();
    pgCfg.user = config.getPostgresUser();
    pgCfg.password = config.getPostgresPassword();
    auto pgPool = std::make_shared<repository::PostgresConnectionPool>(pgCfg);
    auto pgRepo = std::make_shared<repository::PostgresRepository>(pgPool);
    g_pgRepo = pgRepo;

    auto userDao = std::make_shared<dao::UserDao>(pgRepo);
    auto projectDao = std::make_shared<dao::ProjectDao>(pgRepo);
    auto periodDao = std::make_shared<dao::MonitoringPeriodDao>(pgRepo);
    auto imageDao = std::make_shared<dao::ImageDao>(pgRepo);
    auto taskDao = std::make_shared<dao::TaskDao>(pgRepo);
    auto anomalyDao = std::make_shared<dao::AnomalyDao>(pgRepo);
    auto reportDao = std::make_shared<dao::ReportDao>(pgRepo);
    auto evaluationDao = std::make_shared<dao::EvaluationDao>(pgRepo);
    auto fvcDao = std::make_shared<dao::VegetationCoverageDao>(pgRepo);
    auto erosionDao = std::make_shared<dao::ErosionResultDao>(pgRepo);
    auto disturbanceDao = std::make_shared<dao::DisturbanceDao>(pgRepo);
    auto measureDao = std::make_shared<dao::MeasureDao>(pgRepo);
    auto zoneDao = std::make_shared<dao::ZoneDao>(pgRepo);
    auto indicatorDao = std::make_shared<dao::IndicatorDao>(pgRepo);
    auto groundTruthDao = std::make_shared<dao::GroundTruthDao>(pgRepo);
    auto basemapDao = std::make_shared<dao::BasemapDao>(pgRepo);
    auto projectDocumentDao = std::make_shared<dao::ProjectDocumentDao>(pgRepo);
    auto terrainResultDao = std::make_shared<dao::TerrainResultDao>(pgRepo);
    auto landuseResultDao = std::make_shared<dao::LandUseResultDao>(pgRepo);
    auto segmentationResultDao = std::make_shared<dao::SegmentationResultDao>(pgRepo);
    auto spoilDumpDao = std::make_shared<dao::SpoilDumpDao>(pgRepo);
    auto trainedModelDao = std::make_shared<dao::TrainedModelDao>(pgRepo);

    auto gdal = std::make_shared<infrastructure::gdal::GdalWrapper>();
    auto rasterProcessor = std::make_shared<infrastructure::gdal::RasterProcessor>();
    auto vectorProcessor = std::make_shared<infrastructure::gdal::VectorProcessor>();
    auto fileManager = std::make_shared<infrastructure::FileManager>(config.getStorageBaseDir());
    auto grass = std::make_shared<infrastructure::external_tools::GrassInterface>();
    auto qgisClient = std::make_shared<infrastructure::external::QgisClient>();
    auto saga = std::make_shared<infrastructure::SagaInterface>(qgisClient);
    auto otb = std::make_shared<infrastructure::OtbInterface>("");

    std::string jwtSecret = config.get<std::string>("jwt.secret", "soilwatercm-jwt-secret-key");
    g_authService = std::make_shared<services::AuthService>(userDao, jwtSecret);

    auto projectService = std::make_shared<services::ProjectService>(projectDao, periodDao, gdal, projectDocumentDao);
    auto imageService = std::make_shared<services::ImageService>(imageDao, fileManager, gdal);
    auto taskService = std::make_shared<services::TaskService>(taskDao);
    auto anomalyService = std::make_shared<services::AnomalyService>(anomalyDao, disturbanceDao, terrainResultDao,
                                                                     erosionDao);
    auto reportService = std::make_shared<services::ReportService>(reportDao, fileManager);
    auto evaluationService = std::make_shared<services::EvaluationService>(evaluationDao, taskDao);
    auto fvcService = std::make_shared<services::FvcService>(fvcDao, imageDao, taskDao, gdal, rasterProcessor);
    auto erosionService =
        std::make_shared<services::ErosionService>(erosionDao, taskDao, gdal, rasterProcessor, grass, saga);
    auto disturbanceService = std::make_shared<services::DisturbanceService>(
        disturbanceDao, taskDao, gdal, rasterProcessor, vectorProcessor, otb, grass);
    auto measureService = std::make_shared<services::MeasureService>(measureDao, taskDao, gdal, vectorProcessor);
    auto preprocessService =
        std::make_shared<services::PreprocessService>(imageDao, taskDao, gdal, rasterProcessor, otb);
    auto insarService = std::make_shared<services::InsarService>(
        taskDao,
        std::make_shared<infrastructure::SnaphuWrapper>(),
        std::make_shared<infrastructure::InterferogramGenerator>(),
        std::make_shared<infrastructure::PhaseFilter>(),
        std::make_shared<infrastructure::DeformationCalculator>(),
        std::make_shared<infrastructure::MintPyInterface>(),
        std::make_shared<infrastructure::Pyaps3Interface>());
    auto dashboardService = std::make_shared<services::DashboardService>(projectDao, imageDao, taskDao, anomalyDao);
    auto qgisService = std::make_shared<services::QgisService>(qgisClient, gdal);
    auto zoneService = std::make_shared<services::ZoneService>(zoneDao);
    auto indicatorService = std::make_shared<services::IndicatorService>(indicatorDao);
    auto groundTruthService = std::make_shared<services::GroundTruthService>(groundTruthDao);
    auto basemapService = std::make_shared<services::BasemapService>(basemapDao, gdal);
    auto satelliteClient = std::make_shared<infrastructure::external::SatelliteClient>();
    auto acquisitionService = std::make_shared<services::AcquisitionService>(satelliteClient, taskDao);
    auto periodService = std::make_shared<services::PeriodService>(periodDao, disturbanceDao, fvcDao, erosionDao,
                                                                   measureDao, anomalyDao, reportDao);
    auto sceneLayerService = std::make_shared<services::SceneLayerService>(disturbanceDao, measureDao, anomalyDao,
                                                                           periodDao, fvcDao, erosionDao,
                                                                           terrainResultDao, spoilDumpDao,
                                                                           vectorProcessor);
    auto monitoringService = std::make_shared<services::MonitoringService>(periodDao, taskDao);
    auto dynamicService = std::make_shared<services::DynamicService>(periodDao, imageDao, taskDao, indicatorDao, fvcDao,
                                                                     erosionDao, disturbanceDao, measureDao);

    // 定时任务调度器 (季度/年报 + 孤立成果清理), 替代 Python Celery beat
    auto schedulerService = std::make_shared<services::SchedulerService>(projectDao, periodDao, reportService,
                                                                         fvcDao, erosionDao, landuseResultDao,
                                                                         segmentationResultDao, terrainResultDao);
    schedulerService->start();

    drogon::app().registerController(std::make_shared<controllers::AuthController>(g_authService));
    drogon::app().registerController(std::make_shared<controllers::UserController>(userDao, g_authService));
    drogon::app().registerController(std::make_shared<controllers::ProjectController>(projectService));
    drogon::app().registerController(std::make_shared<controllers::ImageController>(imageService));
    drogon::app().registerController(std::make_shared<controllers::ProcessingController>(preprocessService, insarService));
    drogon::app().registerController(std::make_shared<controllers::ExtractionController>(
        fvcService, erosionService, disturbanceService, measureService));
    drogon::app().registerController(std::make_shared<controllers::EvaluationController>(evaluationService));
    drogon::app().registerController(std::make_shared<controllers::AnomalyController>(anomalyService));
    drogon::app().registerController(std::make_shared<controllers::ReportController>(reportService));
    drogon::app().registerController(std::make_shared<controllers::TaskController>(taskService));
    drogon::app().registerController(std::make_shared<controllers::DashboardController>(dashboardService));
    drogon::app().registerController(std::make_shared<controllers::QgisController>(qgisService));
    drogon::app().registerController(std::make_shared<controllers::ZoneController>(zoneService, vectorProcessor));
    drogon::app().registerController(std::make_shared<controllers::IndicatorController>(indicatorService));
    drogon::app().registerController(std::make_shared<controllers::GroundTruthController>(groundTruthService));
    drogon::app().registerController(std::make_shared<controllers::BasemapController>(basemapService));
    drogon::app().registerController(std::make_shared<controllers::AcquisitionController>(acquisitionService));
    drogon::app().registerController(std::make_shared<controllers::PeriodController>(periodService));
    drogon::app().registerController(std::make_shared<controllers::SceneLayerController>(sceneLayerService));
    drogon::app().registerController(std::make_shared<controllers::MonitoringController>(monitoringService));
    drogon::app().registerController(std::make_shared<controllers::DynamicController>(dynamicService));

    utils::Logger::info("Business services initialized successfully");
}

void registerMiddleware()
{
    drogon::app().registerMiddleware(std::make_shared<middleware::LoggingMiddleware>());
    drogon::app().registerMiddleware(std::make_shared<middleware::CorsMiddleware>());

    auto& config = infrastructure::ConfigManager::getInstance();
    auto rateLimitConfig = config.getRateLimitConfig();
    auto rateLimit = std::make_shared<middleware::RateLimitMiddleware>();
    rateLimit->setIpRateLimit(
        rateLimitConfig.count("max_requests") ? std::stoi(rateLimitConfig.at("max_requests")) : 100,
        rateLimitConfig.count("window_seconds") ? std::stoi(rateLimitConfig.at("window_seconds")) : 60);
    drogon::app().registerMiddleware(rateLimit);

    auto jwt = std::make_shared<middleware::JwtMiddleware>(g_authService);
    drogon::app().registerMiddleware(jwt);
    drogon::app().registerMiddleware(std::make_shared<middleware::AuthMiddleware>());

    utils::Logger::info("Middleware registered successfully");
}

void registerControllers()
{
    utils::Logger::info("Controllers registered via registerController calls");
}

// 注册运维端点: /health (存活 + DB 探活) 与 /metrics (Prometheus 文本)
void registerSystemRoutes()
{
    drogon::app().registerHandler(
        "/health",
        [](const drogon::HttpRequestPtr& /*req*/,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            Json::Value body;
            body["status"] = "ok";
            body["service"] = "soilwatercm";
            body["version"] = "1.0";

            // DB 探活: 连不上则标记 degraded 并返回 503
            bool dbOk = false;
            if (g_pgRepo)
            {
                auto rows = g_pgRepo->executeQuery("SELECT 1 AS ok");
                dbOk = !rows.empty();
            }
            body["database"] = dbOk ? "up" : "down";

            auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(dbOk ? drogon::k200OK : drogon::k503ServiceUnavailable);
            callback(resp);
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/metrics",
        [](const drogon::HttpRequestPtr& /*req*/,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
            resp->setBody(infrastructure::MetricsCollector::instance().exportPrometheus());
            callback(resp);
        },
        {drogon::Get});

    utils::Logger::info("System routes registered: /health /metrics");
}

int main(int argc, char* argv[])
{
    try
    {
        initLogging();

        utils::Logger::info("========================================");
        utils::Logger::info("SoilWaterCM C++ Backend Starting...");
        utils::Logger::info("========================================");

        initConfig();
        initDatabase();
        initServices();
        registerMiddleware();
        registerSystemRoutes();

        drogon::app().loadConfigFile("config/drogon.json");

        drogon::app().registerPreRoutingAdvice(
            [](const drogon::HttpRequestPtr& req)
            {
                req->setParameter("request_start_time",
                                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            });

        drogon::app().registerPostHandlingAdvice(
            [](const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp)
            {
                resp->addHeader("Server", "SoilWaterCM/1.0");
                resp->addHeader("X-Content-Type-Options", "nosniff");
                resp->addHeader("X-Frame-Options", "DENY");
            });

        utils::Logger::info("HTTP server starting on port 8080");

        drogon::app().run();
    }
    catch (const std::exception& e)
    {
        utils::Logger::critical("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
