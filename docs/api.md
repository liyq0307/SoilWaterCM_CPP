# SoilWaterCM C++ 后端 API 文档

## 1. API 概述

### 1.1 基础信息

- **基础路径**：`/api/v1`
- **协议**：HTTP/HTTPS
- **数据格式**：JSON
- **字符编码**：UTF-8

### 1.2 认证机制

除登录接口外，所有接口需要在请求头中携带 JWT Token：

```
Authorization: Bearer <token>
```

### 1.3 响应格式

所有接口统一响应格式：

**成功响应**：
```json
{
  "code": 200,
  "message": "success",
  "data": { ... }
}
```

**错误响应**：
```json
{
  "code": 400,
  "message": "错误描述",
  "data": null
}
```

**分页响应**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "items": [ ... ],
    "total": 100,
    "page": 1,
    "page_size": 20
  }
}
```

### 1.4 错误码

| 错误码 | 说明 |
|--------|------|
| 200 | 成功 |
| 400 | 请求参数错误 |
| 401 | 未认证或Token无效 |
| 403 | 权限不足 |
| 404 | 资源不存在 |
| 409 | 资源冲突（如唯一性冲突） |
| 429 | 请求过于频繁（限流） |
| 500 | 服务器内部错误 |
| 503 | 服务不可用 |

---

## 2. 认证接口

### 2.1 用户登录

**接口**：`POST /api/v1/auth/login`

**说明**：用户登录，获取 JWT Token

**请求参数**：
```json
{
  "username": "admin",
  "password": "password123"
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
    "user": {
      "id": 1,
      "username": "admin",
      "real_name": "管理员",
      "email": "admin@example.com",
      "role": "Admin"
    }
  }
}
```

### 2.2 用户登出

**接口**：`POST /api/v1/auth/logout`

**说明**：用户登出，失效 Token

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

### 2.3 获取当前用户信息

**接口**：`GET /api/v1/auth/me`

**说明**：获取当前登录用户信息

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 1,
    "username": "admin",
    "real_name": "管理员",
    "email": "admin@example.com",
    "phone": "13800138000",
    "role": "Admin",
    "is_active": true,
    "created_at": "2024-01-01T00:00:00Z",
    "last_login": "2024-06-26T10:00:00Z"
  }
}
```

---

## 3. 用户管理接口

### 3.1 获取用户列表

**接口**：`GET /api/v1/users`

**说明**：获取用户列表（分页）

**请求头**：需要认证，需要 Admin 角色

**查询参数**：
- `page`：页码，默认 1
- `page_size`：每页数量，默认 20
- `role`：角色过滤（可选）
- `is_active`：状态过滤（可选）

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "items": [
      {
        "id": 1,
        "username": "admin",
        "real_name": "管理员",
        "email": "admin@example.com",
        "role": "Admin",
        "is_active": true
      }
    ],
    "total": 10,
    "page": 1,
    "page_size": 20
  }
}
```

### 3.2 创建用户

**接口**：`POST /api/v1/users`

**说明**：创建新用户

**请求头**：需要认证，需要 Admin 角色

**请求参数**：
```json
{
  "username": "user1",
  "password": "password123",
  "real_name": "用户1",
  "email": "user1@example.com",
  "phone": "13800138001",
  "role": "Monitor"
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 2,
    "username": "user1"
  }
}
```

### 3.3 更新用户

**接口**：`PUT /api/v1/users/{id}`

**说明**：更新用户信息

**请求头**：需要认证

**请求参数**：
```json
{
  "real_name": "新名称",
  "email": "newemail@example.com",
  "phone": "13900139000"
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

### 3.4 删除用户

**接口**：`DELETE /api/v1/users/{id}`

**说明**：删除用户

**请求头**：需要认证，需要 Admin 角色

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

---

## 4. 项目管理接口

### 4.1 获取项目列表

**接口**：`GET /api/v1/projects`

**说明**：获取项目列表（分页）

**请求头**：需要认证

**查询参数**：
- `page`：页码，默认 1
- `page_size`：每页数量，默认 20
- `status`：状态过滤（可选）
- `keyword`：关键词搜索（可选）

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "items": [
      {
        "id": 1,
        "code": "PRJ001",
        "name": "测试项目",
        "responsibility_area": "测试区域",
        "center_lon": 116.0,
        "center_lat": 39.0,
        "status": "Active",
        "created_at": "2024-01-01T00:00:00Z"
      }
    ],
    "total": 5,
    "page": 1,
    "page_size": 20
  }
}
```

### 4.2 创建项目

**接口**：`POST /api/v1/projects`

**说明**：创建新项目

**请求头**：需要认证

**请求参数**：
```json
{
  "code": "PRJ002",
  "name": "新项目",
  "responsibility_area": "新区域",
  "center_lon": 117.0,
  "center_lat": 40.0,
  "geometry": {
    "type": "Polygon",
    "coordinates": [[[116.0, 39.0], [117.0, 39.0], [117.0, 40.0], [116.0, 40.0], [116.0, 39.0]]]
  }
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 2,
    "code": "PRJ002"
  }
}
```

### 4.3 获取项目详情

**接口**：`GET /api/v1/projects/{id}`

**说明**：获取项目详情

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 1,
    "code": "PRJ001",
    "name": "测试项目",
    "responsibility_area": "测试区域",
    "center_lon": 116.0,
    "center_lat": 39.0,
    "geometry": {
      "type": "Polygon",
      "coordinates": [[[116.0, 39.0], [117.0, 39.0], [117.0, 40.0], [116.0, 40.0], [116.0, 39.0]]]
    },
    "status": "Active",
    "created_at": "2024-01-01T00:00:00Z",
    "updated_at": "2024-06-26T10:00:00Z"
  }
}
```

### 4.4 更新项目

**接口**：`PUT /api/v1/projects/{id}`

**说明**：更新项目信息

**请求头**：需要认证

**请求参数**：
```json
{
  "name": "更新后的项目名",
  "responsibility_area": "更新后的区域"
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

### 4.5 删除项目

**接口**：`DELETE /api/v1/projects/{id}`

**说明**：删除项目（级联删除关联数据）

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

### 4.6 获取项目监测期列表

**接口**：`GET /api/v1/projects/{id}/periods`

**说明**：获取项目的监测期列表

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": [
    {
      "id": 1,
      "period_code": "202401",
      "type": "Monthly",
      "start_date": "2024-01-01",
      "end_date": "2024-01-31",
      "status": "Completed"
    }
  ]
}
```

---

## 5. 影像管理接口

### 5.1 获取影像列表

**接口**：`GET /api/v1/images`

**说明**：获取影像列表（分页）

**请求头**：需要认证

**查询参数**：
- `page`：页码
- `page_size`：每页数量
- `project_id`：项目ID过滤（可选）
- `period_id`：监测期ID过滤（可选）
- `type`：影像类型过滤（可选）

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "items": [
      {
        "id": 1,
        "image_code": "IMG001",
        "type": "Optical",
        "sensor_type": "Sentinel-2",
        "acquisition_time": "2024-01-15T10:00:00Z",
        "file_path": "/data/images/IMG001.tif",
        "file_size": 104857600,
        "cloud_cover": 0.1,
        "quality_status": "Passed",
        "project_id": 1,
        "period_id": 1
      }
    ],
    "total": 20,
    "page": 1,
    "page_size": 20
  }
}
```

### 5.2 上传影像

**接口**：`POST /api/v1/images`

**说明**：上传影像文件

**请求头**：需要认证，`Content-Type: multipart/form-data`

**请求参数**：
- `file`：影像文件
- `project_id`：项目ID
- `period_id`：监测期ID
- `type`：影像类型
- `sensor_type`：传感器类型

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 2,
    "image_code": "IMG002",
    "quality_status": "Pending"
  }
}
```

### 5.3 获取影像详情

**接口**：`GET /api/v1/images/{id}`

**说明**：获取影像详情

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 1,
    "image_code": "IMG001",
    "type": "Optical",
    "sensor_type": "Sentinel-2",
    "acquisition_time": "2024-01-15T10:00:00Z",
    "extent": {
      "type": "Polygon",
      "coordinates": [[[...]]]
    },
    "crs": "EPSG:4326",
    "resolution": 10.0,
    "file_path": "/data/images/IMG001.tif",
    "file_size": 104857600,
    "cloud_cover": 0.1,
    "quality_status": "Passed",
    "quality_message": "质量检查通过",
    "project_id": 1,
    "period_id": 1
  }
}
```

### 5.4 下载影像

**接口**：`GET /api/v1/images/{id}/download`

**说明**：下载影像文件

**请求头**：需要认证

**响应**：影像文件流

### 5.5 删除影像

**接口**：`DELETE /api/v1/images/{id}`

**说明**：删除影像

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

---

## 6. 数据处理接口

### 6.1 提交预处理任务

**接口**：`POST /api/v1/processing/preprocess`

**说明**：提交影像预处理任务

**请求头**：需要认证

**请求参数**：
```json
{
  "image_ids": [1, 2, 3],
  "operations": ["radiometric_correction", "geometric_correction", "orthorectification"],
  "params": {
    "output_format": "GTiff",
    "resampling_method": "bilinear"
  }
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "task_id": 1
  }
}
```

### 6.2 提交镶嵌任务

**接口**：`POST /api/v1/processing/mosaic`

**说明**：提交影像镶嵌任务

**请求头**：需要认证

**请求参数**：
```json
{
  "image_ids": [1, 2, 3],
  "output_path": "/data/mosaic/result.tif",
  "params": {
    "resampling_method": "bilinear",
    "nodata_value": 0
  }
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "task_id": 2
  }
}
```

---

## 7. 信息提取接口

### 7.1 提交 FVC 反演任务

**接口**：`POST /api/v1/extraction/fvc`

**说明**：提交植被覆盖度反演任务

**请求头**：需要认证

**请求参数**：
```json
{
  "image_id": 1,
  "period_id": 1,
  "params": {
    "ndvi_soil": 0.2,
    "ndvi_veg": 0.8
  }
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "task_id": 3
  }
}
```

### 7.2 获取 FVC 结果

**接口**：`GET /api/v1/extraction/fvc/{id}`

**说明**：获取 FVC 反演结果

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 1,
    "fvc_raster_path": "/data/fvc/fvc_001.tif",
    "mean_fvc": 0.65,
    "fvc_levels": {
      "low": 10.5,
      "medium": 45.2,
      "high": 44.3
    },
    "period_id": 1,
    "created_at": "2024-06-26T10:00:00Z"
  }
}
```

### 7.3 提交 USLE 侵蚀计算任务

**接口**：`POST /api/v1/extraction/erosion`

**说明**：提交土壤侵蚀计算任务

**请求头**：需要认证

**请求参数**：
```json
{
  "period_id": 1,
  "params": {
    "use_grass": true,
    "use_saga": true,
    "rainfall_data_path": "/data/rainfall/rainfall.tif"
  }
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "task_id": 4
  }
}
```

### 7.4 获取侵蚀计算结果

**接口**：`GET /api/v1/extraction/erosion/{id}`

**说明**：获取土壤侵蚀计算结果

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 1,
    "modulus_raster_path": "/data/erosion/modulus_001.tif",
    "class_raster_path": "/data/erosion/class_001.tif",
    "mean_modulus": 2500.5,
    "erosion_levels": {
      "slight": 20.0,
      "light": 30.0,
      "moderate": 25.0,
      "intense": 15.0,
      "severe": 10.0
    },
    "period_id": 1,
    "created_at": "2024-06-26T10:00:00Z"
  }
}
```

### 7.5 提交扰动图斑提取任务

**接口**：`POST /api/v1/extraction/disturbance`

**说明**：提交扰动图斑提取任务

**请求头**：需要认证

**请求参数**：
```json
{
  "period_id_before": 1,
  "period_id_after": 2,
  "params": {
    "change_threshold": 0.3,
    "min_area": 100.0,
    "use_otb": true
  }
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "task_id": 5
  }
}
```

### 7.6 获取扰动图斑结果

**接口**：`GET /api/v1/extraction/disturbance/{id}`

**说明**：获取扰动图斑提取结果

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": [
    {
      "id": 1,
      "disturbance_code": "DIST001",
      "type": "Construction",
      "geometry": {
        "type": "Polygon",
        "coordinates": [[[...]]]
      },
      "area": 5000.0,
      "perimeter": 300.0,
      "confidence": 0.95,
      "period_id": 2
    }
  ]
}
```

### 7.7 提交水保措施解译任务

**接口**：`POST /api/v1/extraction/measures`

**说明**：提交水保措施解译任务

**请求头**：需要认证

**请求参数**：
```json
{
  "period_id": 1,
  "params": {
    "use_ai": true,
    "model_path": "/data/models/measure_model.onnx"
  }
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "task_id": 6
  }
}
```

---

## 8. 综合评价接口

### 8.1 提交综合评价任务

**接口**：`POST /api/v1/evaluation`

**说明**：提交综合评价任务

**请求头**：需要认证

**请求参数**：
```json
{
  "period_id": 1,
  "params": {
    "use_ahp": true,
    "use_entropy": true,
    "ahp_matrix": [
      [1, 3, 5],
      [0.33, 1, 3],
      [0.2, 0.33, 1]
    ]
  }
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "task_id": 7
  }
}
```

### 8.2 获取评价结果

**接口**：`GET /api/v1/evaluation/{id}`

**说明**：获取综合评价结果

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 1,
    "compliance_score": 85.5,
    "compliance_level": "Good",
    "risk_score": 30.2,
    "risk_level": "Low",
    "indicator_weights": {
      "fvc": 0.3,
      "erosion": 0.4,
      "measure": 0.3
    },
    "indicator_values": {
      "fvc": 0.65,
      "erosion": 2500.0,
      "measure": 0.8
    },
    "period_id": 1,
    "created_at": "2024-06-26T10:00:00Z"
  }
}
```

---

## 9. 异常预警接口

### 9.1 获取异常列表

**接口**：`GET /api/v1/anomalies`

**说明**：获取异常报警列表（分页）

**请求头**：需要认证

**查询参数**：
- `page`：页码
- `page_size`：每页数量
- `project_id`：项目ID过滤（可选）
- `status`：状态过滤（可选）
- `severity`：严重程度过滤（可选）

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "items": [
      {
        "id": 1,
        "type": "DisturbanceViolation",
        "geometry": {
          "type": "Point",
          "coordinates": [116.5, 39.5]
        },
        "severity": "High",
        "description": "违规扰动",
        "status": "Pending",
        "assignee": null,
        "detected_at": "2024-06-26T10:00:00Z",
        "project_id": 1,
        "period_id": 1
      }
    ],
    "total": 5,
    "page": 1,
    "page_size": 20
  }
}
```

### 9.2 派单处置

**接口**：`POST /api/v1/anomalies/{id}/assign`

**说明**：派单处置异常

**请求头**：需要认证

**请求参数**：
```json
{
  "assignee": "user1",
  "measures": "现场核查，拍照取证"
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

### 9.3 结单

**接口**：`POST /api/v1/anomalies/{id}/resolve`

**说明**：结单异常

**请求头**：需要认证

**请求参数**：
```json
{
  "resolution": "已整改，符合要求"
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

---

## 10. 监测报告接口

### 10.1 获取报告列表

**接口**：`GET /api/v1/reports`

**说明**：获取监测报告列表（分页）

**请求头**：需要认证

**查询参数**：
- `page`：页码
- `page_size`：每页数量
- `project_id`：项目ID过滤（可选）
- `type`：报告类型过滤（可选）

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "items": [
      {
        "id": 1,
        "type": "Monthly",
        "title": "2024年6月监测报告",
        "docx_path": "/data/reports/report_001.docx",
        "pdf_path": "/data/reports/report_001.pdf",
        "is_filed": false,
        "project_id": 1,
        "created_at": "2024-06-26T10:00:00Z"
      }
    ],
    "total": 3,
    "page": 1,
    "page_size": 20
  }
}
```

### 10.2 生成报告

**接口**：`POST /api/v1/reports`

**说明**：生成监测报告

**请求头**：需要认证

**请求参数**：
```json
{
  "project_id": 1,
  "period_ids": [1, 2],
  "type": "Monthly",
  "title": "2024年6月监测报告",
  "template_path": "/data/templates/report_template.docx"
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 2,
    "task_id": 8
  }
}
```

### 10.3 下载报告

**接口**：`GET /api/v1/reports/{id}/download`

**说明**：下载报告文件

**请求头**：需要认证

**查询参数**：
- `format`：文件格式（docx/pdf），默认 pdf

**响应**：报告文件流

### 10.4 标记备案

**接口**：`POST /api/v1/reports/{id}/file`

**说明**：标记报告已备案

**请求头**：需要认证

**请求参数**：
```json
{
  "filing_number": "BA2024001",
  "filing_date": "2024-06-26"
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

---

## 11. 异步任务接口

### 11.1 获取任务列表

**接口**：`GET /api/v1/tasks`

**说明**：获取异步任务列表（分页）

**请求头**：需要认证

**查询参数**：
- `page`：页码
- `page_size`：每页数量
- `status`：状态过滤（可选）
- `type`：任务类型过滤（可选）

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "items": [
      {
        "id": 1,
        "type": "FvcExtraction",
        "status": "Completed",
        "progress": 100,
        "result_path": "/data/fvc/fvc_001.tif",
        "error_message": null,
        "submitted_at": "2024-06-26T09:00:00Z",
        "started_at": "2024-06-26T09:00:01Z",
        "completed_at": "2024-06-26T09:05:00Z",
        "project_id": 1,
        "period_id": 1
      }
    ],
    "total": 10,
    "page": 1,
    "page_size": 20
  }
}
```

### 11.2 获取任务详情

**接口**：`GET /api/v1/tasks/{id}`

**说明**：获取异步任务详情

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "id": 1,
    "type": "FvcExtraction",
    "status": "Completed",
    "progress": 100,
    "params": {
      "image_id": 1,
      "period_id": 1
    },
    "result_path": "/data/fvc/fvc_001.tif",
    "error_message": null,
    "submitted_at": "2024-06-26T09:00:00Z",
    "started_at": "2024-06-26T09:00:01Z",
    "completed_at": "2024-06-26T09:05:00Z",
    "retry_count": 0,
    "project_id": 1,
    "period_id": 1
  }
}
```

### 11.3 重试任务

**接口**：`POST /api/v1/tasks/{id}/retry`

**说明**：重试失败的任务

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

### 11.4 删除任务

**接口**：`DELETE /api/v1/tasks/{id}`

**说明**：删除任务记录

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

---

## 12. 数据看板接口

### 12.1 获取看板数据

**接口**：`GET /api/v1/dashboard`

**说明**：获取数据看板汇总数据

**请求头**：需要认证

**查询参数**：
- `project_id`：项目ID（可选，不传则汇总所有项目）

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "project_count": 5,
    "period_count": 20,
    "image_count": 100,
    "anomaly_count": 10,
    "task_count": 50,
    "fvc_stats": {
      "mean": 0.65,
      "trend": [0.6, 0.62, 0.65, 0.68, 0.65]
    },
    "erosion_stats": {
      "mean_modulus": 2500.0,
      "trend": [2800, 2600, 2500, 2400, 2500]
    },
    "compliance_stats": {
      "mean_score": 85.5,
      "level_distribution": {
        "Excellent": 2,
        "Good": 2,
        "Fair": 1
      }
    }
  }
}
```

### 12.2 获取工作台数据

**接口**：`GET /api/v1/dashboard/workbench`

**说明**：获取工作台数据（当前用户的任务、异常等）

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "pending_tasks": [
      {
        "id": 1,
        "type": "FvcExtraction",
        "status": "Running",
        "progress": 60
      }
    ],
    "pending_anomalies": [
      {
        "id": 1,
        "type": "DisturbanceViolation",
        "severity": "High",
        "description": "违规扰动"
      }
    ],
    "recent_reports": [
      {
        "id": 1,
        "title": "2024年6月监测报告",
        "created_at": "2024-06-26T10:00:00Z"
      }
    ]
  }
}
```

---

## 13. QGIS 服务接口

### 13.1 发布图层

**接口**：`POST /api/v1/qgis/publish`

**说明**：发布图层到 QGIS Server

**请求头**：需要认证

**请求参数**：
```json
{
  "layer_name": "fvc_layer",
  "layer_type": "raster",
  "data_path": "/data/fvc/fvc_001.tif",
  "style_path": "/data/styles/fvc_style.qml",
  "project_id": 1
}
```

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": {
    "service_name": "fvc_layer",
    "service_url": "http://qgis-server:8080/wms?fvc_layer"
  }
}
```

### 13.2 获取服务列表

**接口**：`GET /api/v1/qgis/services`

**说明**：获取已发布的 QGIS 服务列表

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": [
    {
      "name": "fvc_layer",
      "type": "raster",
      "url": "http://qgis-server:8080/wms?fvc_layer",
      "project_id": 1,
      "created_at": "2024-06-26T10:00:00Z"
    }
  ]
}
```

### 13.3 删除服务

**接口**：`DELETE /api/v1/qgis/services/{name}`

**说明**：删除 QGIS 服务

**请求头**：需要认证

**响应示例**：
```json
{
  "code": 200,
  "message": "success",
  "data": null
}
```

---

## 14. 健康检查接口

### 14.1 健康检查

**接口**：`GET /health`

**说明**：检查服务健康状态

**响应示例**：
```json
{
  "status": "healthy",
  "timestamp": "2024-06-26T10:00:00Z",
  "checks": {
    "database": "ok",
    "redis": "ok",
    "gdal": "ok"
  }
}
```

### 14.2 指标导出

**接口**：`GET /metrics`

**说明**：导出 Prometheus 指标

**响应**：Prometheus 文本格式指标

---

**文档版本**：v1.0  
**最后更新**：2026-06-26
