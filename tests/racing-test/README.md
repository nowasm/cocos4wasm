# Racing Test — 引擎能力测试（赛车小游戏）

用一个可玩的赛车 demo 验证引擎的基础游戏开发能力闭环：外部资产加载（USD）→ 渲染 →
键盘输入 → 每帧变换 → 游戏循环 → 相机跟随。不启用引擎物理（街机运动学自积分）。

## 玩法

- `W/S`（或 ↑/↓）油门 / 刹车（低速时刹车转倒车）
- `A/D`（或 ←/→）转向（速度越快转向越钝）
- `R` 回到起跑线，`ESC` 退出
- 逆时针沿环形赛道跑，过线自动计圈，圈速写日志（含最佳圈）
- 冲出路面会明显减速（离路阻力 ×5）

## 构建（Windows）

```powershell
cd tests/racing-test
cmake -B build -A x64
cmake --build build --config Release --parallel
# 产物: build/Release/CocosRacingTest.exe（资产在 post-build 拷到 exe 旁）
```

## 资产

`assets/race.usda` 来自 [Kenney Car Kit](https://kenney.nl/assets/car-kit)（CC0）的
`race.glb`，用 `tools/glb2usda.js` 转换：

```powershell
node tools/glb2usda.js path/to/model.glb assets/
```

转换器覆盖 Kenney 式简单资产：平铺节点树（TRS）、三角网格
POSITION/NORMAL/TEXCOORD_0、单 pbrMetallicRoughness 材质（内嵌或外链贴图）。
注意输出里 `doubleSided` 必须带 `uniform` 前缀（tinyusdz 严格校验）。
车轮是独立 USD 节点（`wheel_front_left` 等），运行时按名字找到并做转向/滚动。

## 无人值守验证

引擎日志在 Windows 只走 OutputDebugString（无 stdout），窗口输入只认前台
（RawInputHook 有前台闸），锁屏时截图全黑、present 被节流——所以自动验证走
代码内自动驾驶：

```powershell
$env:COCOS_AUTODRIVE = "1"   # P 控制器自动沿赛道跑圈，每 2s 输出状态日志
.\build\Release\CocosRacingTest.exe
```

配套工具（`tools/`）：

- `dbglisten.ps1` — 抓 OutputDebugString（DBWIN 协议）到文件：
  `powershell -File tools/dbglisten.ps1 -OutFile dbg.txt -Seconds 60`
- `snapwin.ps1` / `snapfg.ps1` — 窗口截图（PrintWindow / 前台+屏幕拷贝）
- `drivekeys.ps1` — 注入扫描码按键模拟驾驶（需要目标窗口能拿到前台）：
  `powershell -File tools/drivekeys.ps1 -Title "Cocos Racing" -Script "W:5000,W+A:2000"`

验证判据：日志出现 `RaceWorld: car loaded — 5 renderers, wheels found`（USD 资产
链路 OK）与 `RaceWorld: LAP n done in x.xxs`（运动学+计圈闭环 OK）。
