bridge enable
fatload mmc 0:1 ${ram_addr_kernel} uImage
fatload mmc 0:1 ${ram_addr_devicetree} devicetree.dtb
fatload mmc 0:1 ${ram_addr_rootfs} uramdisk

altera_set_storage QSPI

bootm ${ram_addr_kernel} ${ram_addr_rootfs} ${ram_addr_devicetree}
