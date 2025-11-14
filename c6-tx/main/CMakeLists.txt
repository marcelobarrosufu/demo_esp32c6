set(srcs "main.c" "ieee802154.c")

set(include "."
            "include")

# In order for the cases defined by `TEST_CASE` to be linked into the final elf,
# the component can be registered as WHOLE_ARCHIVE
idf_component_register(SRCS ${srcs}
                       PRIV_REQUIRES ieee802154 fatfs nvs_flash esp_phy 
                       WHOLE_ARCHIVE)
