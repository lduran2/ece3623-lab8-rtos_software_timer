connect -url tcp:127.0.0.1:3121
source C:/Zynq_Book/lab08_freertos_software_timer/lab08_freertos_software_timer.sdk/zybo_ldbtn_sw_wrapper_hw_platform_0/ps7_init.tcl
targets -set -nocase -filter {name =~"APU*" && jtag_cable_name =~ "Digilent Zybo 210279759083A"} -index 0
loadhw -hw C:/Zynq_Book/lab08_freertos_software_timer/lab08_freertos_software_timer.sdk/zybo_ldbtn_sw_wrapper_hw_platform_0/system.hdf -mem-ranges [list {0x40000000 0xbfffffff}]
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*" && jtag_cable_name =~ "Digilent Zybo 210279759083A"} -index 0
stop
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "ARM*#0" && jtag_cable_name =~ "Digilent Zybo 210279759083A"} -index 0
rst -processor
targets -set -nocase -filter {name =~ "ARM*#0" && jtag_cable_name =~ "Digilent Zybo 210279759083A"} -index 0
dow C:/Zynq_Book/lab08_freertos_software_timer/lab08_freertos_software_timer.sdk/rtos_software_timer/Debug/rtos_software_timer.elf
configparams force-mem-access 0
targets -set -nocase -filter {name =~ "ARM*#0" && jtag_cable_name =~ "Digilent Zybo 210279759083A"} -index 0
con
