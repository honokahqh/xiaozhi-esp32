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
                添加接口    
                    GetNetworkRssi
                    GetNetWorkSsid
        +   wifi_board.cc
        +   wifi_board.h
                实现接口
                    GetNetworkRssi
                    GetNetWorkSsid
        m   esp_box3_board.cc 
                替换初始化
                    SpiLcdDisplay -> HonokaSpiLcdDisplay

    app
        +   application.cc
                添加调用
                    SetMainPanel

