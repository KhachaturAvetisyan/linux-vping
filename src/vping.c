#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/inet.h>

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <net/checksum.h>


#define VPING_PROC_DIR "vping"
#define VPING_PROC_IP  "ip"
#define VPING_IP_BUFSZ 32

static struct net_device *vping_dev;

static struct proc_dir_entry *vping_proc_dir;
static struct proc_dir_entry *vping_proc_ip;

static __be32 vping_ip;


/* Called when interface is brought UP */
static int vping_open(struct net_device *dev)
{
    netif_start_queue(dev);
    pr_info("vping: interface opened\n");
    return 0;
}

/* Called when interface is brought DOWN */
static int vping_stop(struct net_device *dev)
{
    netif_stop_queue(dev);
    pr_info("vping: interface closed\n");
    return 0;
}

/* Called when kernel sends packet through vping0 */
static netdev_tx_t vping_start_xmit(
    struct sk_buff *skb, 
    struct net_device *dev
)
{
    struct ethhdr *eth;
    struct iphdr *iph;
    struct icmphdr *icmph;
    unsigned int ip_hdr_len;
    unsigned int icmp_len;
    unsigned char tmp_mac[ETH_ALEN];
    __be32 tmp_ip;

    if (skb->len < ETH_HLEN + sizeof(struct iphdr))
        goto drop;

    eth = (struct ethhdr *)skb->data;

    if (eth->h_proto != htons(ETH_P_IP))
        goto drop;

    iph = (struct iphdr *)(skb->data + ETH_HLEN);
    ip_hdr_len = iph->ihl * 4;

    if (ip_hdr_len < sizeof(struct iphdr))
        goto drop;

    if (skb->len < ETH_HLEN + ip_hdr_len + sizeof(struct icmphdr))
        goto drop;

    if (iph->protocol != IPPROTO_ICMP)
        goto drop;

    if (iph->daddr != vping_ip)
        goto drop;

    icmph = (struct icmphdr *)((u8 *)iph + ip_hdr_len);

    if (icmph->type != ICMP_ECHO)
        goto drop;

    pr_info("vping: replying to ping src=%pI4 dst=%pI4\n",
            &iph->saddr,
            &iph->daddr);

    /*
     * Swap Ethernet addresses
     */
    ether_addr_copy(tmp_mac, eth->h_source);
    ether_addr_copy(eth->h_source, eth->h_dest);
    ether_addr_copy(eth->h_dest, tmp_mac);

    /*
     * Swap IPv4 addresses
     */
    tmp_ip = iph->saddr;
    iph->saddr = iph->daddr;
    iph->daddr = tmp_ip;

    /*
     * Change ICMP Echo Request to Echo Reply
     */
    icmph->type = ICMP_ECHOREPLY;

    /*
     * Recalculate ICMP checksum.
     * ICMP checksum covers ICMP header + ICMP payload.
     */
    icmp_len = ntohs(iph->tot_len) - ip_hdr_len;
    icmph->checksum = 0;
    icmph->checksum = ip_compute_csum((void *)icmph, icmp_len);

    /*
     * Recalculate IPv4 header checksum.
     */
    iph->check = 0;
    iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

    /*
     * Feed packet back into Linux receive path.
     */
    skb->dev = dev;
    skb->protocol = eth_type_trans(skb, dev);
    skb->ip_summed = CHECKSUM_UNNECESSARY;

    netif_rx(skb);

    return NETDEV_TX_OK;

drop:
    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
}

static const struct net_device_ops vping_netdev_ops = {
    .ndo_open       = vping_open,
    .ndo_stop       = vping_stop,
    .ndo_start_xmit = vping_start_xmit,
};

/* cat /proc/vping/ip */
static ssize_t vping_proc_ip_read(
    struct file *file,
    char __user *ubuf,
    size_t count,
    loff_t *ppos
)
{
    char buf[VPING_IP_BUFSZ];
    int len;

    if (*ppos > 0)
        return 0;

    len = snprintf(buf, sizeof(buf), "%pI4\n", &vping_ip);

    if (copy_to_user(ubuf, buf, len))
        return -EFAULT;

    *ppos = len;

    return len;
}

/* echo 192.168.10.10 > /proc/vping/ip */
static ssize_t vping_proc_ip_write(
    struct file *file,
    const char __user *ubuf,
    size_t count,
    loff_t *ppos
)
{
    char buf[VPING_IP_BUFSZ];
    __be32 new_ip;

    if (count == 0 || count >= sizeof(buf))
        return -EINVAL;

    if (copy_from_user(buf, ubuf, count))
        return -EFAULT;

    buf[count] = '\0';

    if (buf[count - 1] == '\n')
        buf[count - 1] = '\0';

    if (!in4_pton(buf, -1, (u8 *)&new_ip, -1, NULL))
        return -EINVAL;

    vping_ip = new_ip;

    pr_info("vping: ip set to %pI4\n", &vping_ip);

    return count;
}

static const struct proc_ops vping_proc_ip_ops = {
    .proc_read  = vping_proc_ip_read,
    .proc_write = vping_proc_ip_write,
};

static void vping_cleanup_procfs(void)
{
    if (vping_proc_ip) 
    {
        remove_proc_entry(VPING_PROC_IP, vping_proc_dir);
        vping_proc_ip = NULL;
    }

    if (vping_proc_dir) 
    {
        remove_proc_entry(VPING_PROC_DIR, NULL);
        vping_proc_dir = NULL;
    }
}

static int vping_init_procfs(void)
{
    vping_proc_dir = proc_mkdir(VPING_PROC_DIR, NULL);
    if (!vping_proc_dir)
        return -ENOMEM;

    vping_proc_ip = proc_create(
        VPING_PROC_IP,
        0666,
        vping_proc_dir,
        &vping_proc_ip_ops
    );

    if (!vping_proc_ip) 
    {
        vping_cleanup_procfs();
        return -ENOMEM;
    }

    return 0;
}

static int __init vping_init(void)
{
    int ret;

    pr_info("vping: loading module\n");

    vping_dev = alloc_netdev(
        0,
        "vping%d",
        NET_NAME_UNKNOWN,
        ether_setup
    );

    if (!vping_dev) 
    {
        pr_err("vping: alloc_netdev failed\n");
        return -ENOMEM;
    }

    vping_dev->netdev_ops = &vping_netdev_ops;

    eth_hw_addr_random(vping_dev);

    ret = register_netdev(vping_dev);
    if (ret) 
    {
        pr_err("vping: register_netdev failed\n");
        free_netdev(vping_dev);
        vping_dev = NULL;
        return ret;
    }

    ret = vping_init_procfs();
    if (ret) 
    {
        pr_err("vping: procfs init failed\n");
        unregister_netdev(vping_dev);
        free_netdev(vping_dev);
        vping_dev = NULL;
        return ret;
    }

    pr_info("vping: interface registered\n");

    return 0;
}

static void __exit vping_exit(void)
{
    pr_info("vping: unloading module\n");

    vping_cleanup_procfs();

    if (vping_dev) 
    {
        unregister_netdev(vping_dev);
        free_netdev(vping_dev);
        vping_dev = NULL;
    }

    pr_info("vping: module unloaded\n");
}

module_init(vping_init);
module_exit(vping_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Khachtur");
MODULE_DESCRIPTION("Virtual ping network interface");
