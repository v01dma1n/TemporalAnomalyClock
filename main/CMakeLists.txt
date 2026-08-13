# TemporalAnomalyClock app.

idf_component_register(
    SRCS
        "main.cpp"
        "temporal_anomaly_app.cpp"
        "temporal_anomaly_preferences.cpp"
        "temporal_anomaly_access_point_manager.cpp"
        "temporal_anomaly_time_source.cpp"
        "photo_face_data.cpp"
        "photo_face_librarian_data.cpp"
        "photo_face_archivist_data.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp32_ntp_clock
        gc9a01
        nvs_flash
        driver
)

target_compile_options(${COMPONENT_LIB} PUBLIC -std=gnu++17)
