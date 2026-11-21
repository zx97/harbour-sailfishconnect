kdeconnect_add_plugin(
    sailfishconnect_presenter
    JSON sailfishconnect_presenter.json
    SOURCES presenterplugin.cpp)

target_link_libraries(sailfishconnect_presenter
PRIVATE
    kdeconnectcore
    KF5::I18n
)
