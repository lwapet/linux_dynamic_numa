#!/bin/bash
sleep 2
sshpass -p "lm2panpo."  scp -r -P301 arch/x86/xen/vnuma.c root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/xen/
sleep 2
sshpass -p "lm2panpo."  scp -r -P301 arch/x86/xen/time.c root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/xen/
sleep 2
sshpass -p "lm2panpo."  scp -r -P301 drivers/xen/events/events_base.c root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/drivers/xen/events
sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/xen/enlighten_pv.c root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/xen/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  kernel/cpu.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/kernel/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  kernel/smp.c root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/kernel/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/xen/smp_pv.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/xen/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  drivers/acpi/bus.c root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/drivers/acpi/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/kernel/process.c root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/kernel/

sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/xen/xen-ops.h root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/xen/


sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/kernel/e820.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/kernel/e820.c

sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/include/asm/x86_init.h  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/include/asm/x86_init.h
sleep 3

sshpass -p "lm2panpo."  scp -P 301 mm/bootmem.c root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/mm/bootmem.c


sshpass -p "lm2panpo."  scp -P 301 mm/nobootmem.c root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/mm/nobootmem.c

sleep 2
sshpass -p "lm2panpo."  scp -P 301  mm/memblock.c   root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/mm/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  include/linux/memblock.h root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/include/linux/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/mm/numa.c   root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/mm/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  mm/page_alloc.c root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/mm/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/mm/init_64.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/mm/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/xen/mmu_pv.c   root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/xen/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/mm/init.c   root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/mm/

sleep 2
sshpass -p "lm2panpo."  scp -P 301 arch/x86/xen/p2m.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/xen/p2m.c

sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/xen/setup.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/xen/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/kernel/setup.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/kernel/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  mm/sparse.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/mm/ 
sleep 2
sshpass -p "lm2panpo."  scp -P 301  include/xen/interface/memory.h  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/include/xen/interface/memory.h
sleep 2

sshpass -p "lm2panpo."  scp -P 301  mm/sparse-vmemmap.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/mm/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  include/linux/bootmem.h  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/include/linux/
sleep 2
sshpass -p "lm2panpo."  scp -P 301  arch/x86/include/asm/pgalloc.h  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/arch/x86/include/asm/pgalloc.h
sleep 2

sshpass -p "lm2panpo." scp -P 301   mm/memory_hotplug.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/mm/memory_hotplug.c
sleep 2
sshpass -p "lm2panpo." scp -P 301   include/linux/memory_hotplug.h  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/include/linux/memory_hotplug.h



sshpass -p "lm2panpo." scp -P 301   drivers/net/xen-netback/interface.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/drivers/net/xen-netback/interface.c


sshpass -p "lm2panpo." scp -P 301 drivers/net/xen-netback/netback.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/drivers/net/xen-netback/netback.c


sleep 2

 sshpass -p "lm2panpo." scp -P 301  mm/page_isolation.c  root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/mm/page_isolation.c
sleep 2
 sshpass -p "lm2panpo." scp -P 301 init/main.c root@pc-sepia04:/root/lavoisier_dynamic_NUMA/linux-4.13.4_work/init/main.c



