kdeconnect_add_plugin(
    sailfishconnect_receivenotifications
    JSON sailfishconnect_receivenotifications.json
    SOURCES receivenotificationsplugin.cpp)

find_package(PkgConfig REQUIRED)
pkg_search_module(NemoNotifications nemonotifications-qt5 REQUIRED IMPORTED_TARGET)

target_link_libraries(sailfishconnect_receivenotifications
PRIVATE
    kdeconnectcore
    KF5::I18n
    PkgConfig::NemoNotifications
)
