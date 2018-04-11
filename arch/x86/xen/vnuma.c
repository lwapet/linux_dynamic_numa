#include <linux/err.h>
#include <linux/memblock.h>
#include <xen/interface/xen.h>
#include <xen/interface/memory.h>
#include <asm/xen/interface.h>
#include <asm/xen/hypercall.h>
#include <asm/xen/vnuma.h>

/*
 * Called from numa_init if numa_off = 0. 
 * modified by Lavoisier Wapet to take into account the phys numa architecture
 */
int __init xen_numa_init_from_vnuma(void){

        unsigned int i, j, idx;
	unsigned int cpu, nr_vnodes, nr_cpus, nr_vmem;
	unsigned int *vdistance, *cpu_to_vnode;
	unsigned long mem_size, dist_size, cpu_to_vnode_size;
	struct xen_vmemrange *vmem;
	u64 physm, physd, physc;
	int rc;

	struct xen_vnuma_topology_info vnuma_topo = {
		.domid = DOMID_SELF
	};

	rc = -EINVAL;
	physm = physd = physc = 0;

	if (!xen_pv_domain())
		return rc;

	/* get the number of nodes for allocation of memblocks. */
	nr_cpus = num_possible_cpus();

	/* support for nodes with at least one cpu per node. */
        nr_vnodes = nr_cpus;
    
        /* one mem range per node */
        nr_vmem = nr_vnodes;

	/*
	 * Allocate arrays for nr_vcpus/nr_vnodes sizes and let
	 * hypervisor know that these are the boundaries. Partial
	 * copy is not allowed and hypercall will fail.
	 */

	mem_size =  nr_vmem * sizeof(struct xen_vmemrange);
	dist_size = nr_vnodes * nr_vnodes * sizeof(*vnuma_topo.vdistance.h);
	cpu_to_vnode_size = nr_cpus * sizeof(*vnuma_topo.vcpu_to_vnode.h);

	physm = memblock_alloc(mem_size, PAGE_SIZE);
	physd = memblock_alloc(dist_size, PAGE_SIZE);
	physc = memblock_alloc(cpu_to_vnode_size, PAGE_SIZE);

	if (!physm || !physd || !physc)
		goto out;

	vmem = __va(physm);
	vdistance  = __va(physd);
	cpu_to_vnode  = __va(physc);

	vnuma_topo.nr_vnodes = nr_vnodes;
        vnuma_topo.nr_vcpus = nr_cpus;
        vnuma_topo.nr_vmemranges = nr_vmem;

	set_xen_guest_handle(vnuma_topo.vmemrange.h, vmem);
	set_xen_guest_handle(vnuma_topo.vdistance.h, vdistance);
	set_xen_guest_handle(vnuma_topo.vcpu_to_vnode.h, cpu_to_vnode);

	if (HYPERVISOR_memory_op(XENMEM_get_vnumainfo, &vnuma_topo) < 0)
        	goto out;
        
        /* There could be some changes */
        nr_vnodes = vnuma_topo.nr_vnodes;
        nr_cpus = vnuma_topo.nr_vcpus;
        nr_vmem = vnuma_topo.nr_vmemranges;

	/*
	 * NUMA nodes memory ranges are in pfns, constructed and
	 * aligned based on e820 ram domain map.
	 */
	for (i = 0; i < nr_vmem; i++) {
		if (numa_add_memblk(vmem[i].nid, vmem[i].start, vmem[i].end))
			goto out;
		node_set(vmem[i].nid, numa_nodes_parsed);
	}

	setup_nr_node_ids();
	/* Setting the cpu, apicid to node. */
	for_each_cpu(cpu, cpu_possible_mask) {
		set_apicid_to_node(cpu, cpu_to_vnode[cpu]);
		numa_set_node(cpu, cpu_to_vnode[cpu]);
		cpumask_set_cpu(cpu, node_to_cpumask_map[cpu_to_vnode[cpu]]);
	}

	for (i = 0; i < nr_vnodes; i++) {
		for (j = 0; j < nr_vnodes; j++) {
			idx = (i * nr_vnodes) + j;
			numa_set_distance(i, j, *(vdistance + idx));
		}
	}

	rc = 0;
out:
	if (physm)
		memblock_free(__pa(physm), mem_size);
	if (physd)
		memblock_free(__pa(physd), dist_size);
	if (physc)
		memblock_free(__pa(physc), cpu_to_vnode_size);
	/*
	 * Set a dummy node and return success.  This prevents calling any
	 * hardware-specific initializers which do not work in a PV guest.
	 * Taken from dummy_numa_init code.
	 */
	if (rc) {
		for (i = 0; i < MAX_LOCAL_APIC; i++)
			set_apicid_to_node(i, NUMA_NO_NODE);
		nodes_clear(numa_nodes_parsed);
		nodes_clear(node_possible_map);
		nodes_clear(node_online_map);
		node_set(0, numa_nodes_parsed);
		/* cpus up to max_cpus will be assigned to one node. */
		numa_add_memblk(0, 0, PFN_PHYS(max_pfn));
		setup_nr_node_ids();
	}
	return 0; 

}

int __init xen_numa_print_vnuma(void){

        unsigned int i, j, idx;
	unsigned int cpu, nr_vnodes, nr_cpus, nr_vmem;
	unsigned int *vdistance, *cpu_to_vnode;
	unsigned long mem_size, dist_size, cpu_to_vnode_size;
	struct xen_vmemrange *vmem;
	u64 physm, physd, physc;
	int rc;

	struct xen_vnuma_topology_info vnuma_topo = {
		.domid = DOMID_SELF
	};

	rc = -EINVAL;
	physm = physd = physc = 0;
	printk ("lavoisier wapet in function %s to print the vnuma memranges planned for the vm",__func__);
	if (!xen_pv_domain())
		return rc;

	/* get the number of nodes for allocation of memblocks. */
	nr_cpus = num_possible_cpus();

	/* support for nodes with at least one cpu per node. */
        nr_vnodes = nr_cpus;
    
        /* one mem range per node */
        nr_vmem = nr_vnodes;

	/*
	 * Allocate arrays for nr_vcpus/nr_vnodes sizes and let
	 * hypervisor know that these are the boundaries. Partial
	 * copy is not allowed and hypercall will fail.
	 */

	mem_size =  nr_vmem * sizeof(struct xen_vmemrange);
	dist_size = nr_vnodes * nr_vnodes * sizeof(*vnuma_topo.vdistance.h);
	cpu_to_vnode_size = nr_cpus * sizeof(*vnuma_topo.vcpu_to_vnode.h);

	physm = memblock_alloc(mem_size, PAGE_SIZE);
	physd = memblock_alloc(dist_size, PAGE_SIZE);
	physc = memblock_alloc(cpu_to_vnode_size, PAGE_SIZE);

	if (!physm || !physd || !physc)
		goto out;

	vmem = __va(physm);
	vdistance  = __va(physd);
	cpu_to_vnode  = __va(physc);

	vnuma_topo.nr_vnodes = nr_vnodes;
        vnuma_topo.nr_vcpus = nr_cpus;
        vnuma_topo.nr_vmemranges = nr_vmem;

	set_xen_guest_handle(vnuma_topo.vmemrange.h, vmem);
	set_xen_guest_handle(vnuma_topo.vdistance.h, vdistance);
	set_xen_guest_handle(vnuma_topo.vcpu_to_vnode.h, cpu_to_vnode);

	if (HYPERVISOR_memory_op(XENMEM_get_vnumainfo, &vnuma_topo) < 0)
        	goto out;
        
        /* There could be some changes */
        nr_vnodes = vnuma_topo.nr_vnodes;
        nr_cpus = vnuma_topo.nr_vcpus;
        nr_vmem = vnuma_topo.nr_vmemranges;

	/*
	 * NUMA nodes memory ranges are in pfns, constructed and
	 * aligned based on e820 ram domain map.
	 */
	for (i = 0; i < nr_vmem; i++) {
		printk("***\n range index : %d, node : %d , start_pfn : %llu, end_pfn : %llu",i,vmem[i].nid, vmem[i].start, vmem[i].end);
		
	}

	/* Setting the cpu, apicid to node. */
	for_each_cpu(cpu, cpu_possible_mask) {
                printk ("***\n cpu index : %d , corresponding node : %d ",cpu, cpu_to_vnode[cpu] );
	}

	for (i = 0; i < nr_vnodes; i++) {
		for (j = 0; j < nr_vnodes; j++) {
			idx = (i * nr_vnodes) + j;
			 printk("****\n distance entre le noeud %d et le noeud %d : %d ",i,j,*(vdistance + idx));
		}
	}

	rc = 0;
out:
	if (physm)
		memblock_free(__pa(physm), mem_size);
	if (physd)
		memblock_free(__pa(physd), dist_size);
	if (physc)
		memblock_free(__pa(physc), cpu_to_vnode_size);
	/*
	 * Set a dummy node and return success.  This prevents calling any
	 * hardware-specific initializers which do not work in a PV guest.
	 * Taken from dummy_numa_init code.
	 */
	/*if (rc) {
		for (i = 0; i < MAX_LOCAL_APIC; i++)
			set_apicid_to_node(i, NUMA_NO_NODE);
		nodes_clear(numa_nodes_parsed);
		nodes_clear(node_possible_map);
		nodes_clear(node_online_map);
		node_set(0, numa_nodes_parsed);*/
		/* cpus up to max_cpus will be assigned to one node. */
	/*	numa_add_memblk(0, 0, PFN_PHYS(max_pfn));
		setup_nr_node_ids();
	}*/
	return 0; 

}

int __init xen_numa_init_from_phys_numa(void){

        unsigned int i, j, idx;
        unsigned int cpu, nr_phys_nodes, nr_phys_cpus, nr_phys_mem;
        unsigned int *phys_distance, *phys_cpu_to_phys_node;
        unsigned long phys_mem_size, phys_dist_size, phys_cpu_to_phys_node_size;
        struct xen_vmemrange *phys_mem;
        u64  phys_addr_phys_m, phys_addr_phys_d, phys_addr_phys_c; /*m for memory ranges, d for distances, c for cpus*/
        int rc;

        struct xen_vnuma_topology_info /*for phys numa*/ phys_numa_topo = {
                .domid = DOMID_SELF
        };

        printk (" Lavoisier wapet in function %s ", __func__);
        printk (" Lavoisier wapet in function %s ", __func__);

        //send_printklog_tovmm();
        x86_init.resources.send_printklog(); 
        rc = -EINVAL;
        /* for phys memory initialisation */
        phys_addr_phys_m = phys_addr_phys_d = phys_addr_phys_c = 0;

        if (!xen_pv_domain())
                return rc;

        /* doing the same to for phys numa architectures nr*/
        nr_phys_mem = nr_phys_nodes = nr_phys_cpus =/* num_processors_phys*/ /*nr_cpu_ids*/num_possible_cpus();
        printk ("\n **** number of possible cpu : %d", nr_phys_cpus);


        /* allocate array for phys numa architecture */
        phys_mem_size =  nr_phys_mem * sizeof(struct xen_vmemrange);
        phys_dist_size = nr_phys_nodes * nr_phys_nodes * sizeof(*phys_numa_topo.vdistance.h);
        phys_cpu_to_phys_node_size = nr_phys_cpus * sizeof(*phys_numa_topo.vcpu_to_vnode.h);

        phys_addr_phys_m = memblock_alloc(phys_mem_size, PAGE_SIZE);
        phys_addr_phys_d = memblock_alloc(phys_dist_size, PAGE_SIZE);
        phys_addr_phys_c = memblock_alloc(phys_cpu_to_phys_node_size, PAGE_SIZE);

        if (!phys_addr_phys_m || !phys_addr_phys_d || !phys_addr_phys_d)
                goto out;
        phys_mem = __va(phys_addr_phys_m);
        phys_distance  = __va(phys_addr_phys_d);
        phys_cpu_to_phys_node  = __va(phys_addr_phys_c);

        phys_numa_topo.nr_vnodes = nr_phys_nodes;
        phys_numa_topo.nr_vcpus = nr_phys_cpus;
        phys_numa_topo.nr_vmemranges = nr_phys_mem;

        set_xen_guest_handle(phys_numa_topo.vmemrange.h, phys_mem);
        set_xen_guest_handle(phys_numa_topo.vdistance.h, phys_distance);
        set_xen_guest_handle(phys_numa_topo.vcpu_to_vnode.h, phys_cpu_to_phys_node);

        if (HYPERVISOR_memory_op(XENMEM_get_phys_numainfo, &phys_numa_topo) < 0)
                goto out;


        /* There could be some changes */
        nr_phys_nodes = phys_numa_topo.nr_vnodes;
        nr_phys_cpus = phys_numa_topo.nr_vcpus;
        nr_phys_mem = phys_numa_topo.nr_vmemranges;
        printk ("\n **** number of final phys cpu : %d", nr_phys_nodes);

        /*
         * NUMA nodes memory ranges are in pfns, constructed and
         * aligned based on e820 ram domain map.
         */

        
        for (i = 0; i < nr_phys_mem; i++) {
                 printk("***\n range index : %d, node : %d , start : %llu, end : %llu",i,phys_mem[i].nid, phys_mem[i].start, phys_mem[i].end);
                if (numa_add_memblk(phys_mem[i].nid, phys_mem[i].start, phys_mem[i].end)){
                       printk("\n **** the range has not been added");
                       goto out;
                }
                printk("\n **** the range has been successfully added");
                node_set(phys_mem[i].nid, numa_nodes_parsed);
        }

        setup_nr_node_ids();
        /* Setting the cpu, apicid to node. */
        for_each_cpu(cpu, cpu_possible_mask) {
                printk ("***\n cpu index : %d , corresponding node : %d ",cpu, phys_cpu_to_phys_node[cpu] );
                set_apicid_to_node(cpu, phys_cpu_to_phys_node[cpu]);
                numa_set_node(cpu, phys_cpu_to_phys_node[cpu]);
                cpumask_set_cpu(cpu, node_to_cpumask_map[phys_cpu_to_phys_node[cpu]]);
        }

        for (i = 0; i < nr_phys_nodes; i++) {
                for (j = 0; j < nr_phys_nodes; j++) {
                        idx = (i * nr_phys_nodes) + j;
                        printk("****\n distance entre le noeud %d et le noeud (before the call) %d : %d ",i,j,*(phys_distance + idx));

                        numa_set_distance(i, j, *(phys_distance + idx));
                        printk("****\n distance entre le noeud %d et le noeud %d : %d ",i,j,*(phys_distance + idx));
                }
        }

        rc = 0;
out:
        if (phys_addr_phys_m)
                memblock_free(__pa(phys_addr_phys_m), phys_mem_size);
        if (phys_addr_phys_d)
                memblock_free(__pa(phys_addr_phys_d), phys_dist_size);
        if (phys_addr_phys_c)
                memblock_free(__pa(phys_addr_phys_c), phys_cpu_to_phys_node_size);
        /*
         * Set a dummy node and return success.  This prevents calling any
         * hardware-specific initializers which do not work in a PV guest.
         * Taken from dummy_numa_init code.
         */
        if (rc) {
                for (i = 0; i < MAX_LOCAL_APIC; i++)
                        set_apicid_to_node(i, NUMA_NO_NODE);
                nodes_clear(numa_nodes_parsed);
                nodes_clear(node_possible_map);
                nodes_clear(node_online_map);
                node_set(0, numa_nodes_parsed);
                /* cpus up to max_cpus will be assigned to one node. */
                numa_add_memblk(0, 0, PFN_PHYS(max_pfn));
                setup_nr_node_ids();
        }
        return 0;
}
int __init xen_numa_init(void)
{
	/*
	if (xen_numa_init_from_vnuma() || xen_numa_init_from_phys_numa()){
		return -EINVAL;
        }
        return 0;*/
        xen_numa_print_vnuma();
        return xen_numa_init_from_phys_numa();
 
        
}

