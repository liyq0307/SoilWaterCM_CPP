-- 新增成果表 DDL (与 Python 版 backend/app/models/{results,spoil_dump,trained_model}.py 对齐)
-- 幂等: 可重复执行。已有 20 张基础表不在这里, 见 README 第 9 节。

CREATE EXTENSION IF NOT EXISTS postgis;

-- 地形/专项分析成果 (地形指数/淹没/视域/InSAR 形变, analysis 区分类型)
CREATE TABLE IF NOT EXISTS terrain_results (
    id          SERIAL PRIMARY KEY,
    project_id  INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    analysis    VARCHAR(32) NOT NULL,          -- twi/tpi/tri/solar/channel/inundation/viewshed/insar
    raster_path VARCHAR(512),
    stats       TEXT,                          -- JSON 统计信息文本
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_terrain_results_project ON terrain_results(project_id);
CREATE INDEX IF NOT EXISTS idx_terrain_results_analysis ON terrain_results(analysis);

-- 土地利用分类成果
CREATE TABLE IF NOT EXISTS landuse_results (
    id           SERIAL PRIMARY KEY,
    project_id   INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    period_id    INTEGER REFERENCES monitoring_periods(id) ON DELETE CASCADE,
    image_id     INTEGER REFERENCES images(id),
    method       VARCHAR(32) NOT NULL DEFAULT 'kmeans',
    n_classes    INTEGER,
    raster_path  VARCHAR(512),
    class_areas  TEXT,                         -- 各类面积统计 JSON 文本
    created_at   TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_landuse_results_project ON landuse_results(project_id);

-- 影像分割成果
CREATE TABLE IF NOT EXISTS segmentation_results (
    id          SERIAL PRIMARY KEY,
    project_id  INTEGER REFERENCES projects(id) ON DELETE CASCADE,
    period_id   INTEGER REFERENCES monitoring_periods(id),
    image_id    INTEGER NOT NULL REFERENCES images(id),
    method      VARCHAR(32) NOT NULL DEFAULT 'mean_shift',
    n_segments  INTEGER,
    raster_path VARCHAR(512),
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_segmentation_results_image ON segmentation_results(image_id);

-- 弃渣场图斑 (堆渣方量/堆高/稳定性/合规性)
CREATE TABLE IF NOT EXISTS spoil_dumps (
    id                SERIAL PRIMARY KEY,
    project_id        INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    period_id         INTEGER NOT NULL REFERENCES monitoring_periods(id) ON DELETE CASCADE,
    image_id          INTEGER REFERENCES images(id),
    geom              GEOMETRY(MULTIPOLYGON, 4326) NOT NULL,
    area_m2           DOUBLE PRECISION NOT NULL,
    volume_m3         DOUBLE PRECISION,
    max_height_m      DOUBLE PRECISION,
    mean_height_m     DOUBLE PRECISION,
    mean_slope_deg    DOUBLE PRECISION,
    in_approved_area  BOOLEAN,
    has_protection    BOOLEAN,
    protection_types  VARCHAR(128),
    stability_level   VARCHAR(16) NOT NULL DEFAULT 'low_risk',
    risk_score        DOUBLE PRECISION,
    compliance        VARCHAR(32) NOT NULL DEFAULT 'pending',
    detect_method     VARCHAR(32) NOT NULL DEFAULT 'dem_diff',
    confidence        DOUBLE PRECISION,
    note              TEXT,
    created_at        TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_spoil_dumps_project ON spoil_dumps(project_id);
CREATE INDEX IF NOT EXISTS idx_spoil_dumps_stability ON spoil_dumps(stability_level);
CREATE INDEX IF NOT EXISTS idx_spoil_dumps_compliance ON spoil_dumps(compliance);

-- 训练模型注册表 (独立于任务记录维护模型生命周期)
CREATE TABLE IF NOT EXISTS trained_models (
    id               SERIAL PRIMARY KEY,
    project_id       INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    period_id        INTEGER REFERENCES monitoring_periods(id) ON DELETE SET NULL,
    image_id         INTEGER REFERENCES images(id) ON DELETE SET NULL,
    source_task_id   INTEGER REFERENCES tasks(id) ON DELETE SET NULL,
    method           VARCHAR(32) NOT NULL,
    model_name       VARCHAR(255) NOT NULL,
    model_path       VARCHAR(2048) NOT NULL,
    status           VARCHAR(16) NOT NULL DEFAULT 'available',
    metrics          TEXT,
    file_size        BIGINT,
    file_mtime_ns    BIGINT,
    file_sha256      VARCHAR(64),
    created_at       TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at       TIMESTAMPTZ NOT NULL DEFAULT now(),
    deleted_at       TIMESTAMPTZ,
    CONSTRAINT uq_trained_models_project_path UNIQUE (project_id, model_path)
);
CREATE INDEX IF NOT EXISTS idx_trained_models_project ON trained_models(project_id);
CREATE INDEX IF NOT EXISTS idx_trained_models_status ON trained_models(status);
