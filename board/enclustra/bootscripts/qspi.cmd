bridge enable LWHPS-to-FPGA;
sf read ${ram_addr_kernel} ${qspi_offset_addr_kernel} ${size_kernel}
sf read ${ram_addr_devicetree} ${qspi_offset_addr_devicetree} ${size_devicetree}
sf read ${ram_addr_rootfs} ${qspi_offset_addr_rootfs} ${size_rootfs}
bootm ${ram_addr_kernel} ${ram_addr_rootfs} ${ram_addr_devicetree}