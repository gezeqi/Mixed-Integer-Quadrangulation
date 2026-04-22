# MIQ — Mixed-Integer Quadrangulation 复现

[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](./LICENSE) ![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey) ![libigl](https://img.shields.io/badge/libigl-v2.5.0-green)

一个从三角网格到纯四边形网格的完整 pipeline 复现，基于：

- **Bommes, Zimmer, Kobbelt**, *Mixed-Integer Quadrangulation*, SIGGRAPH 2009
- **Ebke, Bommes, Campen, Kobbelt**, *QEx: Robust Quad Mesh Extraction*, SIGGRAPH Asia 2013

整个流程分三阶段：

```
  三角网格 (V, F)
     │
     ▼
  [01] NRosy 4-RoSy Cross Field          — igl::copyleft::comiso::nrosy
     │
     ▼
  [02] Mixed-Integer Seamless UV         — igl::copyleft::comiso::miq
     │   (greedy rounding, 论文核心贡献)
     ▼
  [03] Quad Mesh Extraction              — libQEx (qex_extractQuadMesh)
     │
     ▼
  quad_out.obj (纯 quad)
```

---

## 1. 环境要求

| 项 | 版本/备注 |
|---|---|
| OS | Windows 10/11 x64 |
| 编译器 | Visual Studio 2022 (MSVC v143) |
| CMake | ≥ 3.16；**支持 4.x**（项目已内置兼容 shim） |
| Git | 任意近期版本 |
| vcpkg | 任意近期版本，需要 `openblas` + `openmesh` 两个包 |
| 磁盘 | build 目录约 2–3 GB（libigl + CoMISo + libQEx 全家桶） |
| 网络 | 首次 configure 要 FetchContent 拉 libigl v2.5.0 + CoMISo + libQEx |

本项目已在 Windows 11 + VS 2022 + CMake 4.2.1 + vcpkg 下验证通过。

---

## 2. 一次性环境配置

### 2.1 安装 vcpkg 依赖

假设 vcpkg 装在 `D:\repo\vcpkg`（没有就 `git clone https://github.com/microsoft/vcpkg` 然后 `bootstrap-vcpkg.bat`）。

```powershell
D:\repo\vcpkg\vcpkg install openblas:x64-windows
D:\repo\vcpkg\vcpkg install openmesh:x64-windows
```

- **openblas**：修复 CoMISo 自带的 2014 年 MinGW OpenBLAS 在新版 Windows 上会 `0xC0000005` 崩溃的问题。
- **openmesh**：libQEx 的唯一依赖（vcpkg 提供 `OpenMeshCore` / `OpenMeshTools` 两个 CMake target）。

两个包加起来约 5–10 分钟。

### 2.2 克隆本项目

```powershell
cd C:\Users\<你>\Desktop\repo\Research\Projects
git clone <本仓库 URL> miq
cd miq
```

或者本项目已在 `Projects/miq` 下，跳过这步。

---

## 3. 编译

### 3.1 配置（**首次必须传 vcpkg toolchain**）

```powershell
cd miq
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=D:\repo\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows
```

第一次 configure 需要 5–15 分钟：CMake 会 FetchContent 拉取

- `libigl v2.5.0`（最后一个还带 `copyleft/comiso/miq` 的 tag；v2.6+ 已删除）
- CoMISo、gmm、Eigen、glfw、imgui、imguizmo（libigl 的传递依赖）
- `libQEx` master (commit `c0a4dc6`)

一切缓存在 `build/_deps/` 下，以后只要不删 `build` 就不再重下。

### 3.2 编译

```powershell
# 全量
cmake --build . --config Release -j

# 或只编某一 target
cmake --build . --config Release --target 01_cross_field -j
cmake --build . --config Release --target 02_miq -j
cmake --build . --config Release --target 03_quad_extract -j
```

预计首次全量编译 10–20 分钟（CoMISo + libigl + libQEx + OpenMesh 相关转发）。

### 3.3 产物位置

```
build/Release/
├── 01_cross_field.exe
├── 02_miq.exe
├── 03_quad_extract.exe
├── data/bumpy.off                   ← 由 copy_data target 自动拷入
├── libopenblas.dll + 其它 vcpkg dll ← 由 CMake POST_BUILD 自动部署
└── ...
```

---

## 4. 使用

### 4.1 数据

默认使用 `data/bumpy.off`（libigl 官方示例网格，约 3.4k faces）。替换成你自己的网格：

```powershell
# 把任意 OBJ 或 OFF 放进 data/，例如 mymodel.obj
.\01_cross_field.exe data\mymodel.obj
```

测试过的网格规模：3k – 50k 三角形均可，再大会让 MIQ 的混合整数求解器变慢。

### 4.2 Stage 1 — Cross Field 可视化

```powershell
cd build\Release
.\01_cross_field.exe                         # 用默认 bumpy.off
.\01_cross_field.exe data\3holes.off         # 指定网格
```

预期：viewer 中网格表面布满**红色 + 蓝色十字线段**，方向光滑变化，少数位置是奇异点（方向突变）。控制台会打印 `Singularities (nonzero S): N`。

### 4.3 Stage 2 — MIQ 参数化

```powershell
.\02_miq.exe                                 # 用默认 bumpy.off
.\02_miq.exe data\mymodel.obj
```

预期：控制台打印：

```
[trace] calling miq...
INITIALIZED SPARSE MATRIX OF ... 
# integer    variables: NN
# continuous variables: MMMM
...greedy rounding 日志...
[trace] miq done; UV=... FUV=...
[trace] launching viewer
```

然后 viewer 弹出，网格表面贴**棋盘格纹理**，格线对齐 cross field。按 `1` 切换到 UV 2D 视图。

### 4.4 Stage 3 — Quad 提取与导出

```powershell
.\03_quad_extract.exe                        # 输出到 quad_out.obj
.\03_quad_extract.exe data\mymodel.obj myout.obj
```

**第二个参数就是输出 OBJ 路径**（相对当前工作目录）。

预期：控制台打印：

```
[trace] MIQ: V=... F=... UV=... FUV=...
[trace] calling qex_extractQuadMesh...
[trace] QEx done: V_quad=... F_quad=...
[trace] wrote quad_out.obj
```

然后 viewer 弹出显示 quad mesh：浅色填充 + 黑色 quad 边。

#### 导出的 OBJ 格式

写出的是**原生 quad**（每个面 4 个索引），不是三角化：

```
v 1.234 5.678 9.012
v ...
f 1 2 3 4
f 2 5 6 3
...
```

可以用 **Blender**（推荐）、**MeshLab**、**ZBrush**、**Houdini** 等打开，都会识别为 quad 拓扑。

---

## 5. Viewer 快捷键速查

| 键 | 作用 |
|---|---|
| `1` / `2` | 切换 3D mesh / UV 2D 视图（仅 02） |
| `L` | 切换 wireframe（03 会暴露三角化的假对角线） |
| `;` | 顶点编号 |
| `:` | 面编号 |
| 鼠标左拖 | 旋转 |
| 右拖 / 滚轮 | 缩放 |
| Esc | 退出 |

---

## 6. 调参指南

### 6.1 `02_miq.cpp` 里的关键参数

```cpp
const double gradient_size  = 50.0;   // quad 密度，越大 quad 越小越密
const double stiffness      = 5.0;    // （保留参数，源码里实际未使用）
const bool   direct_round   = false;  // false = greedy 逐变量整数化（论文核心）
                                      // true  = 一次性全部 round（快但糟）
const unsigned int iter     = 0;      // stiffness 迭代（0 = 不做）
const unsigned int local_iter = 5;    // 每次整数化内的 Poisson 次数
const bool   do_round       = true;   // false = 跳过整数化（对比实验用）
const bool   singular_round = true;   // singularity 坐标是否 snap 整数
```

### 6.2 推荐实验

| 实验 | 改动 | 观察 |
|---|---|---|
| 密度 | `gradient_size = 20` | quad 大而稀 |
| 密度 | `gradient_size = 150` | quad 小而密；求解时间↑ |
| **核心对比** | `do_round = false` | UV 接缝对不齐；QEx 可能返回 `F_quad=0` — 这就证明了 MIQ 论文的整数约束是必要的 |
| 算法对比 | `direct_round = true` | 求解快 5–10×，但 quad 质量差、可能出现退化面 |

### 6.3 约束方向（高级）

默认代码把 face 0 的方向约束到 `(1, 0, 0)`，选面很随意。要对齐到特征边：

```cpp
// 在 02 或 03 里替换这两行：
VectorXi b(N); b << face0_id, face1_id, ...;
MatrixXd bc(N, 3);
bc << v0x, v0y, v0z,
      v1x, v1y, v1z, ...;
```

一种更好的做法是从 mesh 的 sharp feature 边自动提取约束（超出本项目范围，参考 Directional 库）。

---

## 7. 故障排查

### 7.1 运行时 `找不到 libopenblas.dll`

CMake 的 `POST_BUILD` 没把 DLL 复制到 exe 旁边。手动补：

```powershell
Copy-Item D:\repo\vcpkg\installed\x64-windows\bin\*.dll build\Release\ -Force
Copy-Item build\Release\openblas.dll build\Release\libopenblas.dll
```

### 7.2 `02_miq.exe` 退出码 `0xC0000005`（访问违规）

没用 vcpkg 版 OpenBLAS。确认：

1. `cmake ..` 时传了 `-DCMAKE_TOOLCHAIN_FILE=D:\repo\vcpkg\scripts\buildsystems\vcpkg.cmake`
2. `build\Release\libopenblas.dll` 文件大小应该在 **30 MB 左右**（vcpkg 版），不是 8 MB（MinGW 老版）

删 CMakeCache.txt 重来：

```powershell
cd build; Remove-Item CMakeCache.txt; cmake .. -DCMAKE_TOOLCHAIN_FILE=...
```

### 7.3 CMake 4.x 报 `Invalid CMAKE_POLICY_VERSION_MINIMUM`

已在顶层 `CMakeLists.txt` 写死 `set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)`，正常情况下不会撞。如果还撞，环境变量兜底：

```powershell
$env:CMAKE_POLICY_VERSION_MINIMUM="3.5"; cmake ..
```

### 7.4 编译 libQEx 报 `_USE_MATH_DEFINES` 或 `dllimport` 未解析

项目已在 CMake 里给 QExStatic / 03 target 补 `-D_USE_MATH_DEFINES -DNOMINMAX -DDLLEXPORT=`。如果你自己改 CMake 打掉了，这些坑会回来。

### 7.5 Stage 3 输出 `F_quad=0`

QEx 判定 MIQ 的 UV 不够 seamless。常见原因：

- `do_round = false` 跑的（故意的对比实验，不是 bug）
- `gradient_size` 太小（< 10），导致整数格点太稀
- 网格质量差（skinny triangle 很多、非流形）

先把 `gradient_size` 调回 30–50，确认 Stage 2 的棋盘格对齐。

### 7.6 FetchContent 拉不下来 libigl / libQEx

网络问题。给 Git 设代理：

```powershell
git config --global http.proxy http://127.0.0.1:<port>
git config --global https.proxy http://127.0.0.1:<port>
```

或者手动 clone 到 `build/_deps/libigl-src` / `build/_deps/libqex-src` 后重跑 cmake。

---

## 8. 项目结构

```
miq/
├── README.md                        ← 本文件
├── CMakeLists.txt                   ← 顶层，定义 3 个 target + DLL 部署
├── cmake/
│   └── libigl.cmake                 ← FetchContent libigl v2.5.0 + libQEx + 打 patch
├── src/
│   ├── 01_cross_field.cpp           ← Stage 1: NRosy cross field
│   ├── 02_miq.cpp                   ← Stage 2: Mixed-Integer 参数化
│   └── 03_quad_extract.cpp          ← Stage 3: libQEx 提取 quad
├── data/
│   ├── README.md
│   └── bumpy.off                    ← 默认测试网格
└── build/                           ← CMake 生成，可随时删
```

---

## 9. 关键技术决策（为什么是这样）

### 为什么钉住 libigl v2.5.0

**libigl v2.6.0 (2024) 删除了整个 `include/igl/copyleft/comiso/` 目录**，`miq.h`、`nrosy.h`、`frame_field.h` 以及 tutorial `505_MIQ` 全没了。v2.5.0 是最后一个还完整带 MIQ 的 release tag。

如果想跟上 libigl 主线，备选方案是迁到 [**Directional**](https://github.com/avaxman/Directional)（同一作者圈子维护的继任者）。

### 为什么 libQEx 而不是自写 extractor

libQEx 是 Bommes 团队自己的 SIGGRAPH Asia 2013 论文代码，**专门**为 MIQ 输出设计，处理 T-junction / degenerate / non-manifold 情况已经工业级成熟。自写 200 行版本只能处理最干净的输入，不值得。

代价：多一个 OpenMesh 依赖 + libQEx 老 CMake 写法需要打几处 patch（已固化在 `cmake/libigl.cmake`）。

### 为什么 vcpkg OpenBLAS 替换 CoMISo 自带版

CoMISo 的 `ext/OpenBLAS-v0.2.14-Win64-int64/` 是 2014 年 MinGW 编的预编译包。它在新版 Windows 10/11 + 新版 MSVC CRT 环境里加载，调用 DGEMM/Cholmod factorization 时会触发段错误（已验证退出码 `0xC0000005`）。

vcpkg 版 openblas（现代 MSVC 编译）ABI 兼容，覆盖后 MIQ 主循环稳定运行。

---

## 10. 参考资源

- **论文**：
  - [Bommes et al. 2009, *Mixed-Integer Quadrangulation*](https://www.graphics.rwth-aachen.de/publication/44/)
  - [Ebke et al. 2013, *QEx: Robust Quad Mesh Extraction*](https://www.graphics.rwth-aachen.de/publication/03287/)

- **代码**：
  - [libigl v2.5.0](https://github.com/libigl/libigl/tree/v2.5.0)（tutorial 505_MIQ 是本项目的起点）
  - [libQEx](https://github.com/hcebke/libQEx)
  - [CoMISo](https://www.graphics.rwth-aachen.de/software/comiso/)（由 libigl 自动拉取，一般无需直接接触）

- **工具**：
  - [Blender](https://www.blender.org/) — 查看 quad OBJ、对比原始三角网格
  - [MeshLab](https://www.meshlab.net/) — 轻量级网格查看/比较

---

## 11. 下一步可玩

- **约束边 picking**：libigl viewer 支持鼠标选面/边，做成交互式指定 cross field 约束
- **从 sharp feature 自动提取约束**：检测折边（dihedral > 阈值），自动生成约束集
- **对比 direct_round vs greedy rounding**：跑两份，把两个 quad OBJ 放进 Blender 叠看，论文 Figure 4 的复现
- **更大模型**：试试 armadillo / dragon（约 40k faces），观察求解时间与 singularity 分布
- **迁移到 Directional**：同样的 pipeline 能在现代 libigl 生态里跑起来，API 更现代

---

## 12. License

本项目采用 **GPL-3.0-or-later**。详见 [`LICENSE`](./LICENSE)。

为什么是 GPL：CoMISo 和 libQEx 都是 GPL-3.0，链接它们的组合工程必须继承 GPL（传递性要求）。如果想要更宽松的下游使用条件，需要把这两个组件替换成非 GPL 的方案（工作量非常大）。

完整 GPL-3.0 条款可通过下载获取：

```powershell
# PowerShell
Invoke-WebRequest https://www.gnu.org/licenses/gpl-3.0.txt -OutFile COPYING
```

## 致谢

- libigl 团队 — 提供了 MIQ 的 C++ 接口和完整的 tutorial 505_MIQ
- Bommes et al. — 原论文 MIQ，SIGGRAPH 2009
- Ebke et al. — libQEx，SIGGRAPH Asia 2013
- CoMISo / RWTH Aachen — 混合整数二次规划求解器
