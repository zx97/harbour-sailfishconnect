kdeconnect_add_plugin(
    sailfishconnect_systemvolume
    JSON sailfishconnect_systemvolume.json
    SOURCES systemvolumeplugin.cpp)

target_link_libraries(sailfishconnect_systemvolume
PRIVATE
    kdeconnectcore
    KF5::I18n
    Qt5::DBus
)
