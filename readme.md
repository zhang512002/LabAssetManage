## 📁 项目目录结构

```text
LabAssetManage/
│
├── MyLabAsset.sln                      # 总解决方案文件 (包含全部工程)
├── CommonSettings.props                # 全局属性表 (统一配置头文件与库路径)
├── README.md                           # 项目说明文档
│
├── commonlibdll/                       # [依赖仓库] 统一管理所有自研模块的产物
│   │
│   ├── module_database/                # 数据库底层模块产物
│   │   ├── bin/                        # ├── 存放 module_database.dll
│   │   ├── include/                    # ├── 存放 databasemanager.h, enums.h 等公共头文件
│   │   └── lib/                        # └── 存放 module_database.lib
│   │
│   ├── module_borrow/                  # 借阅业务模块产物
│   │   ├── bin/                        # ├── 存放 module_borrow.dll
│   │   ├── include/                    # ├── 存放 BorrowManager.h 等对外接口
│   │   └── lib/                        # └── 存放 module_borrow.lib
│   │
│   ├── module_equipment/               # 设备业务模块产物
│   │   ├── bin/
│   │   ├── include/
│   │   └── lib/
│   │
│   ├── module_maintenance/             # 维保业务模块产物
│   │   ├── bin/
│   │   ├── include/
│   │   └── lib/
│   │
│   └── module_statistics/              # 统计导出模块产物
│       ├── bin/
│       ├── include/
│       └── lib/
│
│ -------------------------------------- (源码开发区与产物区分离) --------------------------------------
│
├── module_database/                    # [源码] 数据库模块工程
│   ├── module_database.vcxproj         # VS 工程文件
│   ├── databasemanager.h / .cpp        # 数据库连接与建表逻辑
│   ├── enums.h                         # 全局枚举定义
│   └── x64/                            # VS 中间编译产物 (可随时删除)
│
├── module_borrow/                      # [源码] 借阅业务模块工程
│   ├── module_borrow.vcxproj
│   ├── BorrowManager.h / .cpp          # 核心借阅逻辑 (已与 UI 彻底解耦)
│   └── x64/
│
├── module_equipment/                   # [源码] 设备业务模块工程
│   └── ...
│
├── module_maintenance/                 # [源码] 维保业务模块工程
│   └── ...
│
├── module_statistics/                  # [源码] 统计导出模块工程
│   └── ...
│
└── LabAssetUI/                         # [源码] UI 主界面工程 (最终宿主程序)
    ├── LabAssetUI.vcxproj              # VS 工程文件 (通过属性表链接 commonlibdll)
    ├── main.cpp                        # 程序启动入口
    ├── mainwindow.h / .cpp / .ui       # 主框架 (左侧导航栏 + QStackedWidget)
    │
    ├── ui_borrow/                      # (界面组件) 借阅相关界面
    │   ├── borrowpage.h / .cpp / .ui   # (重构自原项目，剥离了业务逻辑)
    │   └── borrowdialog.h / .cpp / .ui
    │
    ├── ui_equipment/                   # (界面组件) 设备相关界面
    │   └── ...
    │
    └── x64/                            # 最终输出目录 (运行此处的 exe)