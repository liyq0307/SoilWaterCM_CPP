# SoilWaterCM C++ 后端部署文档

## 1. 环境要求

### 1.1 操作系统

- **Linux**：Ubuntu 20.04+、CentOS 7+、Debian 10+
- **Windows**：Windows 10/11、Windows Server 2016+

### 1.2 编译器

- **Linux**：GCC ≥ 9.0 或 Clang ≥ 10.0
- **Windows**：MSVC ≥ 2019 (Visual Studio 2019+)

### 1.3 依赖版本

| 依赖 | 版本要求 | 说明 |
|------|---------|------|
| CMake | ≥ 3.20.0 | 构建工具 |
| vcpkg | Latest | C++ 包管理器 |
| PostgreSQL | ≥ 14.0 | 主数据库 |
| PostGIS | ≥ 3.3.0 | 空间数据扩展 |
| MongoDB | ≥ 5.0 | 文档数据库 |
| Redis | ≥ 6.0 | 缓存数据库 |
| GDAL | ≥ 3.8.0 | 空间数据处理 |
| QGIS | ≥ 3.28.0 | GIS工具（含GRASS、SAGA） |

### 1.4 外部工具（可选）

| 工具 | 版本要求 | 用途 |
|------|---------|------|
| GRASS GIS | ≥ 8.3.0 | 地形分析、水文分析、LS因子计算 |
| SAGA GIS | ≥ 8.0.0 | K因子计算、地形形态分析 |
| OTB | ≥ 8.1.0 | 辐射校正、正射纠正、影像分类 |
| MintPy | ≥ 1.5.0 | 时序InSAR分析 |
| pyaps3 | Latest | 大气校正 |

---

## 2. Linux 部署

### 2.1 安装系统依赖

**Ubuntu/Debian**：
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    curl \
    zip \
    unzip \
    tar \
    postgresql \
    postgresql-contrib \
    postgresql-14-postgis-3 \
    mongodb \
    redis-server \
    libgdal-dev \
    gdal-bin \
    python3 \
    python3-pip
```

**CentOS/RHEL**：
```bash
sudo yum install -y \
    gcc \
    gcc-c++ \
    cmake \
    git \
    pkgconfig \
    curl \
    zip \
    unzip \
    postgresql \
    postgresql-contrib \
    postgis \
    mongodb \
    redis \
    gdal-devel \
    gdal \
    python3 \
    python3-pip
```

### 2.2 安装 vcpkg

```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg integrate install
```

设置环境变量：
```bash
export VCPKG_ROOT=/path/to/vcpkg
```

### 2.3 编译项目

```bash
cd SoilWaterCM_CPP

# 配置 CMake
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# 编译
cmake --build build --config Release -j$(nproc)

# 安装
sudo cmake --install build
```

### 2.4 配置数据库

**PostgreSQL**：
```bash
# 启动服务
sudo systemctl start postgresql
sudo systemctl enable postgresql

# 创建数据库和用户
sudo -u postgres psql << EOF
CREATE USER soilwatercm WITH PASSWORD 'your_password';
CREATE DATABASE soilwatercm OWNER soilwatercm;
GRANT ALL PRIVILEGES ON DATABASE soilwatercm TO soilwatercm;
\c soilwatercm
CREATE EXTENSION postgis;
CREATE EXTENSION postgis_topology;
EOF
```

**MongoDB**：
```bash
# 启动服务
sudo systemctl start mongodb
sudo systemctl enable mongodb

# 创建数据库和用户
mongo << EOF
use soilwatercm
db.createUser({
  user: "soilwatercm",
  pwd: "your_password",
  roles: [{role: "readWrite", db: "soilwatercm"}]
})
EOF
```

**Redis**：
```bash
# 启动服务
sudo systemctl start redis
sudo systemctl enable redis

# 设置密码（可选）
sudo sed -i 's/# requirepass foobared/requirepass your_password/' /etc/redis/redis.conf
sudo systemctl restart redis
```

### 2.5 配置应用

编辑配置文件 `config/config.yaml`：
```yaml
server:
  host: "0.0.0.0"
  port: 8080
  workers: 4

database:
  postgres:
    host: "localhost"
    port: 5432
    database: "soilwatercm"
    user: "soilwatercm"
    password: "your_password"
    pool_size: 10
  
  mongo:
    uri: "mongodb://soilwatercm:your_password@localhost:27017/soilwatercm"
  
  redis:
    host: "localhost"
    port: 6379
    password: "your_password"

jwt:
  secret: "your_jwt_secret"
  expiration: 3600

logging:
  level: "INFO"
  file: "/var/log/soilwatercm/app.log"
```

### 2.6 配置 Systemd 服务

创建服务文件 `/etc/systemd/system/soilwatercm.service`：
```ini
[Unit]
Description=SoilWaterCM C++ Backend
After=network.target postgresql.service mongodb.service redis.service

[Service]
Type=simple
User=soilwatercm
Group=soilwatercm
WorkingDirectory=/opt/soilwatercm
ExecStart=/opt/soilwatercm/bin/soilwatercm
Restart=on-failure
RestartSec=10
StandardOutput=syslog
StandardError=syslog
SyslogIdentifier=soilwatercm

[Install]
WantedBy=multi-user.target
```

启动服务：
```bash
sudo systemctl daemon-reload
sudo systemctl start soilwatercm
sudo systemctl enable soilwatercm
```

---

## 3. Windows 部署

### 3.1 安装系统依赖

1. **Visual Studio 2019+**：安装 C++ 开发工具
2. **CMake**：下载并安装 https://cmake.org/download/
3. **vcpkg**：
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg integrate install
   ```

4. **PostgreSQL**：下载并安装 https://www.postgresql.org/download/windows/
5. **MongoDB**：下载并安装 https://www.mongodb.com/download-center/community
6. **Redis**：下载并安装 https://github.com/microsoftarchive/redis/releases

### 3.2 编译项目

```powershell
cd SoilWaterCM_CPP

# 配置 CMake
cmake -B build -S . `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake

# 编译
cmake --build build --config Release

# 安装
cmake --install build --prefix C:\opt\soilwatercm
```

### 3.3 配置应用

编辑配置文件 `config/config.yaml`，参考 Linux 部署章节。

### 3.4 配置 Windows 服务

使用 NSSM (Non-Sucking Service Manager)：

1. 下载 NSSM：https://nssm.cc/download
2. 安装服务：
   ```powershell
   nssm install SoilWaterCM C:\opt\soilwatercm\bin\soilwatercm.exe
   nssm set SoilWaterCM AppDirectory C:\opt\soilwatercm
   nssm set SoilWaterCM DisplayName "SoilWaterCM C++ Backend"
   nssm set SoilWaterCM Description "SoilWaterCM C++ Backend Service"
   nssm set SoilWaterCM Start SERVICE_AUTO_START
   nssm start SoilWaterCM
   ```

---

## 4. Docker 部署

### 4.1 构建镜像

```bash
cd SoilWaterCM_CPP
docker build -t soilwatercm-backend:latest -f docker/Dockerfile .
```

### 4.2 使用 Docker Compose

编辑 `docker/docker-compose.yml`：
```yaml
version: '3.8'

services:
  postgres:
    image: postgis/postgis:14-3.3
    environment:
      POSTGRES_USER: soilwatercm
      POSTGRES_PASSWORD: your_password
      POSTGRES_DB: soilwatercm
    volumes:
      - postgres_data:/var/lib/postgresql/data
    ports:
      - "5432:5432"

  mongo:
    image: mongo:5.0
    environment:
      MONGO_INITDB_ROOT_USERNAME: soilwatercm
      MONGO_INITDB_ROOT_PASSWORD: your_password
    volumes:
      - mongo_data:/data/db
    ports:
      - "27017:27017"

  redis:
    image: redis:6.0
    command: redis-server --requirepass your_password
    volumes:
      - redis_data:/data
    ports:
      - "6379:6379"

  backend:
    image: soilwatercm-backend:latest
    depends_on:
      - postgres
      - mongo
      - redis
    environment:
      POSTGRES_HOST: postgres
      POSTGRES_PORT: 5432
      POSTGRES_USER: soilwatercm
      POSTGRES_PASSWORD: your_password
      POSTGRES_DB: soilwatercm
      MONGO_URI: mongodb://soilwatercm:your_password@mongo:27017/soilwatercm
      REDIS_HOST: redis
      REDIS_PORT: 6379
      REDIS_PASSWORD: your_password
      JWT_SECRET: your_jwt_secret
    volumes:
      - ./config:/app/config
      - ./data:/data
    ports:
      - "8080:8080"

  prometheus:
    image: prom/prometheus:latest
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml
    ports:
      - "9090:9090"

  grafana:
    image: grafana/grafana:latest
    environment:
      GF_SECURITY_ADMIN_PASSWORD: admin
    volumes:
      - grafana_data:/var/lib/grafana
    ports:
      - "3000:3000"

volumes:
  postgres_data:
  mongo_data:
  redis_data:
  grafana_data:
```

启动服务：
```bash
docker-compose -f docker/docker-compose.yml up -d
```

---

## 5. 外部工具安装

### 5.1 QGIS（含GRASS、SAGA）

**Linux**：
```bash
sudo apt-get install -y qgis qgis-plugin-grass saga
```

**Windows**：下载安装 https://qgis.org/en/site/forusers/download.html

### 5.2 OTB

**Linux**：
```bash
# 下载 OTB
wget https://www.orfeo-toolbox.org/packages/OTB-8.1.0-Linux64.run
chmod +x OTB-8.1.0-Linux64.run
./OTB-8.1.0-Linux64.run

# 配置环境变量
export OTB_HOME=/path/to/OTB-8.1.0-Linux64
export PATH=$OTB_HOME/bin:$PATH
```

**Windows**：下载安装 https://www.orfeo-toolbox.org/download/

### 5.3 MintPy（时序InSAR分析）

```bash
# 创建 conda 环境
conda create -n mintpy python=3.9
conda activate mintpy

# 安装 MintPy
conda install -c conda-forge mintpy

# 安装 pyaps3（大气校正）
pip install pyaps3
```

配置环境变量：
```bash
export MINTPY_ENV=mintpy
```

---

## 6. 配置说明

### 6.1 主配置文件 (config/config.yaml)

```yaml
server:
  host: "0.0.0.0"          # 监听地址
  port: 8080               # 监听端口
  workers: 4               # 工作线程数

database:
  postgres:
    host: "localhost"
    port: 5432
    database: "soilwatercm"
    user: "soilwatercm"
    password: "your_password"
    pool_size: 10          # 连接池大小
  
  mongo:
    uri: "mongodb://soilwatercm:your_password@localhost:27017/soilwatercm"
  
  redis:
    host: "localhost"
    port: 6379
    password: "your_password"

jwt:
  secret: "your_jwt_secret"    # JWT密钥
  expiration: 3600             # Token过期时间（秒）

logging:
  level: "INFO"                # 日志级别
  file: "/var/log/soilwatercm/app.log"
  max_size: 100                # 单个日志文件最大大小（MB）
  max_files: 10                # 保留日志文件数量

storage:
  base_path: "/data"           # 数据存储根路径
  image_path: "/data/images"   # 影像存储路径
  result_path: "/data/results" # 结果存储路径

external_tools:
  grass_home: "/usr/lib/grass82"
  saga_home: "/usr/lib/saga"
  otb_home: "/opt/otb"
  mintpy_env: "mintpy"
```

### 6.2 环境变量

可以使用环境变量覆盖配置：

| 环境变量 | 说明 |
|---------|------|
| POSTGRES_HOST | PostgreSQL 主机 |
| POSTGRES_PORT | PostgreSQL 端口 |
| POSTGRES_USER | PostgreSQL 用户 |
| POSTGRES_PASSWORD | PostgreSQL 密码 |
| POSTGRES_DB | PostgreSQL 数据库 |
| MONGO_URI | MongoDB 连接URI |
| REDIS_HOST | Redis 主机 |
| REDIS_PORT | Redis 端口 |
| REDIS_PASSWORD | Redis 密码 |
| JWT_SECRET | JWT 密钥 |
| LOG_LEVEL | 日志级别 |

---

## 7. 服务管理

### 7.1 Linux (Systemd)

```bash
# 启动服务
sudo systemctl start soilwatercm

# 停止服务
sudo systemctl stop soilwatercm

# 重启服务
sudo systemctl restart soilwatercm

# 查看状态
sudo systemctl status soilwatercm

# 查看日志
sudo journalctl -u soilwatercm -f
```

### 7.2 Windows (NSSM)

```powershell
# 启动服务
nssm start SoilWaterCM

# 停止服务
nssm stop SoilWaterCM

# 重启服务
nssm restart SoilWaterCM

# 查看状态
nssm status SoilWaterCM
```

### 7.3 Docker

```bash
# 启动服务
docker-compose -f docker/docker-compose.yml up -d

# 停止服务
docker-compose -f docker/docker-compose.yml down

# 重启服务
docker-compose -f docker/docker-compose.yml restart

# 查看日志
docker-compose -f docker/docker-compose.yml logs -f backend
```

---

## 8. 监控配置

### 8.1 Prometheus 配置

编辑 `prometheus.yml`：
```yaml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'soilwatercm'
    static_configs:
      - targets: ['backend:8080']
```

### 8.2 Grafana 配置

1. 访问 Grafana：http://localhost:3000
2. 添加 Prometheus 数据源
3. 导入仪表板配置

---

## 9. 性能调优

### 9.1 数据库优化

**PostgreSQL**：
```sql
-- 编辑 postgresql.conf
shared_buffers = 256MB
effective_cache_size = 1GB
maintenance_work_mem = 64MB
checkpoint_completion_target = 0.9
wal_buffers = 16MB
default_statistics_target = 100
random_page_cost = 1.1
effective_io_concurrency = 200
work_mem = 2621kB
min_wal_size = 1GB
max_wal_size = 4GB
```

**Redis**：
```bash
# 编辑 redis.conf
maxmemory 2gb
maxmemory-policy allkeys-lru
```

### 9.2 应用优化

- **线程数**：根据 CPU 核心数调整 `server.workers`
- **连接池**：根据并发量调整 `database.postgres.pool_size`
- **日志级别**：生产环境使用 INFO 或 WARNING

---

## 10. 故障排查

### 10.1 常见问题

**问题1：数据库连接失败**
```
错误：could not connect to server: Connection refused
解决：检查 PostgreSQL 服务是否启动，检查连接配置
```

**问题2：GDAL 找不到**
```
错误：GDAL not found
解决：安装 GDAL 并设置环境变量
export GDAL_DATA=/usr/share/gdal
```

**问题3：外部工具调用失败**
```
错误：GRASS GIS not found
解决：检查工具路径配置，确保工具已安装
```

### 10.2 日志查看

**应用日志**：
```bash
tail -f /var/log/soilwatercm/app.log
```

**Systemd 日志**：
```bash
sudo journalctl -u soilwatercm -f
```

---

## 11. 升级指南

### 11.1 备份数据

```bash
# 备份 PostgreSQL
pg_dump -U soilwatercm soilwatercm > backup_$(date +%Y%m%d).sql

# 备份 MongoDB
mongodump --uri="mongodb://soilwatercm:your_password@localhost:27017/soilwatercm" --out=mongo_backup_$(date +%Y%m%d)

# 备份文件
tar -czf data_backup_$(date +%Y%m%d).tar.gz /data
```

### 11.2 升级步骤

1. 停止服务
2. 备份数据
3. 更新代码
4. 重新编译
5. 更新配置（如有变更）
6. 启动服务
7. 验证功能

---

**文档版本**：v1.0  
**最后更新**：2026-06-26
