# 0.49 地图物件规则与验证范围

本次处理用户截图中的三个真实规则缺口，保留原图坐标、阻挡和动画素材：

| 物件 | 原始类型 | 已接入行为 |
| --- | --- | --- |
| 盗贼之家 | 97 | 打开原版 TPRANK 情报界面，显示九项势力排名、并列旗帜、最强英雄及属性、最强生物；每次访问读取当前状态 |
| 海妖 | 92 | 每队损失 30%，向上取整；只有一名的队伍不受影响；按损失单位的最大生命值给予经验；同一英雄在战斗前访问其他海妖也不会再次损兵 |
| 灯塔 | 42 | 可占领和夺回；原版旗帜和小地图显示当前归属；所属玩家的全部航海英雄每日行动力 +500；多座可叠加；占领当日不凭空增加剩余行动力 |

海妖使用死亡阴影的战后重访行为，没有采用 HotA 的终身访问限制。各英雄独立保存访问标记；战斗结束后消耗该标记。发生升级时先完成技能选择，再展示访问结果。旧存档中类型 42 的通用物件名称会自动显示为“灯塔”。存档格式仍为 15。

城堡灯塔的每日航海加成也已接入；不复制原版城内灯塔错误地使敌方获益的缺陷。电脑可以选择占领地图灯塔。灯塔同时计入“占领所有矿山”的特殊胜利条件。行动力按英雄所属势力计算，修复了副英雄和电脑英雄每日航海行动力固定为 1500 的路径。

情报中的军队排名和最强英雄使用本项目现有战力估算，尚未声称与原版 AI Value 精确一致。“电脑策略”目前显示玩家／电脑身份，地图中的完整电脑性格与策略系统仍需接入。Learning／Navigation 等尚未支持的辅助技能不在本次补齐范围内。

## 验证

- 开发、通用发布、ASan／UBSan 构建的 12 组 CTest 回归通过。
- 新增地图服务规则检查：兵力向上取整、单兵保护、多队与亡灵、英雄间隔离、存读档、战后重新访问、灯塔叠加和势力隔离、副英雄过夜、城堡灯塔、特殊胜利、情报只读及旧物件命名。
- 对当前《行者无疆》第 7 天进度的副本执行三个真实物件的点击、返回、重复访问。海妖将两名穴居巨魔减为一名，获得 40 经验。
- 界面测试将英雄放在被测物件入口，并移除附近野兵，隔离访问规则；这不是完整地图通关测试。另在未移除野兵的当前进度副本上，实机验证海妖首次／重复访问并返回地图。
- 三种构建各输出 10 张地图／访问／情报截图，检查情报 Retina 显示、渲染不修改游戏状态，并读取灯塔旗帜像素验证颜色。
- 原存档 SHA-256 前后相同。验证记录位于本机 `data/native/validation-0.49.json` 与 `references/validation/0.49/`，个人存档不提交到 Git。
- macOS 14+ 通用应用包含 arm64／x86_64，包内 11413 个运行资源通过校验，不依赖源码目录。

## 仍未完成的范围

新增 `h3_map_object_audit` 直接探测真实的未实现分支，审计本机 15 张下载地图的 Site 物件。探测时清除守卫并给足资源，避免把“能开战”误认成“战后奖励规则已实现”。

73 类 Site 中仍有 41 类进入通用未实现分支，共 6814 个实例。分派已实现也不代表所有细节都符合原版；此数字不包括其他 ObjectType 的规则缺口。数量较多的后续项目包括山丘堡垒、野外巢穴、墓穴、生物宝屋、知识之树、竞技场、观测塔等。清单保存于 `data/native/map-site-audit-0.49.json`，本机可重新生成：

```sh
cmake --build build/native --target h3_map_object_audit
build/native/h3_map_object_audit data/native/map-site-audit-0.49.json
build/native/h3_map_service_tests /path/to/private-save-copy.json
build/native/HeroesIII.app/Contents/MacOS/HeroesIII \
  --map-service-tests /tmp/heroesiii-map-service-ui \
  --campaign-save /path/to/private-save-copy.json
```

## 规则核对来源

使用公开兼容引擎中的行为与资源元数据核对事实，相关引擎实现未复制到本项目：

- [VCMI 海妖与盗贼之家行为](https://github.com/vcmi/vcmi/blob/develop/lib/mapObjects/MiscObjects.cpp)
- [VCMI 灯塔对象类型及 500 航海加成配置](https://github.com/vcmi/vcmi/blob/develop/config/objects/lighthouse.json)
- [VCMI 原版情报界面资源与布局定义](https://github.com/vcmi/vcmi/blob/develop/client/windows/GUIClasses.cpp)
- [VCMI 情报统计字段](https://github.com/vcmi/vcmi/blob/develop/lib/gameState/SThievesGuildInfo.h)

2026-11-18 下载副本和 SHA-256 记录位于本机 `references/object-rules-0.49/`。
