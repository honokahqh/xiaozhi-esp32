v1 
    display
        +   honoka_display.cc
        +   honoka_display.h
        +   ui_320x240
        +   display.h
                添加定义与变量   
                    ScreenPanelId
                    int panel_id_ = SCREEN_PANEL_LOADING;
    board
        +   board.h
                添加网络接口    
                    GetNetworkRssi
                    GetNetWorkSsid
                添加电池接口
                    
        +   wifi_board.cc
        +   wifi_board.h
                实现接口
                    GetNetworkRssi
                    GetNetWorkSsid
        m   esp_box3_board.cc 
                替换初始化
                    SpiLcdDisplay -> HonokaSpiLcdDisplay

        +   wifi_configuration_ble.cc
        +   wifi_configuration_ble.h
                增加ble配置WiFi功能

        +   battery_monitor.cc
        +   battery_monitor.h
                增加电池电量检测接口
                
    app
        +   application.cc
                添加调用
                    SetMainPanel
                添加调用
                    BatteryMonitor::GetInstance().Init();

