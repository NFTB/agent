#include <stdio.h>
#include <stdlib.h>
#include <libproc.h>

int main() {
    // 获取所需字节数
    int bytes = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (bytes <= 0) {
        fprintf(stderr, "proc_listpids (count) failed\n");
        return 1;
    }

    // 动态分配 PID 数组
    pid_t *pids = (pid_t *)malloc((size_t)bytes);
    if (!pids) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    // 获取真实数据
    int ret = proc_listpids(PROC_ALL_PIDS, 0, pids, bytes);
    if (ret <= 0) {
        fprintf(stderr, "proc_listpids (data) failed\n");
        free(pids);
        return 1;
    }
    int num_pids = ret / (int)sizeof(pid_t);

    for (int i = 0; i < num_pids; i++) {
        pid_t pid = pids[i];
        if (pid == 0) continue;

        struct proc_taskallinfo info;
        int n = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &info, sizeof(info));
        if (n != sizeof(info)) continue;  /* 无权限或进程已退出 */

        printf("[PID: %d] Name: %s\n", pid, info.pbsd.pbi_comm);
    }

    free(pids);
    return 0;
}
