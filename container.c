#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mount.h>
#include <limits.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#define ROOT_ID 0
#define OPTS_BUFFER_SIZE 16384

// path set.
char cwd_dir[128];
char rootfs_dir[256];
char myroot_dir[512];
char tools_dir[512];
char opts[OPTS_BUFFER_SIZE];

// path set
void init_paths(void)
{
  if (getcwd(cwd_dir, sizeof(cwd_dir)) == NULL)
  {
    perror("[Error] getcwd failed");
    exit(1);
  }
  snprintf(rootfs_dir, sizeof(rootfs_dir), "%s/rootfs", cwd_dir);     // /home/.../container-lab/rootfs
  snprintf(myroot_dir, sizeof(myroot_dir), "%s/myroot", rootfs_dir);  // /home/.../container-lab/rootfs/myroot
  snprintf(tools_dir, sizeof(tools_dir), "%s/tools", rootfs_dir);     // /home/.../container-lab/rootfs/tools
}

// 0. Root(sudo) permission checked.
void check_root_permissions(void)
{
  if (geteuid() != ROOT_ID)
  {
    fprintf(stderr, "[Error] need permission (sudo)\n");
    exit(1);
  }
}

// mkdir -p (recursive)
int mkdir_p(const char *path, mode_t chmode)
{
  char tmp[512];
  char *p = NULL;
  size_t len;

  strncpy(tmp, path, sizeof(tmp) - 1); // /home/.../container-lab/rootfs
  tmp[sizeof(tmp) - 1] = '\0';

  len = strlen(tmp);
  if (tmp[len - 1] == '/')
    tmp[len - 1] = 0;

  for (p = tmp + 1; *p; p++)
  {
    if (*p == '/')
    {
      *p = 0;
      if (mkdir(tmp, chmode) != 0 && errno != EEXIST)
        return -1;
      *p = '/';
    }
  }
  if (mkdir(tmp, chmode) != 0 && errno != EEXIST)
    return -1;
  return 0;
}

// @brief : make file and write content.
// @param1 : path (file path)
// @param2 : content (file content)
void write_file(const char *path, const char *content)
{
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0)
  {
    return;
  }
  ssize_t res = write(fd, content, strlen(content));
  if (res < 0)
    perror("[Error] write failed in write_file()");
  close(fd);
}

// @brief : copy binary file from src to target_dir/bin/.
// @param1 : src (source file path)
// @param2 : target_dir (target directory path)
void copy_bin(const char *src, const char *target_dir)
{
  char dest[512];
  char bin_folder[512];
  char buf[4096];
  ssize_t bytes;

  snprintf(bin_folder, sizeof(bin_folder), "%s/bin", target_dir);
  mkdir_p(bin_folder, 0755);

  char src_copy[1024];
  strncpy(src_copy, src, sizeof(src_copy) - 1);
  src_copy[sizeof(src_copy) - 1] = '\0';
  char *base_name = basename(src_copy);
  snprintf(dest, sizeof(dest), "%s/bin/%s", target_dir, base_name);

  int src_fd = open(src, O_RDONLY);
  if (src_fd < 0)
  {
    fprintf(stderr, "[Warn] open failed in copy_bin() : %s\n", src);
    return;
  }

  int dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0755);
  if (dest_fd < 0)
  {
    perror("[Error] open failed in copy_bin() ");
    close(src_fd);
    return;
  }

  while ((bytes = read(src_fd, buf, sizeof(buf))) > 0)
  {
    ssize_t res = write(dest_fd, buf, bytes);
    if (res < 0)
      perror("[Error] write failed in copy_bin() ");
  }
  if (src_fd >= 0)
    close(src_fd);
  if (dest_fd >= 0)
    close(dest_fd);
}

// @brief : copy library file from src to target_dir/lib/x86_64-linux-gnu/.
// @param1 : src (source library path)
// @param2 : target_dir (target directory path)
void copy_lib(const char *src, const char *target_dir)
{
  char dest[512];
  char lib_folder[512];
  char buf[4096];
  ssize_t bytes;

  snprintf(lib_folder, sizeof(lib_folder), "%s/lib/x86_64-linux-gnu", target_dir);
  mkdir_p(lib_folder, 0755);

  char src_copy[1024];
  strncpy(src_copy, src, sizeof(src_copy) - 1);
  src_copy[sizeof(src_copy) - 1] = '\0';
  char *base_name = basename(src_copy);
  snprintf(dest, sizeof(dest), "%s/lib/x86_64-linux-gnu/%s", target_dir, base_name);

  int src_fd = open(src, O_RDONLY);
  if (src_fd < 0)
  {
    fprintf(stderr, "[Warn] open failed in copy_lib() : %s\n", src);
    return;
  }

  int dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0755);
  if (dest_fd < 0)
  {
    perror("[Error] open failed in copy_lib() ");
    close(src_fd);
    return;
  }

  while ((bytes = read(src_fd, buf, sizeof(buf))) > 0)
  {
    ssize_t res = write(dest_fd, buf, bytes);
    if (res < 0)
      perror("[Error] write failed in copy_lib() ");
  }
  if (src_fd >= 0)
    close(src_fd);
  if (dest_fd >= 0)
    close(dest_fd);
}

// 2. fs set. (myroot & tools)
void init_rootfs(void)
{
  char check_file[1024];
  snprintf(check_file, sizeof(check_file), "%s/bin/sh", myroot_dir);
  if (access(check_file, F_OK) == 0)
  {
    return;
  }
  printf("[Step 1] make rootfs directory\n");

  char path[4096];

  // myroot
  snprintf(path, sizeof(path), "%s/bin", myroot_dir);
  mkdir_p(path, 0755);
  snprintf(path, sizeof(path), "%s/lib/x86_64-linux-gnu", myroot_dir);
  mkdir_p(path, 0755);
  snprintf(path, sizeof(path), "%s/lib64", myroot_dir);
  mkdir_p(path, 0755);
  snprintf(path, sizeof(path), "%s/usr", myroot_dir);
  mkdir_p(path, 0755);
  snprintf(path, sizeof(path), "%s/sys", myroot_dir);
  mkdir_p(path, 0755);
  snprintf(path, sizeof(path), "%s/proc", myroot_dir);
  mkdir_p(path, 0755);
  snprintf(path, sizeof(path), "%s/dev/pts", myroot_dir);
  mkdir_p(path, 0755);
  snprintf(path, sizeof(path), "%s/etc", myroot_dir);
  mkdir_p(path, 0755);

  // tools
  snprintf(path, sizeof(path), "%s/bin", tools_dir);
  mkdir_p(path, 0755);
  snprintf(path, sizeof(path), "%s/lib/x86_64-linux-gnu", tools_dir);
  mkdir_p(path, 0755);
  snprintf(path, sizeof(path), "%s/lib64", tools_dir);
  mkdir_p(path, 0755);
  snprintf(path, sizeof(path), "%s/usr", tools_dir);
  mkdir_p(path, 0755);

  printf("[Step 2] copy binaries\n");
  copy_bin("/bin/sh", myroot_dir);
  copy_bin("/bin/ls", myroot_dir);
  copy_bin("/bin/ps", myroot_dir);
  copy_bin("/bin/mount", myroot_dir);
  copy_bin("/bin/cat", myroot_dir);
  copy_bin("/bin/rm", myroot_dir);
  copy_bin("/bin/hostname", myroot_dir);

  copy_bin("/bin/which", tools_dir);
  copy_bin("/bin/ping", tools_dir);
  copy_bin("/bin/readlink", tools_dir);

  printf("[Step 3] copy required libraries (ldd dependencies)\n");
  /* 
    search ldd from each binary in bin folder and copy
    /bin/sh, /bin/ls, /bin/ps, /bin/mount/ 
  */
    /* myroot base libraries */
  copy_lib("/lib/x86_64-linux-gnu/libc.so.6", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libselinux.so.1", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libpcre.so.3", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libpcre2-8.so.0", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libdl.so.2", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libpthread.so.0", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libprocps.so.6", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libprocps.so.8", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libproc2.so.0", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libsystemd.so.0", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libzstd.so.1", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/librt.so.1", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/liblzma.so.5", myroot_dir);
  copy_lib("/usr/lib/x86_64-linux-gnu/liblz4.so.1", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libgcrypt.so.20", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libgpg-error.so.0", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libmount.so.1", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libblkid.so.1", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/libuuid.so.1", myroot_dir);
  copy_lib("/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2", myroot_dir);

  /* tools dependencies for ping, which, etc. */ 
  copy_lib("/lib/x86_64-linux-gnu/libcap.so.2", tools_dir);
  copy_lib("/lib/x86_64-linux-gnu/libidn2.so.0", tools_dir);
  copy_lib("/usr/lib/x86_64-linux-gnu/libunistring.so.2", tools_dir);
  copy_lib("/usr/lib/x86_64-linux-gnu/libnettle.so.6", tools_dir);
  copy_lib("/lib/x86_64-linux-gnu/libresolv.so.2", tools_dir);
  copy_lib("/usr/lib/x86_64-linux-gnu/libunistring.so.5", tools_dir);
  copy_lib("/lib/x86_64-linux-gnu/libunistring.so.5", tools_dir);
  //copy_lib("/lib/x86_64-linux-gnu/libc.so.6", tools_dir);
  //copy_lib("/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2", tools_dir); 
  // dynamic linker (ld-linux) symlink
  snprintf(path, sizeof(path), "%s/lib64/ld-linux-x86-64.so.2", myroot_dir);
  unlink(path);
  symlink("/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2", path);

  snprintf(path, sizeof(path), "%s/lib64/ld-linux-x86-64.so.2", tools_dir);
  unlink(path);
  symlink("/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2", path);  // tools/lib64/ld-linux-x86-64.so.2 -> /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2

  // /usr/lib -> ../lib  and /usr/lib64 -> ../lib64 to save costs of copying again.
  snprintf(path, sizeof(path), "%s/usr/lib", myroot_dir);
  unlink(path);
  symlink("../lib", path);
  snprintf(path, sizeof(path), "%s/usr/lib64", myroot_dir);
  unlink(path);
  symlink("../lib64", path);
  snprintf(path, sizeof(path), "%s/usr/lib", tools_dir);
  unlink(path);
  symlink("../lib", path);
  snprintf(path, sizeof(path), "%s/usr/lib64", tools_dir);
  unlink(path);

  symlink("../lib64", path);

  printf("[SUCCESS] init_rootfs done!\n\n");
}

// 3. 네트워크 브릿지(vb0) 및 NAT(iptables) 설정 함수 (인터넷 연결 완벽 지원)
void setup_network(const char *name)
{
  printf("[Step 4] setup Bridge (vb0) & NAT network for container '%s'\n", name);

  int ip_num = 2;
  for (int i = 0; name[i]; i++)
    ip_num = (ip_num + name[i]) % 250 + 2;

  char ip_addr[64];
  snprintf(ip_addr, sizeof(ip_addr), "172.19.0.%d", ip_num);
  
  FILE *check_f = fopen("/home/jisookim/container-lab/rootfs/myroot/etc/hosts", "r");
  if (!check_f)
  {
    FILE *init_f = fopen("/home/jisookim/container-lab/rootfs/myroot/etc/hosts", "w");
    if (init_f)
    {
      fprintf(init_f, "127.0.0.1\tlocalhost\n");
      fclose(init_f);
    }
  }
  else
  {
    fclose(check_f);
  }
  FILE *hf = fopen("/home/jisookim/container-lab/rootfs/myroot/etc/hosts", "a");
  if (hf)
  {
    fprintf(hf, "%s\t%s\n", ip_addr, name);
    fclose(hf);
  }

  // char hosts_name[512];
  // snprintf(hosts_name, sizeof(hosts_name), "127.0.0.1\tlocalhost\n" "%s\t%s\n", ip_addr, name);
  // write_file("/home/jisookim/container-lab/rootfs/myroot/etc/hosts", hosts_name);
  
  char cmd[512];
  int res = 0;

  // 1. set up a virtual switch, assign gateway IP(172.19.0.1)
  res = system("ip link add vb0 type bridge 2>/dev/null || true"); (void)res;
  res = system("ip addr add 172.19.0.1/24 dev vb0 2>/dev/null || true"); (void)res;
  res = system("ip link set vb0 up"); (void)res;

  // 2. container netns 생성
  snprintf(cmd, sizeof(cmd), "ip netns add %s 2>/dev/null || true", name);
  res = system(cmd); (void)res;

  // 3. Veth Pair 가상 랜선 생성 (veth-host <--> veth-guest)
  char veth_host[64], veth_guest[64];
  snprintf(veth_host, sizeof(veth_host), "v-%s", name);
  snprintf(veth_guest, sizeof(veth_guest), "v-%s-c", name);

  snprintf(cmd, sizeof(cmd), "ip link add %s type veth peer name %s 2>/dev/null || true", veth_host, veth_guest);
  res = system(cmd); (void)res;

  // 4. 호스트 쪽 veth를 vb0 switch 꽂고 켜기
  snprintf(cmd, sizeof(cmd), "ip link set %s master vb0", veth_host);
  res = system(cmd); (void)res;
  snprintf(cmd, sizeof(cmd), "ip link set %s up", veth_host);
  res = system(cmd); (void)res;

  // 5. veth를 netns로 밀어 넣고 eth0로 이름 변경
  snprintf(cmd, sizeof(cmd), "ip link set %s netns %s", veth_guest, name);
  res = system(cmd); (void)res;

  snprintf(cmd, sizeof(cmd), "ip netns exec %s ip link set %s name eth0", name, veth_guest);
  res = system(cmd); (void)res;

  // 6. netns에 IP(172.19.0.2/24) add a subnet, interface on.
  snprintf(cmd, sizeof(cmd), "ip netns exec %s ip addr add %s/24 dev eth0 2>/dev/null || true", name, ip_addr);
  res = system(cmd); (void)res;
  snprintf(cmd, sizeof(cmd), "ip netns exec %s ip link set eth0 up", name);
  res = system(cmd); (void)res;
  snprintf(cmd, sizeof(cmd), "ip netns exec %s ip link set lo up", name); //interface on
  res = system(cmd); (void)res;

  // 7. Default gateway(Default Route -> vb0: 172.19.0.1) registration
  snprintf(cmd, sizeof(cmd), "ip netns exec %s ip route add default via 172.19.0.1 2>/dev/null || true", name);
  res = system(cmd); 
  (void)res;

  // 8. Host packet forwarding & NAT (iptables) on- like tollgate in highway
  write_file("/proc/sys/net/ipv4/ip_forward", "1\n"); //enabled IP forwarding
  
  res = system("iptables -t nat -C POSTROUTING -s 172.19.0.0/24 ! -o vb0 -j MASQUERADE 2>/dev/null || "
               "iptables -t nat -A POSTROUTING -s 172.19.0.0/24 ! -o vb0 -j MASQUERADE");
               (void)res;
  //출국 규칙  -C  check  -A accept -i input -o ouput -j jump ! = not
  res = system("iptables -C FORWARD -i vb0 ! -o vb0 -j ACCEPT 2>/dev/null || "
               "iptables -A FORWARD -i vb0 ! -o vb0 -j ACCEPT");
               (void)res;
  // 입국 규칙 conntrack : connection tracking , related, established: 연결된 패킷, 응답 패킷 수락
  // --ctstate : 패킷의 connection state
  res = system("iptables -C FORWARD -o vb0 -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT 2>/dev/null || "
               "iptables -A FORWARD -o vb0 -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT");
               (void)res;

  // 9. DNS configuration(/etc/resolv.conf에 8.8.8.8 등록)
  mkdir_p("/etc", 0755);
  write_file("/home/jisookim/container-lab/rootfs/myroot/etc/resolv.conf", "nameserver 8.8.8.8\nnameserver 1.1.1.1\n");

  printf("[SUCCESS] Bridge & NAT network setup complete!\n\n");
}

// 4. resource allocations (Cgroups : CPU 40%, Memory 200MB Limit)
void setup_cgroups(const char *name)
{
  printf("[Step 5] setup cgroups limitation for '%s'\n", name);

  char cg_dir[512], cg_file[1024];
  // cgroups  controller hierarchy
  snprintf(cg_dir, sizeof(cg_dir), "/sys/fs/cgroup/%s", name); 
  mkdir_p(cg_dir, 0755);
  write_file("/sys/fs/cgroup/cgroup.subtree_control", "+cpu +memory +pids\n");
  system("cat /sys/fs/cgroup/cgroup.controllers\n");
  // KIMCHI resource allocations
  snprintf(cg_file, sizeof(cg_file), "/sys/fs/cgroup/%s/cpu.max", name);
  write_file(cg_file, "30000 100000\n"); //30%
  snprintf(cg_file, sizeof(cg_file), "/sys/fs/cgroup/%s/memory.max", name);
  write_file(cg_file, "104857600\n"); // 100MB
  snprintf(cg_file, sizeof(cg_file), "%s/pids.max", cg_dir);
  write_file(cg_file, "50\n");
  printf("[SUCCESS] cgroups setup complete!\n\n");
}

//5. overlay
void setup_overlay(const char *name)
{
  printf("[Step 6] setup overlayfs\n");
  char upper[512], work[512], merge[512];
  
  snprintf(upper, sizeof(upper), "%s/%s/upper", rootfs_dir, name);
  snprintf(work, sizeof(work), "%s/%s/work", rootfs_dir, name);
  snprintf(merge, sizeof(merge), "%s/%s/merge", rootfs_dir, name);

  mkdir_p(upper, 0755);
  mkdir_p(work, 0755);
  mkdir_p(merge, 0755);
  
  // lowerdir(readonly): tools(first floor) : myroot(second floor), upperdir(write only) = upper, workdir = work
  snprintf(opts, sizeof(opts), "lowerdir=%s:%s,upperdir=%s,workdir=%s",
            tools_dir, myroot_dir, upper, work);
  
  /*
    @breif : Attach overlayFS to the merge directory
    @param1 : virtual filesystem name
    @param2 : final dest folder to merge(read & write)
    @param3 : file system type
    @param4 : mount flag
    @param5 : options
  */
  if (mount("virtual_FS", merge, "overlay", 0, opts) != 0)
  {
    if (errno == EBUSY)
    {
      printf("already mount\n");
    }
    else
    {
      perror("[ERROR] failed mount in setup overlayfs");
      exit(1);
    }
  }
  printf("[SUCCESS] overlayfs setup done\n");
}  

//7. pivot_root
void setup_pivot_root(const char *name)
{
  char netns_path[512];
  snprintf(netns_path, sizeof(netns_path), "/var/run/netns/%s", name);
  int net_fd = open(netns_path, O_RDONLY);
  if (net_fd >= 0)
  {
    setns(net_fd, CLONE_NEWNET);
    close(net_fd);
  }
  //cat /sys/fs/cgroup/init.scope/cgroup.procs
  char cgroup_path[512];
  snprintf(cgroup_path, sizeof(cgroup_path), "/sys/fs/cgroup/%s/cgroup.procs", name);
  write_file(cgroup_path, "0\n"); // to make child pid to root root is 0

  mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL); //cut off chain with host's mount.
  char merge[512];
  snprintf(merge, sizeof(merge), "%s/%s/merge", rootfs_dir, name);
  //printf("[DEBUG] before pivot_root, merge path: %s\n", merge); 
  mount(merge, merge, NULL, MS_BIND | MS_REC, NULL);   //before calling pivot_root, we need a mount point, because pitvot_root gonna move the mount point to the new root. so it did self bind mount to persist it. since merge is gonna be a new root.
  mkdir_p(merge, 0775);


  chdir(merge);
  mkdir_p(".old_root", 0755);
  // . is merge.
  if (syscall(SYS_pivot_root, ".", ".old_root") < 0)  // merge is gonna be a new root and ./old_root is gonna be a current host's root folder.
  {
    perror("[ERROR] pivot_root failed");
    exit(1);
  }
  // chdir("/");
  // int fd = open("/", O_RDONLY);
  // mkdir("temp", 0755);
  // chroot("temp");
  // fchdir(fd);
  // close(fd);
  // for (int i = 0; i < 10; i++)
  // {
  //   chdir("..");
  // }
  // chroot(".");
  // chdir("/");
  // system("ls -la /");
  
  chdir("/");
  mount("proc", "/proc", "proc", 0, NULL);
  mount("sysfs", "/sys", "sysfs", 0, NULL);
  mount("devpts", "/dev/pts", "devpts", 0, NULL);
  umount2("/.old_root", MNT_DETACH);    // now detach... not rm! don't rm!
}

void setup_chroot(const char *name)
{
  char merge[512];
  snprintf(merge, sizeof(merge), "%s/%s/merge", rootfs_dir, name);
  chdir(merge);
  chroot(".");
  // int fd = open("/", O_RDONLY);
  // mkdir("temp", 0755);
  // chroot("temp");
  // fchdir(fd);
  // close(fd);
  // for (int i = 0; i < 10; i++)
  // {
  //   chdir("..");
  // }
  // chroot(".");
  chdir("/");
  //system("ls -la /"); 
  mount("proc", "/proc", "proc", 0, NULL);
  mount("sysfs", "/sys", "sysfs", 0, NULL);
  mount("devpts", "/dev/pts", "devpts", 0, NULL);
}

//6. unsahre
/*
  //process isolation.  
  1. hostname isolation.
  2. pid isolation.
  3. mount isolation.
  4. network isolation.
  5. user isolation for future..
*/
void ioslate_and_exec(const char *name)
{
  if (unshare(CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWIPC) != 0)
  {
    perror("[ERROR] unshare failed in isolate_and_exec");
    exit(1);
  }
  if (sethostname(name, strlen(name)) != 0)
  {
    perror("[ERROR] sethostname failed in ioslate_and_exec");
    exit(1);
  }
  pid_t pid = fork();
  if (pid < 0)
  {
    perror("[ERROR] fork failed");
    exit(1);
  }
  if (pid == 0)
  {
    setup_pivot_root(name);
    //setup_chroot(name);
    //char cmd[256];
    // snprintf(cmd, sizeof(cmd), "cat /proc/%d/mountinfo", getpid());
    // int res = system(cmd);
    // (void)res;
    char *args[] = {"/bin/sh", NULL};
    char *envp[] = {"PATH=/bin:/usr/bin", "TERM=xterm", NULL};
    execve("/bin/sh", args, envp);
    perror("[ERROR] execlp failed");
    exit(1);
  }
  if (pid > 0)
  {
    char pid_file[256];
    snprintf(pid_file, sizeof(pid_file), "/home/jisookim/container-lab/%s.pid", name);
    FILE *f = fopen(pid_file, "w");
    //printf("[DEBUG] pid_file path: %s\n", pid_file); 
    if (f)
    {
      fprintf(f, "%d\n", pid);
      printf("pid : %d\n", pid);
      fclose(f);
    }

    int status;
    waitpid(pid, &status, 0);
    return ;
  }
}

void set_ns(const char *name)
{
  const char *ns_list[] = {"net", "uts", "ipc", "pid" , "mnt", NULL};
  int total = 0;
  while (ns_list[total])
  {
    total++;
  }
  char pid_file[256];
  snprintf(pid_file, sizeof(pid_file), "/home/jisookim/container-lab/%s.pid", name);

  printf("pid_file: %s\n", pid_file); 
  FILE *f = fopen(pid_file, "r");
  if (!f)
  {
    perror("[ERROR] fopen failed");
    exit(1);
  }
  char buf[32];
  int fd = open(pid_file, O_RDONLY);
  ssize_t len = read(fd, buf, sizeof(buf) - 1);
  buf[len] = '\0';
  int pid = atoi(buf); 
  close(fd);

  int fds[total];
  for (int i = 0; i < total; i++)
  {
    char path[256] = {0};
    snprintf(path, sizeof(path), "/proc/%d/ns/%s", pid, ns_list[i]);
    fds[i] = open(path, O_RDONLY);
    if (fds[i] < 0)
    {
      perror("[ERROR] open ns failed");
      exit(1);
    }
  }
  for (int i = 0; i < total; i++)
  {
    if (setns(fds[i], 0) != 0)
    {
      perror("[ERROR] setns failed");
      exit(1);
    }
    close(fds[i]);
  }
}

void clean_container(const char *name)
{
  char cmd[1024];

  snprintf(cmd, sizeof(cmd), "while umount -l %s/%s/merge 2>/dev/null; do :; done", rootfs_dir, name);
  system(cmd);

  char cmd_hosts[512];
  snprintf(cmd_hosts, sizeof(cmd_hosts), "sed -i '/\\b%s\\b/d' /home/jisookim/container-lab/rootfs/myroot/etc/hosts 2>/dev/null || true", name);
  system(cmd_hosts);

  snprintf(cmd, sizeof(cmd), "ip netns del %s 2>/dev/null || true", name);
  system(cmd);
  snprintf(cmd, sizeof(cmd), "ip link del v-%s 2>/dev/null || true", name);
  system(cmd);

  snprintf(cmd, sizeof(cmd), "/home/jisookim/container-lab/%s.pid", name);
  unlink(cmd);

  snprintf(cmd, sizeof(cmd), "rmdir /sys/fs/cgroup/%s 2>/dev/null || true", name);
  system(cmd);
  
  snprintf(cmd, sizeof(cmd), "rm -rf %s/%s", rootfs_dir, name);
  system(cmd);
  
  printf("[SUCCESS] Container '%s' cleaned up completely!\n", name);
}

void fclean_container(const char *name)
{
  clean_container(name);
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "umount -l %s/*/merge 2>/dev/null || true; rm -rf %s", rootfs_dir, rootfs_dir);
  system(cmd);
}

void exec_container(const char *name)
{
  set_ns(name);
  pid_t pid = fork();
  if (pid == 0)
  {
    char *args[] = {"/bin/sh", NULL};
    char *envp[] = {"PATH=/bin:/usr/bin", "TERM=xterm", NULL};
    execve("/bin/sh", args, envp);
    perror("[ERROR] execve failed in exec_container");
    exit(1);
  }
  waitpid(pid, NULL, 0);
}

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    printf("invalid arguments\n");
    return 1;
  }
  char *command = argv[1];

  init_paths();
  if (strcmp(command, "test-chroot") == 0)
  {
    setup_chroot(argc > 2 ? argv[2] : "kimchi");
    return 0;
  }

  if (argc < 3)
  {
    printf("invalid arguments\n");
    return 1;
  }
  char *name = argv[2];

  printf("Container Lab\n");
  check_root_permissions();
  if (strcmp(command, "run") == 0)
  {
    init_rootfs();
    setup_network(name);
    setup_cgroups(name);
    setup_overlay(name);
    ioslate_and_exec(name);
  }
  else if (strcmp(command, "exec") == 0)
  {
    exec_container(name);
  }
  else if (strcmp(command, "ping") == 0)
  {
    const char *target = "8.8.8.8"; // google DNS
    if (argc > 3)
    {
      target = argv[3];
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip netns exec %s ping -c 10 %s", name, target);
    system(cmd);
  }
  else if (strcmp(command, "clean") == 0)
  {
    clean_container(name);
  }
  else if (strcmp(command, "fclean") == 0)
  {
    fclean_container(name);
  }

  printf("ALL SETTINGS SUCCESS!\n");
  return 0;
}
