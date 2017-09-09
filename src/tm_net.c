#include "tm_main.h"
#include "tm_x.h"
#include "tm_thread.h"
#include <sys/types.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

struct tm_net_stat {
	u64		rx_packets;
	u64		tx_packets;
	u64		rx_bytes;
	u64		tx_bytes;
};

struct tm_net {
	/* User configuration variables. */
	const char	*if_name;
	int		interval;	/* in milliseconds */
	u32		net_fg;
	u32		net_hi;

	int		ifindex;
	unsigned int	ifi_flags;
	size_t		dn_len;
	char		dot_notion[64];
	struct tm_net_stat	st_dif;
	struct tm_net_stat	st_cur;

	/* icon */
	struct tm_icon	icon_net;
	struct tm_icon	icon5;

	/* i/f state */
	struct tm_item	item_if_state[3];

	/* i/f stat */
	struct tm_item	item_if_stat[12];
};

static struct tm_net *tm_net(struct tm_context *tc)
{
	return tm_get_object(tc, TM_OBJECT_NET);
}

static int tm_net_parse_opts(struct tm_net *net, int argc, char **argv)
{
	int i, err;

	err = 1;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--if_name")) {
			if (++i >= argc) {
				fprintf(stderr, "--if_name needs argument.\n");
				goto out;
			}
			net->if_name = argv[i];
		} else if (!strcmp(argv[i], "--net_interval")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--net_interval needs argument.\n");
				goto out;
			}
			/* Hope user is careful enough. */
			net->interval = atoi(argv[i]);
		} else if (!strcmp(argv[i], "--net_fg")) {
			if (++i >= argc) {
				fprintf(stderr, "--net_fg needs argument.\n");
				goto out;
			}
			net->net_fg = tm_x_get_color_from_str(argv[i]);
		} else if (!strcmp(argv[i], "--net_hi")) {
			if (++i >= argc) {
				fprintf(stderr, "--net_hi needs argument.\n");
				goto out;
			}
			net->net_hi = tm_x_get_color_from_str(argv[i]);
		}
	}

	err = 0;
out:
	return err;
}

static int tm_net_get_ifindex(struct tm_net *net)
{
	struct ifreq req;
	int err, skfd;

	err = 1;
	strncpy(req.ifr_name, net->if_name, IFNAMSIZ);

	skfd = socket(PF_INET, SOCK_DGRAM, 0);
	if (skfd == -1) {
		pr_err("socket");
		goto out;
	}

	err = ioctl(skfd, SIOCGIFINDEX, &req);
	if (err) {
		pr_err("ioctl");
		goto err;
	}

	net->ifindex = req.ifr_ifindex;
err:
	close(skfd);
out:
	return err;
}

/**
 * "wlan0: up"
 * "wlan0: ":		item_if_state[0]	fixed-width
 * "up(/down)":		item_if_state[1]	changeable, left-align
 *
 * "XX.XX.XX.XX/YY":	item_if_state[2]	changeable, left-align
 */
static void tm_net_item_if_state_init(struct tm_context *tc)
{
	double x, y, height, margin_icon;
	struct tm_icon *icon_net;
	struct tm_net *net;
	u32 net_fg, net_hi;
	char if_str[32];

	net = tm_net(tc);
	icon_net = &net->icon_net;

	net_fg = net->net_fg;
	net_hi = net->net_hi;

	margin_icon = tm_x_margin_icon(tc);
	height = (double)tm_x_font_max_height(tc);

	x = icon_net->width + margin_icon;
	y = icon_net->height / 2.0 - height;

	sprintf(if_str, "%s: ", net->if_name);

	tm_item_init(tc, &net->item_if_state[0], TM_OBJECT_NET, net_fg, &x, &y,
		     0, height, if_str, TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &net->item_if_state[1], TM_OBJECT_NET, net_hi, &x, &y,
		     0, height, NULL,
		     TM_ITEM_WIDTH_CHANGEABLE | TM_ITEM_ALIGN_LEFT);

	x = icon_net->width + margin_icon;
	y = icon_net->height / 2.0;

	tm_item_init(tc, &net->item_if_state[2], TM_OBJECT_NET, net_fg, &x, &y,
		     0, height, NULL,
		     TM_ITEM_WIDTH_CHANGEABLE | TM_ITEM_ALIGN_LEFT);
}

/**
 * "Up: X.XX Ybps / XX.X YB"
 *
 * "Up: ":	item_if_stat[0]		fixed-width
 * "X.XX":	item_if_stat[1]		3digit+dot, right-align
 * " Ybps":	item_if_stat[2]		fixed-width(max-unit-width), left-align
 * " / ":	item_if_stat[3]		fixed-width
 * "XX.X":	item_if_stat[4]		3digit+dot, right-align
 * " YB":	item_if_stat[5]		fixed-width(max-unit-width), left-align
 *
 * "Down: X.XXX Ybps / XX.XX YB"
 *
 * "Down: ":	item_if_stat[6]		fixed-width
 * "X.XX":	item_if_stat[7]		3digit+dot, right-align
 * " Ybps":	item_if_stat[8]		fixed-width(max-unit-width), left-align
 * " / ":	item_if_stat[9]		fixed-width
 * "XX.X":	item_if_stat[10]	3digit+dot, right-align
 * " YB":	item_if_stat[11]	fixed-width(max-unit-width), left-align
 */
static void tm_net_item_if_stat_init(struct tm_context *tc)
{
	double x, y, lbl_wid, avail, ut_wid, height;
	struct tm_net *net;
	u32 net_fg, net_hi;

	net = tm_net(tc);

	net_fg = net->net_fg;
	net_hi = net->net_hi;

	tm_x_text_size(tc, "Down: ", 6, &lbl_wid, NULL);

	tm_x_text_size(tc, " bps", 4, &ut_wid, NULL);
	ut_wid += (double)tm_x_font_max_unit_width(tc);

	avail = tm_x_font_dot_width(tc) + tm_x_font_max_digit_width(tc) * 3.0;
	height = (double)tm_x_font_max_height(tc);

	x = 0;
	y = net->icon_net.height + tm_x_margin_icon(tc);

	tm_item_init(tc, &net->item_if_stat[0], TM_OBJECT_NET, net_fg, &x, &y,
		     lbl_wid, height, "Up: ", TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &net->item_if_stat[1], TM_OBJECT_NET, net_hi, &x, &y,
		     avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &net->item_if_stat[2], TM_OBJECT_NET, net_fg, &x, &y,
		     ut_wid, height, NULL, TM_ITEM_ALIGN_LEFT);
	tm_item_init(tc, &net->item_if_stat[3], TM_OBJECT_NET, net_fg, &x, &y,
		     0, height, " / ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &net->item_if_stat[4], TM_OBJECT_NET, net_hi, &x, &y,
		     avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &net->item_if_stat[5], TM_OBJECT_NET, net_fg, &x, &y,
		     ut_wid, height, NULL, TM_ITEM_ALIGN_LEFT);

	x = 0;
	y += height;

	tm_item_init(tc, &net->item_if_stat[6], TM_OBJECT_NET, net_fg, &x, &y,
		     lbl_wid, height, "Down: ", TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &net->item_if_stat[7], TM_OBJECT_NET, net_hi, &x, &y,
		     avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &net->item_if_stat[8], TM_OBJECT_NET, net_fg, &x, &y,
		     ut_wid, height, NULL, TM_ITEM_ALIGN_LEFT);
	tm_item_init(tc, &net->item_if_stat[9], TM_OBJECT_NET, net_fg, &x, &y,
		     0, height, " / ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &net->item_if_stat[10], TM_OBJECT_NET, net_hi, &x, &y,
		     avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &net->item_if_stat[11], TM_OBJECT_NET, net_fg, &x, &y,
		     ut_wid, height, NULL, TM_ITEM_ALIGN_LEFT);
}

static void tm_net_sockaddr_nl_init(struct sockaddr_nl *addr)
{
	*addr = (struct sockaddr_nl){
		.nl_family	= PF_NETLINK,
		.nl_pid		= 0,	/* To kernel. */
		.nl_groups	= 0
	};
}

static void tm_net_msghdr_init(struct msghdr *msg, struct sockaddr_nl *addr,
			       struct iovec *iov)
{
	*msg = (struct msghdr){
		.msg_name	= addr,
		.msg_namelen	= sizeof(*addr),
		.msg_iov	= iov,
		.msg_iovlen	= 1
	};
}

static void
tm_net_nlmsghdr_init(struct nlmsghdr *nlh, u32 nlmsg_len, u16 nlmsg_type)
{
	*nlh = (struct nlmsghdr){
		.nlmsg_len	= nlmsg_len,
		.nlmsg_type	= nlmsg_type,
		.nlmsg_flags	= NLM_F_REQUEST | NLM_F_DUMP
	};
}

static void tm_net_ifinfomsg_init(struct ifinfomsg *ifi)
{
	*ifi = (struct ifinfomsg){
		.ifi_family	= PF_UNSPEC
	};
}

static int tm_net_send_nl_request(int *nlsk, struct msghdr *msg,
				  struct sockaddr_nl *addr, struct iovec *iov,
				  char *buf, u16 nlmsg_type)
{
	struct ifinfomsg *ifi;
	struct nlmsghdr *nlh;
	ssize_t sz;
	int err;

	err = 1;

	*nlsk = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (*nlsk == -1) {
		pr_err("socket");
		goto out;
	}

	tm_net_sockaddr_nl_init(addr);
	tm_net_msghdr_init(msg, addr, iov);

	nlh = (struct nlmsghdr *)buf;
	ifi = NLMSG_DATA(nlh);

	tm_net_nlmsghdr_init(nlh, NLMSG_LENGTH(sizeof(*ifi)), nlmsg_type);
	tm_net_ifinfomsg_init(ifi);

	iov->iov_base = buf;
	iov->iov_len = (char *)(ifi + 1) - buf;

	sz = sendmsg(*nlsk, msg, 0);
	if (sz != iov->iov_len) {
		pr_err("sendmsg");
		goto err;
	}

	err = 0;
out:
	return err;
err:
	close(*nlsk);
	goto out;
}

static int tm_net_sockaddr_nl_check(const struct sockaddr_nl *addr)
{
	int err;

	err = 0;

	if (addr->nl_family != PF_NETLINK) {
		fprintf(stderr, "Invalid address family: %hu.\n",
			addr->nl_family);
		err = 1;
	}

	if (addr->nl_pid) {
		fprintf(stderr, "Invalid portid: %u.\n", addr->nl_pid);
		err = 1;
	}

	return err;
}

static bool nla_ok(const struct nlattr *nla, size_t rem)
{
	return rem >= sizeof(*nla) && nla->nla_len >= sizeof(*nla) &&
	       nla->nla_len <= rem;
}

static struct nlattr *nla_next(const struct nlattr *nla, size_t *rem)
{
	size_t totlen;

	totlen = NLA_ALIGN(nla->nla_len);
	*rem -= totlen;

	return (struct nlattr *)((char *)nla + totlen);
}

#define nla_for_each_attr(pos, head, rem)	\
	for (pos = head; nla_ok(pos, rem); pos = nla_next(pos, &(rem)))

static u16 nla_type(const struct nlattr *nla)
{
	return nla->nla_type & NLA_TYPE_MASK;
}

static void *nla_data(struct nlattr *nla)
{
	return (char *)nla + NLA_HDRLEN;
}

static size_t nla_len(const struct nlattr *nla)
{
	return nla->nla_len - NLA_HDRLEN;
}

static void
nla_parse(struct nlattr **tb, int maxtype, struct nlattr *head, size_t rem)
{
	struct nlattr *nla;

	memset(tb, 0, sizeof(nla) * maxtype);

	nla_for_each_attr(nla, head, rem) {
		u16 type;

		type = nla_type(nla);
		if (!type || type >= maxtype)
			continue;

		if (tb[type])
			fprintf(stderr, "nla_type %hu is overwritten.\n", type);
		tb[type] = nla;
	}

	if (rem)
		fprintf(stderr, "rem: %zd.\n", rem);
}

static int
tm_net_recv_nl_response(struct tm_context *tc, int nlsk, struct msghdr *msg,
			u16 nlmsg_type,
			bool (*tm_net_recv_func)(struct tm_context *,
						 struct nlmsghdr *))
{
	struct sockaddr_nl *addr;
	struct iovec *iov;
	char *buf;
	int err;

	err = 1;
	addr = msg->msg_name;
	iov = msg->msg_iov;
	buf = iov->iov_base;

	for (;;) {
		struct nlmsghdr *nlh;
		ssize_t sz;

#define TM_NET_IOV_LEN	32768
		iov->iov_len = TM_NET_IOV_LEN;

		sz = recvmsg(nlsk, msg, 0);
		if (sz == -1) {
			if (errno == EINTR)
				continue;
			pr_err("recvmsg");
			goto err;
		}

		err = tm_net_sockaddr_nl_check(addr);
		if (err)
			goto err;

		for (nlh = (struct nlmsghdr *)buf; NLMSG_OK(nlh, sz);
		     nlh = NLMSG_NEXT(nlh, sz)) {
			bool done;

			if (nlh->nlmsg_type == NLMSG_DONE)
				goto done;

			if (nlh->nlmsg_type != nlmsg_type) {
				fprintf(stderr, "Invalid nlmsg_type: %hu.\n",
					nlh->nlmsg_type);
				continue;
			}

			done = tm_net_recv_func(tc, nlh);
			if (done)
				goto done;
		}
	}
done:
	err = 0;
err:
	close(nlsk);

	return err;
}

static bool tm_net_recv_rtm_newaddr(struct tm_context *tc, struct nlmsghdr *nlh)
{
	struct nlattr *nla, *tb[__IFA_MAX];
	struct ifaddrmsg *ifm;
	struct tm_net *net;
	char addr_str[64];
	size_t len, rem;
	const char *p;
	bool done;

	net = tm_net(tc);
	ifm = NLMSG_DATA(nlh);
	done = false;

	if (net->ifindex != ifm->ifa_index)
		goto out;

	len = NLMSG_SPACE(sizeof(*ifm));
	nla = (struct nlattr *)((char *)nlh + len);
	rem = nlh->nlmsg_len - len;
	nla_parse(tb, __IFA_MAX, nla, rem);

	if (!tb[IFA_LOCAL])
		tb[IFA_LOCAL] = tb[IFA_ADDRESS];

	p = inet_ntop(ifm->ifa_family, nla_data(tb[IFA_LOCAL]), addr_str,
		      sizeof(addr_str));
	if (!p) {
		pr_err("inet_ntop");
		goto out;
	}

	net->dn_len = sprintf(net->dot_notion, "%s/%d", addr_str,
			      ifm->ifa_prefixlen);

	/* First occurrence is sufficient?
	 * We need to consider here.
	 */
	done = true;
out:
	return done;
}

static int tm_net_get_addr(struct tm_context *tc)
{
	static char buf[TM_NET_IOV_LEN];
	struct sockaddr_nl addr;
	struct tm_net *net;
	struct msghdr msg;
	struct iovec iov;
	int nlsk;

	net = tm_net(tc);

	memset(net->dot_notion, 0, sizeof(net->dot_notion));

	return tm_net_send_nl_request(&nlsk, &msg, &addr, &iov, buf,
				      RTM_GETADDR) ||
	       tm_net_recv_nl_response(tc, nlsk, &msg, RTM_NEWADDR,
				       tm_net_recv_rtm_newaddr);
}

static bool tm_net_recv_rtm_newlink(struct tm_context *tc, struct nlmsghdr *nlh)
{
	struct nlattr *nla, *tb[__IFLA_MAX];
	struct ifinfomsg *ifi;
	struct tm_net *net;
	size_t len, rem;
	bool done;

	net = tm_net(tc);
	ifi = NLMSG_DATA(nlh);
	done = false;

	if (net->ifindex != ifi->ifi_index)
		goto out;

	net->ifi_flags = ifi->ifi_flags;

	len = NLMSG_SPACE(sizeof(*ifi));
	nla = (struct nlattr *)((char *)nlh + len);
	rem = nlh->nlmsg_len - len;
	nla_parse(tb, __IFLA_MAX, nla, rem);

	if ((nla = tb[IFLA_STATS64]) &&
	    nla_len(nla) >= sizeof(struct rtnl_link_stats64)) {
		struct rtnl_link_stats64 *sp;

		sp = nla_data(nla);

#define STORE_STAT(__mem)						\
	do {								\
		net->st_dif.__mem = sp->__mem - net->st_cur.__mem;	\
		net->st_cur.__mem = sp->__mem;				\
	} while (0)
		STORE_STAT(rx_packets);
		STORE_STAT(tx_packets);
		STORE_STAT(rx_bytes);
		STORE_STAT(tx_bytes);

		done = true;
	} else if ((nla = tb[IFLA_STATS]) &&
		   nla_len(nla) >= sizeof(struct rtnl_link_stats)) {
		struct rtnl_link_stats *sp;

		sp = nla_data(nla);

		STORE_STAT(rx_packets);
		STORE_STAT(tx_packets);
		STORE_STAT(rx_bytes);
		STORE_STAT(tx_bytes);

		done = true;
	} else {
		fprintf(stderr, "Can't get i/f stats.\n");
	}
out:
	return done;
}

static int tm_net_get_link(struct tm_context *tc)
{
	char buf[TM_NET_IOV_LEN];
	struct sockaddr_nl addr;
	struct tm_net *net;
	struct msghdr msg;
	struct iovec iov;
	int nlsk;

	net = tm_net(tc);

	memset(&net->st_dif, 0, sizeof(net->st_dif));

	return tm_net_send_nl_request(&nlsk, &msg, &addr, &iov, buf,
				      RTM_GETLINK) ||
	       tm_net_recv_nl_response(tc, nlsk, &msg, RTM_NEWLINK,
				       tm_net_recv_rtm_newlink);
}

static void tm_net_ip_state_update(struct tm_context *tc)
{
	struct tm_net *net;
	const char *p;
	int len;

	net = tm_net(tc);

	if (net->ifi_flags & IFF_UP) {
		p = "up";
		len = 2;
	} else {
		p = "down";
		len = 4;
	}

	tm_item_cmp_and_update(&net->item_if_state[1], p, len);
	tm_item_cmp_and_update(&net->item_if_state[2], net->dot_notion,
			       net->dn_len);
}

static void
tm_net_stat_update(struct tm_item *item_stat, struct tm_item *item_unit,
		   const char *fmt_unit, double data)
{
	const char *units, *p, *fmt;
	char str[16];
	double rd;
	int len;

	units = p = " kMGTPE";

	while (round(data) >= 1000) {
		data /= 1000.0;
		p++;
	}

	rd = round(data);

	if (rd >= 100)
		fmt = "%.0f";
	else if (rd >= 10)
		fmt = "%.1f";
	else
		fmt = "%.2f";

	len = sprintf(str, fmt, data);
	tm_item_cmp_and_update(item_stat, str, len);

	if (p == units)
		fmt_unit++;
	len = sprintf(str, fmt_unit, *p);
	tm_item_cmp_and_update(item_unit, str, len);
}

static void tm_net_ip_stat_update(struct tm_context *tc)
{
	struct tm_net *net;

	net = tm_net(tc);

	tm_net_stat_update(&net->item_if_stat[1], &net->item_if_stat[2],
			   " %cbps", (double)net->st_dif.tx_bytes * 8.0 /
				     ((double)net->interval / 1000.0));
	tm_net_stat_update(&net->item_if_stat[4], &net->item_if_stat[5],
			   " %cB", (double)net->st_cur.tx_bytes);
	tm_net_stat_update(&net->item_if_stat[7], &net->item_if_stat[8],
			   " %cbps", (double)net->st_dif.rx_bytes * 8.0 /
				     ((double)net->interval / 1000.0));
	tm_net_stat_update(&net->item_if_stat[10], &net->item_if_stat[11],
			   " %cB", (double)net->st_cur.rx_bytes);
}

static int tm_net_timer(struct tm_context *tc)
{
	int err;

	err = tm_net_get_addr(tc) || tm_net_get_link(tc);
	if (err)
		goto out;

	tm_net_ip_state_update(tc);
	tm_net_ip_stat_update(tc);
out:
	return err;
}

static struct tm_thread_timer timer_net = {
	.timer_cb	= tm_net_timer,
	.expires_msecs	= 1000
};

static int tm_net_init(struct tm_context *tc, int argc, char **argv)
{
	struct tm_net *net;
	int err;

	net = tm_net(tc);

	net->if_name = "wlan0";
	net->interval = 1000;
	net->net_fg = 0x806600;
	net->net_hi = 0xc83737;

	err = tm_net_parse_opts(net, argc, argv);
	if (err)
		goto out;

	err = tm_net_get_ifindex(net);
	if (err)
		goto out;

	/* Load icon. */
	err = tm_x_load_icon(tc, "wifi.svg", TM_ICON_MAIN,
			     &net->icon_net);
	if (err)
		goto out;

	err = tm_x_load_icon(tc, "icon5.svg", TM_ICON_SIDE | TM_ICON_FLIP,
			     &net->icon5);
	if (err)
		goto err;

	tm_net_item_if_state_init(tc);
	tm_net_item_if_stat_init(tc);

	/* Get i/f state and stats periodically. */
	timer_net.expires_msecs = net->interval;
	tm_thread_timer_add(&timer_net);

	err = tm_net_timer(tc);
out:
	return err;
err:
	tm_x_unload_icon(&net->icon_net);
	goto out;
}

static void tm_net_exit(struct tm_context *tc)
{
	struct tm_net *net;

	net = tm_net(tc);

	/* Stop timer. */
	tm_thread_timer_del(&timer_net);

	/* Unload icon. */
	tm_x_unload_icon(&net->icon5);
	tm_x_unload_icon(&net->icon_net);
}

static void tm_net_help(struct tm_context *tc)
{
	printf("\n\tnet\n"
	       "\t--if_name <INTERFACE-NAME>\n"
	       "\t\tSpecify interface name.\n"
	       "\t--net_interval <INTERVAL>\n"
	       "\t\tSpecify interval time in milliseconds.\n"
	       "\t\te.g.: 1000 gives 1 sec.\n"
	       "\t--net_fg <COLOR>\n"
	       "\t\tforeground used for text in net.\n"
	       "\t--net_hi<COLOR>\n"
	       "\t\thighlight color used for number.\n");
}

static void tm_net_get_area(struct tm_context *tc, struct tm_area *area)
{
	struct tm_net *net;

	net = tm_net(tc);

	area->width = tm_x_width(tc);
	area->height = net->icon_net.height + tm_x_margin_icon(tc) +
		       tm_x_font_max_height(tc) * 2.0;
}

static void tm_net_draw(struct tm_context *tc)
{
	struct tm_area area;
	struct tm_net *net;

	net = tm_net(tc);

	tm_net_get_area(tc, &area);

	/* icon */
	tm_x_draw_icon(tc, &net->icon_net, 0, 0);
	tm_x_draw_icon(tc, &net->icon5,
		       (int)(area.width - net->icon5.width),
		       (int)(area.height - net->icon5.height));

	/* i/f state */
	tm_x_draw_text(tc, net->item_if_state, ARRAY_SIZE(net->item_if_state));

	/* i/f stat */
	tm_x_draw_text(tc, net->item_if_stat, ARRAY_SIZE(net->item_if_stat));
}

static struct tm_object tm_object_net = {
	.obj_size	= sizeof(struct tm_net),
	.init		= tm_net_init,
	.exit		= tm_net_exit,
	.help		= tm_net_help,
	.get_area	= tm_net_get_area,
	.draw		= tm_net_draw
};

__attribute__((constructor))
static void tm_net_constructor(void)
{
	int err;

	err = tm_object_register(TM_OBJECT_NET, &tm_object_net);
	if (err)
		panic();
}
