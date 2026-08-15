# 智能记忆帽 · 多视角视觉记忆辅助系统

面向“空间性遗忘”问题的可穿戴视觉记忆辅助系统。帽体采集周围画面，云端做语义理解与向量检索，可以用自然语言查询，也可以为关键物品设置主动提醒。

---

## 目录

- [项目介绍](#项目介绍)
- [功能说明](#功能说明)
- [系统架构](#系统架构)
- [技术栈](#技术栈)
- [目录结构](#目录结构)
- [快速开始](#快速开始)
- [API 说明](#api-说明)
- [数据与存储](#数据与存储)
- [模型说明](#模型说明)
- [隐私说明](#隐私说明)


---

## 项目介绍

系统解决的是“看见过、放过、经过过，但后来想不起来”的问题。帽体自动采集周围画面，云端完成特征提取和向量入库；可以像对话一样提问，系统返回答案并附带历史画面作为依据。同时，可以为药盒、钥匙、证件等关键物品录入提醒，当摄像头再次拍到该物品时触发提示。

适用群体包括阿尔茨海默患者、记忆衰退老人、家庭照护场景，以及任务切换频繁、容易忘记物品位置或行为是否完成的普通用户。

## 功能说明

### 1. 视觉记忆检索

用于“忘记之后找回信息”。

流程：帽体采集场景图像 → 云端语义理解与向量入库 → 自然语言提问 → 返回回答与图片证据。

### 2. 主动记忆提醒

用于“关键物品再次出现时主动提示”。

流程：录入目标物品与提醒内容 → 目标向量入库 → 后续图像相似度匹配 → 超过阈值后语音提醒。

### 设计要点

- **多视角采集**：前、后、左、右四路摄像头，减少单一前向视角的盲区。
- **语义化记忆**：用 CLIP 向量编码和 ChromaDB 检索，把图片转化为可自然语言查询的记忆单元。
- **目标触发提醒**：录入目标物品后，系统在目标再次出现时触发提醒，无需给物品贴标签。

## 系统架构

```text
ESP32-CAM 四路摄像头（渔夫帽）
        │  采集画面帧
        ▼
FastAPI 后端  /upload_frame
        │  CLIP 提取视觉特征
        ▼
ChromaDB 向量库  ◄── 本地图片文件（memory_images）
        ▲
        │  /query_memory 向量检索
        ▼
通义千问 qwen-vl-max（视觉理解 + 语言生成）
        │
        ▼
前端网页端：展示回答与图片证据 / 录入提醒 / 上传画面
```

- 前端调用 `/query_memory` 发起提问。
- 后端用 CLIP 把问题编码为向量，在 ChromaDB 中按时间范围检索最匹配的历史画面。
- 把检索到的图片与问题一起交给通义千问多模态模型生成回答。
- 前端展示回答，并预留图片证据展示区域。

## 技术栈

- **后端**：FastAPI、Uvicorn、ChromaDB、sentence-transformers（CLIP）、OpenAI SDK（通义千问 VL）、Pillow、Pydantic。
- **前端**：原生 HTML / CSS / JavaScript，无构建依赖，移动端友好。
- **硬件**：ESP32-CAM、Arduino、Blinker（点灯科技）、巴法云图床。

## 目录结构

```text
smart-memory-hat/
├── README.md
├── .gitignore
├── LICENSE                           # MIT 开源许可证
├── backend/                          # 后端服务
│   ├── main.py                       # FastAPI 入口
│   ├── requirements.txt
│   ├── memory_db/                    # Chroma 数据库缓存（运行时生成）
│   ├── visual_memory_db/             # 视觉记忆向量库缓存（运行时生成）
│   └── memory_images/                # 采集画面图片数据（运行时生成）
├── frontend/                         # 网页端
│   ├── index.html
│   ├── css/
│   │   └── style.css
│   └── js/
│       └── app.js
└── hardware/                         # 硬件端
    ├── esp32cam/                     # 主程序源码
    ├── camera_webserver/             # 摄像头 Web 服务（开发过程）
    └── cam_test/                     # 摄像头采集测试
```

## 快速开始

### 环境要求

- Python 3.9+
- 可访问 Hugging Face 镜像源（用于首次下载 CLIP 模型）
- 阿里云百炼 DashScope API Key
- Arduino IDE 与 ESP32 开发环境（仅硬件端需要）

### 1. 后端运行

```bash
cd backend
pip install -r requirements.txt
# 设置你的 DashScope API Key（使用环境变量，不要提交真实密钥）
export DASHSCOPE_API_KEY=sk-你的密钥
# 也可以参考 backend/.env.example 创建 backend/.env
python main.py
```

服务默认监听 `http://0.0.0.0:8000`。首次运行会自动下载 `clip-ViT-B-32` 模型，可能耗时较长。

### 2. 前端运行

前端 API 地址默认写在 `frontend/js/app.js` 顶部的 `API_BASE` 常量中（`http://127.0.0.1:8000`），可按需修改。

方式一：直接用浏览器打开 `frontend/index.html`。

方式二：在 `frontend` 目录启动静态服务：

```bash
cd frontend
python -m http.server 5500
```

然后访问 `http://127.0.0.1:5500`。后端已开启 CORS，允许前端跨域请求。

### 3. 硬件烧录

使用 Arduino IDE 打开 `hardware/esp32cam/32cam/32cam.ino`，配置摄像头型号与引脚后编译烧录。当前硬件端为单相机验证版本，四路协调采集尚在开发中。

## API 说明

### `POST /upload_frame`

接收一帧画面，提取视觉特征并写入视觉记忆库。

- 请求：`multipart/form-data`，字段 `file`（图片文件）。
- 响应示例：

```json
{
  "status": "success",
  "message": "画面已成功写入记忆库",
  "id": "frame_1720000000000"
}
```

### `POST /query_memory`

按问题与时间范围检索视觉记忆，并交给通义千问生成回答。

- 请求体：

```json
{
  "user_prompt": "我昨天把钥匙放在哪里了？",
  "start_time": 1720000000.0,
  "end_time": 1720086400.0
}
```

`start_time` / `end_time` 为 Unix 时间戳（秒）。

- 响应示例：

```json
{
  "answer": "根据检索到的历史画面，钥匙被放在……"
}
```

### `POST /add_reminder`

录入目标物品与提醒内容，用于主动记忆提醒。

## 数据与存储

- `backend/visual_memory_db/`：视觉记忆向量库（ChromaDB），保存特征向量、时间戳和图片路径。
- `backend/memory_db/`：预留的数据库目录，计划用于提醒目标向量。
- `backend/memory_images/`：采集画面的原始图片文件，作为回答的证据来源。


## 模型说明

- **视觉特征提取**：CLIP（`clip-ViT-B-32`），把画面编码为向量用于相似度检索。
- **视觉理解与语言生成**：通义千问 `qwen-vl-max`，通过阿里云百炼 DashScope 的 OpenAI 兼容接口调用。

> 当前英文 CLIP 对中文查询的语义对齐有限，后续计划替换为中文/多语言 CLIP 变体。

## 隐私说明

- 产品定位为个人视觉记忆辅助工具，不是监控设备。
- 涉及用户授权、采集开关、数据删除、加密传输和敏感场景提示。
- 后端 DashScope API Key 通过环境变量 `DASHSCOPE_API_KEY` 注入；硬件端 WiFi 账号密码、Blinker / 巴法云密钥均以 `YOUR_*` 占位符表示，运行前替换为你自己的配置。

