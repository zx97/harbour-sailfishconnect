kdeconnect_add_plugin(
    sailfishconnect_connectivityreport
    JSON sailfishconnect_connectivityreport.json
    SOURCES connectivityreportplugin.cpp)

target_link_libraries(sailfishconnect_connectivityreport
PRIVATE
    kdeconnectcore
    KF5::I18n
    Qt5::DBus
)
