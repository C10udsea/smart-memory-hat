"""智能记忆帽后端服务。

提供接口：
- POST /upload_frame  接收一帧画面，提取视觉特征并写入向量库
- POST /query_memory  按问题与时间范围检索历史画面，调用多模态模型生成回答
"""

import os
import base64
import io
import time

# 使用国内镜像源下载 Hugging Face 模型
os.environ['HF_ENDPOINT'] = 'https://hf-mirror.com'

from fastapi import FastAPI, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import chromadb
from sentence_transformers import SentenceTransformer
from PIL import Image
from openai import OpenAI

app = FastAPI()

# 允许前端页面跨域调用（前端与后端分开部署时使用）
# 本地开发阶段允许任意来源跨域；不使用 Cookie，因此关闭 credentials。
# 部署到公网时，建议把 allow_origins 改成明确的前端域名。
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)

# 视觉特征提取模型：将画面和查询文本编码到同一向量空间
embedding_model = SentenceTransformer('clip-ViT-B-32')

# 视觉记忆向量库：保存特征向量、时间戳和图片路径
chroma_client = chromadb.PersistentClient(path="./visual_memory_db")
collection = chroma_client.get_or_create_collection(name="user_memory")

# 多模态大模型：通义千问，负责视觉理解与语言生成
DASHSCOPE_API_KEY = os.getenv("DASHSCOPE_API_KEY")
if not DASHSCOPE_API_KEY:
    raise RuntimeError("请先设置环境变量 DASHSCOPE_API_KEY，例如：export DASHSCOPE_API_KEY=sk-xxx")

llm_client = OpenAI(
    api_key=DASHSCOPE_API_KEY,
    base_url="https://dashscope.aliyuncs.com/compatible-mode/v1",
)

# 查询请求体：问题文本 + 起止时间（Unix 秒）
class QueryRequest(BaseModel):
    user_prompt: str
    start_time: float
    end_time: float

@app.post("/upload_frame")
async def upload_frame(file: UploadFile = File(...)):
    """接收一帧画面，提取视觉特征并写入记忆库。"""

    image_data = await file.read()
    image = Image.open(io.BytesIO(image_data)).convert("RGB")

    feature_vector = embedding_model.encode(image).tolist()

    # 以毫秒时间戳作为该帧的唯一标识
    timestamp = time.time()
    frame_id = f"frame_{int(timestamp * 1000)}"

    # 保存原始图片，作为后续回答的证据
    os.makedirs("./memory_images", exist_ok=True)
    image_path = f"./memory_images/{frame_id}.jpg"
    image.save(image_path)

    # 写入向量库：向量 + 元数据
    collection.add(
        ids=[frame_id],
        embeddings=[feature_vector],
        metadatas=[{"timestamp": timestamp, "image_path": image_path}],
    )

    return {"status": "success", "message": "画面已成功写入记忆库", "id": frame_id}

@app.post("/query_memory")
async def query_memory(request: QueryRequest):
    """检索最相关的历史画面，并交给多模态模型生成回答。"""

    # 将查询问题编码为向量
    query_embedding = embedding_model.encode(request.user_prompt).tolist()

    # 在指定时间范围内检索最相似的一帧画面
    results = collection.query(
        query_embeddings=[query_embedding],
        n_results=1,
        where={
            "$and": [
                {"timestamp": {"$gte": request.start_time}},
                {"timestamp": {"$lte": request.end_time}},
            ]
        },
    )

    if not results or not results['metadatas'] or len(results['metadatas'][0]) == 0:
        return {"answer": "在指定的时间段内，我没有找到相关的视觉记忆。"}

    # 读取匹配到的图片并转为 Base64
    matched_image_path = results['metadatas'][0][0]['image_path']
    try:
        with open(matched_image_path, "rb") as image_file:
            base64_image = base64.b64encode(image_file.read()).decode('utf-8')
    except Exception as e:
        return {"answer": f"抱歉，找到了记忆记录，但无法读取本地图片文件：{str(e)}"}

    # 组装图文消息，调用通义千问生成回答
    response = llm_client.chat.completions.create(
        model="qwen-vl-max",
        messages=[
            {
                "role": "system",
                "content": "你是智能记忆帽的视觉记忆助手。请基于检索到的历史画面，客观、准确地回答用户的问题，不要编造画面中不存在的信息。",
            },
            {
                "role": "user",
                "content": [
                    {"type": "text", "text": f"用户提问：{request.user_prompt}"},
                    {"type": "image_url", "image_url": {"url": f"data:image/jpeg;base64,{base64_image}"}},
                ],
            },
        ],
    )

    return {"answer": response.choices[0].message.content}

if __name__ == "__main__":
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=8000)