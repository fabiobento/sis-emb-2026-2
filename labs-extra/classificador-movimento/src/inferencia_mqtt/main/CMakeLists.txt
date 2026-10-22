# main/CMakeLists.txt — ATENCAO: o exemplo do Edge Impulse ja traz um CMakeLists.txt em main/
# que lista as fontes do SDK. NAO substitua o dele: ABRA o dele e (1) troque o arquivo principal
# pelo main.cpp deste lab e (2) acrescente "esp_wifi nvs_flash mqtt esp_event esp_netif driver"
# em REQUIRES/PRIV_REQUIRES. O trecho abaixo mostra so a ideia.
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi nvs_flash mqtt esp_event esp_netif driver esp_timer
)
