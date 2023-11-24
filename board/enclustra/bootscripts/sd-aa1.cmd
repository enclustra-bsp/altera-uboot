bridge enable
fatload mmc 0:1 ${ram_addr_kernel} uImage
fatload mmc 0:1 ${ram_addr_devicetree} devicetree.dtb
bootm ${ram_addr_kernel} - ${ram_addr_devicetree}
