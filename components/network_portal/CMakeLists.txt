idf_component_register(
    SRCS "network_portal.c"
    INCLUDE_DIRS "include"
    REQUIRES wifi_manager portal_backend twse_client
)
