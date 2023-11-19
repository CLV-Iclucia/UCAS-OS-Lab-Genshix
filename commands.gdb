define cpu
    set $cpu_index = $arg0
    set $spin_lock_cnt = cpus[$cpu_index].spin_lock_cnt
    echo CPU $cpu_index\n
    echo Process Name:
    p $cpu_name = cpus[$cpu_index].current_running->pcb->name
    echo Spin Lock Count: 
    p $spin_lock_cnt
end

define sq
    set $queue=&$arg0
    set $tcbnode=$queue->next
    while $tcbnode!=$queue
        set $tcb=(tcb_t*)((char*)$tcbnode - offsetof(tcb_t, list))
        set $pcb=(pcb_t*)($tcb->pcb)
        set $name=(char*)($pcb->name) 
        echo Process Name:
        p $name
        set $tcbnode=$tcbnode->next
    end
end

define mythread
    set $cpuid=$arg0
    set $tcb_ptr=cpus[$cpuid].current_running
    p $tcb_ptr->pcb->name
end