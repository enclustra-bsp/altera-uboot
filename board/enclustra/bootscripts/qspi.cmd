sf probe
sf read ${ram_addr_bitstream} ${qspi_offset_addr_bitstream} ${size_bitstream}
fpga load 0 ${ram_addr_bitstream} ${size_bitstream}
bridge enable HPS-to-FPGA
bridge enable FPGA-to-HPS
sf read ${ram_addr_kernel} ${qspi_offset_addr_kernel} ${size_kernel}
sf read ${ram_addr_devicetree} ${qspi_offset_addr_devicetree} ${size_devicetree}
sf read ${ram_addr_rootfs} ${qspi_offset_addr_rootfs} ${size_rootfs}
bootm ${ram_addr_kernel} ${ram_addr_rootfs} ${ram_addr_devicetree}
