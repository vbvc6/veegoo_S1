$(call inherit-product-if-exists, target/allwinner/azalea-common/azalea-common.mk)

PRODUCT_PACKAGES +=

PRODUCT_COPY_FILES +=

PRODUCT_AAPT_CONFIG := large xlarge hdpi xhdpi
PRODUCT_AAPT_PERF_CONFIG := xhdpi
PRODUCT_CHARACTERISTICS := musicbox

PRODUCT_BRAND := vyagoo
PRODUCT_NAME := azalea_aiworld
PRODUCT_DEVICE := azalea-aiworld
PRODUCT_MODEL := Azalea AIWORLD
