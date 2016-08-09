///////////////////////////////////////////////////////////////////////////////
///
/// @file CmdParam.cpp
///
/// Processing prlctl command line options
///
/// @author igor@
///
/// Copyright (c) 2005-2015 Parallels IP Holdings GmbH
///
/// This file is part of OpenVZ. OpenVZ is free software; you can redistribute
/// it and/or modify it under the terms of the GNU General Public License as
/// published by the Free Software Foundation; either version 2 of the License,
/// or (at your option) any later version.
/// 
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU General Public License for more details.
/// 
/// You should have received a copy of the GNU General Public License
/// along with this program; if not, write to the Free Software
/// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
/// 02110-1301, USA.
///
/// Our contact details: Parallels IP Holdings GmbH, Vordergasse 59, 8200
/// Schaffhausen, Switzerland.
///
///////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <bitset>

// #include "Interfaces/ParallelsDomModel.h"
#define XML_DATE_FORMAT								"yyyy-MM-dd"

#include "CmdParam.h"
#include "GetOpt.h"
#include "Logger.h"
#include "Utils.h"
#include "PrlDev.h"
#if defined(_WIN_)
#include <direct.h>
#include <io.h>
#define getcwd _getcwd
#define access _access
#else
#include <unistd.h>
#endif

bool g_vzcompat_mode = false;
bool g_problem_report_cmd = false;

std::vector<std::string> LoginInfo::g_passwds_stack;

static CmdParamData invalid_action;
#define OPTION_GLOBAL					\
	{"verbose", 'v', OptRequireArg, CMD_VERBOSE},	\
	{"timeout", '\0', OptRequireArg, CMD_TIMEOUT},	\
	{"login", 'l', OptRequireArg, CMD_LOGIN},	\
	{"read-passwd", 'p', OptRequireArg, CMD_PASSWD}, \
	{"compat", '\0', OptNoArg, CMD_VZCOMPAT},


static Option no_options[] = {
	OPTION_GLOBAL
	OPTION_END
};

static Option stop_options[] = {
	OPTION_GLOBAL
	{"fast", '\0', OptNoArg, CMD_FAST},
	{"kill", 'k', OptNoArg, CMD_FAST},
	{"acpi", '\0', OptNoArg, CMD_USE_ACPI},
	{"force", '\0', OptNoArg, CMD_FORCE},
	{"noforce", '\0', OptNoArg, CMD_NOFORCE},
	OPTION_END
};

static Option destroy_options[] = {
	OPTION_GLOBAL
	{"force", '\0', OptNoArg, CMD_FORCE},
	OPTION_END
};

static Option clone_options[] = {
	OPTION_GLOBAL
	{"location", '\0', OptRequireArg, CMD_LOCATION},
		{"dst", '\0', OptRequireArg, CMD_LOCATION},
	{"name", 'n', OptRequireArg, CMD_NAME},
	{"template", 't', OptNoArg, CMD_TMPL},
	{"changesid", 's', OptNoArg, CMD_CHANGE_SID},
	{"linked", '\0', OptNoArg, CMD_LINKED_CLONE},
	{"detach-external-hdd", '\0', OptRequireArg, CMD_DETACH_EXTERNAL_HDD},
	{"online", '\0', OptNoArg, CMD_ONLINE_CLONE},
	OPTION_END
};

static Option auth_options[] = {
	OPTION_GLOBAL
	{"username", '\0', OptRequireArg, CMD_USERNAME},
	{"userpassword", '\0', OptRequireArg, CMD_USERPASSWORD},
	OPTION_END
};

static Option create_options[] = {
	OPTION_GLOBAL
	{"location", '\0', OptRequireArg, CMD_LOCATION},
		{"dst", '\0', OptRequireArg, CMD_LOCATION},
	{"ostype", 'o', OptRequireArg, CMD_OSTYPE},
	{"distribution", 'd', OptRequireArg, CMD_DIST},
	{"config", 'c', OptRequireArg, CMD_CONFIG},
	{"ostemplate", 't', OptRequireArg, CMD_OSTEMPLATE},
	{"changesid", 's', OptNoArg, CMD_CHANGE_SID},
	{"vmtype", '\0', OptRequireArg, CMD_VMTYPE},
	{"no-hdd", '\0', OptNoArg, CMD_NO_HDD},
	{"hdd-block-size", '\0', OptRequireArg, CMD_HDD_BLOCK_SIZE},
	{"lion-recovery", '\0', OptNoArg, CMD_LION_RECOVERY},
	{"uuid", '\0', OptRequireArg, CMD_UUID},
	OPTION_END
};

static Option convert_options[] = {
	OPTION_GLOBAL
	{"location", '\0', OptRequireArg, CMD_LOCATION},
		{"dst", '\0', OptRequireArg, CMD_LOCATION},
	{"force", 'f', OptNoArg, CMD_FORCE},
	OPTION_END
};

static Option register_options[] = {
	OPTION_GLOBAL
	{"force", '\0', OptNoArg, CMD_FORCE},
	{"ignore-ha-cluster", '\0', OptNoArg, CMD_IGNORE_HA_CLUSTER},
	{"preserve-uuid", '\0', OptNoArg, CMD_PRESERVE_UUID},
	{"regenerate-src-uuid", '\0', OptNoArg, CMD_REGENERATE_SRC_UUID},
	{"uuid", '\0', OptRequireArg, CMD_UUID},
	OPTION_END
};

static Option set_options[] = {
	OPTION_GLOBAL
	/*
	--device-del <name> [--destroy-image|--detach-only]
	--device-add <hdd | cdrom | fdd | net> [DEV_OPTIONS]
			[--enable | --disable]
	HDD
           --device-add <hdd> [--image <image>]
                [--size 32M] [--split]
		[--iface <ide |scsi>] [--passthr] [--position <n>]
           --device-add <hdd> --device <real_name>]
		[--iface <ide | scsi>] [--passthr] [--position <n>]
	   --device-set <name> [--iface <ide | scsi>]
			[--passthr] [--position <n>]
	CDROM
	  --device-add <cdrom> {--device <name> | --image <image>}
		[--iface <ide | scsi>] [--passthrought] [--position <n>]
	NET
	  --device_add <net> --type <shared|host> [--mac <addr>]
	  --device-add <net> --type <bridged> [--iface <name>] [--mac <addr>]
	  --device-add <net> --network <network_id> [--mac <addr>]

        */
	{"device-add", '\0', OptRequireArg, CMD_DEVICE_ADD},
		{"netif_add", '\0', OptRequireArg, CMD_NETIF_ADD},
	{"device-set", '\0', OptRequireArg, CMD_DEVICE_SET},
	{"device-del", '\0', OptRequireArg, CMD_DEVICE_DEL},
		{"netif_del", '\0', OptRequireArg, CMD_NETIF_DEL},
	{"device-connect", '\0', OptRequireArg, CMD_DEVICE_CONNECT},
	{"device-disconnect", '\0', OptRequireArg, CMD_DEVICE_DISCONNECT},
	{"device-bootorder", '\0', OptRequireArg, CMD_BOOTORDER},

	{"device", '\0', OptRequireArg, CMD_DEVICE},
	{"distribution", 'd', OptRequireArg, CMD_DIST},
	{"image", '\0', OptRequireArg, CMD_IMAGE},
	{"mnt", '\0', OptRequireArg, CMD_MNT},
	{"recreate", '\0', OptNoArg, CMD_RECREATE},
	{"type", 't', OptRequireArg, CMD_DEV_TYPE},
	{"adapter-type", '\0', OptRequireArg, CMD_ADAPTER_TYPE},
	{"size", 's', OptRequireArg, CMD_SIZE},
	{"offline", 's', OptNoArg, CMD_OFFLINE},
	{"no-fs-resize", 's', OptNoArg, CMD_NO_FS_RESIZE},
	{"diskspace", '\0', OptRequireArg, CMD_SIZE},
	{"split", '\0', OptNoArg, CMD_SPLIT},
	{"enable", 'e', OptNoArg, CMD_ENABLE},
	{"disable", '\0', OptNoArg, CMD_DISABLE},
	{"connect", 'c', OptNoArg, CMD_CONNECT},
	{"disconnect", '\0', OptNoArg, CMD_DISCONNECT},

	{"hdd-block-size", '\0', OptRequireArg, CMD_HDD_BLOCK_SIZE},

	{"iface", '\0', OptRequireArg, CMD_IFACE},
	{"subtype", '\0', OptRequireArg, CMD_SUBTYPE},
	{"passthr", '\0', OptRequireArg, CMD_PASSTHR},
	{"position", '\0', OptRequireArg, CMD_POSITION},
	{"mac", '\0', OptRequireArg, CMD_MAC},
	/*
	  --cpus <N> --memsize <N> --videosize <N>
	*/
	{"cpus", '\0', OptRequireArg, CMD_CPUS},
	{"cpu-hotplug", '\0', OptRequireArg, CMD_CPU_HOTPLUG},
	{"memsize", '\0', OptRequireArg, CMD_MEMSIZE},
	{"videosize", '\0', OptRequireArg, CMD_VIDEOSIZE},
	{"3d-accelerate", '\0', OptRequireArg, CMD_3D_ACCELERATE},
	{"vertical-sync", '\0', OptRequireArg, CMD_VERTICAL_SYNC},
	{"high-resolution", '\0', OptRequireArg, CMD_HIGH_RESOLUTION},
	{"mem-hotplug", '\0', OptRequireArg, CMD_MEM_HOTPLUG},
	{"memquota", '\0', OptRequireArg, CMD_MEMQUOTA},
	{"memguarantee", '\0', OptRequireArg, CMD_MEMGUARANTEE},
	{"applyconfig", '\0', OptRequireArg, CMD_CONFIG},
	{"description", '\0', OptRequireArg, CMD_DESC},
	{"name", '\0', OptRequireArg, CMD_VM_NAME},
	{"rename-ext-disks", '\0', OptNoArg, CMD_VM_RENAME_EXT_DISKS},
	{"template", '\0', OptRequireArg, CMD_TEMPLATE},
	{"smart-mouse-optimize", '\0', OptRequireArg, CMD_SMART_MOUSE_OPTIMIZE},
	{"sticky-mouse", '\0', OptRequireArg, CMD_STICKY_MOUSE},
	{"keyboard-optimize", '\0', OptRequireArg, CMD_KEYBOARD_OPTIMIZE},
	{"sync-host-printers", '\0', OptRequireArg, CMD_SYNC_HOST_PRINTERS},
	{"sync-default-printer", '\0', OptRequireArg, CMD_SYNC_DEFAULT_PRINTER},
	{"auto-share-camera", '\0', OptRequireArg, CMD_AUTO_SHARE_CAMERA},
	{"auto-share-bluetooth", '\0', OptRequireArg, CMD_AUTO_SHARE_BLUETOOTH},
	{"support-usb30", '\0', OptRequireArg, CMD_SUPPORT_USB30},
	{"efi-boot", '\0', OptRequireArg, CMD_EFI_BOOT},
	{"select-boot-device", '\0', OptRequireArg, CMD_SELECT_BOOT_DEV},
	{"external-boot-device", '\0', OptRequireArg, CMD_EXT_BOOT_DEV},
	{"output", '\0', OptRequireArg, CMD_OUTPUT},
	{"socket", '\0', OptRequireArg, CMD_SOCKET},
	{"socket-tcp", '\0', OptRequireArg, CMD_SOCKET_TCP},
	{"socket-udp", '\0', OptRequireArg, CMD_SOCKET_UDP},
	{"socket-mode", '\0', OptRequireArg, CMD_SOCKET_MODE},

	{"mixer", '\0', OptRequireArg, CMD_MIXER},
	{"input", '\0', OptRequireArg, CMD_MIXER},

	{"autostart", '\0', OptRequireArg, CMD_AUTOSTART},
		{"onboot", '\0', OptRequireArg, CMD_AUTOSTART},
	{"autostart-delay", '\0', OptRequireArg, CMD_AUTOSTART_DELAY},
	{"autostop", '\0', OptRequireArg, CMD_AUTOSTOP},
	{"startup-view",	'\0', OptRequireArg, CMD_STARTUP_VIEW},
	{"on-shutdown",		'\0', OptRequireArg, CMD_ON_SHUTDOWN},
	{"on-window-close",	'\0', OptRequireArg, CMD_ON_WINDOW_CLOSE},
	{"undo-disks",		'\0', OptRequireArg, CMD_UNDO_DISKS},

	{"system-flags", 'f', OptRequireArg, CMD_FLAGS},

	{"faster-vm",		'\0', OptRequireArg, CMD_FASTER_VM},
	{"adaptive-hypervisor",		'\0', OptRequireArg, CMD_ADAPTIVE_HYPERVISOR},
	{"auto-compress",		'\0', OptRequireArg, CMD_AUTO_COMPRESS},
	{"nested-virt",		'\0', OptRequireArg, CMD_NESTED_VIRT},
	{"pmu-virt",		'\0', OptRequireArg, CMD_PMU_VIRT},
	{"longer-battery-life",		'\0', OptRequireArg, CMD_LONGER_BATTERY_LIFE},
	{"battery-status",		'\0', OptRequireArg, CMD_BATTERY_STATUS},

	{"winsystray-in-macmenu", '\0', OptRequireArg, CMD_WINSYSTRAY_IN_MACMENU},
	{"auto-switch-fullscreen", '\0', OptRequireArg, CMD_AUTO_SWITCH_FULLSCREEN},
	{"disable-aero", '\0', OptRequireArg, CMD_DISABLE_AERO},
	{"hide-min-windows", '\0', OptRequireArg, CMD_HIDE_MIN_WINDOWS},

	{"require-pwd", '\0', OptRequireArg, CMD_REQUIRE_PWD},
	{"lock-edit-settings", '\0', OptRequireArg, CMD_LOCK_EDIT_SETTINGS},
	{"lock-on-suspend", '\0', OptRequireArg, CMD_LOCK_ON_SUSPEND},
	{"isolate-vm", '\0', OptRequireArg, CMD_ISOLATE_VM},
	{"smart-guard", '\0', OptRequireArg, CMD_SMART_GUARD},
	{"sg-notify-before-create", '\0', OptRequireArg, CMD_SG_NOTIFY_BEFORE_CREATE},
	{"sg-interval", '\0', OptRequireArg, CMD_SG_INTERVAL},
	{"sg-max-snapshots", '\0', OptRequireArg, CMD_SG_MAX_SNAPSHOTS},
	{"expiration",		'\0', OptRequireArg, CMD_EXPIRATION},

	{"vnc-mode",		'\0', OptRequireArg, CMD_VNC_MODE},
	{"vnc-port",		'\0', OptRequireArg, CMD_VNC_PORT},
	{"vnc-passwd",		'\0', OptRequireArg, CMD_VNC_PASSWD},
	{"vnc-nopasswd",	'\0', OptNoArg, CMD_VNC_NOPASSWD},
	{"vnc-address",		'\0', OptRequireArg, CMD_VNC_ADDRESS},

	{"features",		'\0', OptRequireArg, CMD_FEATURES},

	{"ifname",		'\0', OptRequireArg, CMD_IFNAME},
	{"searchdomain",	'\0', OptRequireArg, CMD_SEARCHDOMAIN},
	{"hostname",		'\0', OptRequireArg, CMD_HOSTNAME},
	{"nameserver",		'\0', OptRequireArg, CMD_NAMESERVER},
	{"apply-iponly",	'\0', OptRequireArg, CMD_APPLY_IPONLY},
	{"ipset",		'\0', OptRequireArg, CMD_IP_SET},
	{"ipadd",		'\0', OptRequireArg, CMD_IP_ADD},
	{"ipdel",		'\0', OptRequireArg, CMD_IP_DEL},
	{"gw",			'\0', OptRequireArg, CMD_GW},
	{"gw6",			'\0', OptRequireArg, CMD_GW6},
	{"dhcp",		'\0', OptRequireArg, CMD_DHCP},
	{"dhcp6",		'\0', OptRequireArg, CMD_DHCP6},
	{"configure",		'\0', OptRequireArg, CMD_CONFIGURE},
	{"preventpromisc",	'\0', OptRequireArg, CMD_PREVENT_PROMISC},
	{"ipfilter",		'\0', OptRequireArg, CMD_IP_FILTER},
	{"macfilter",		'\0', OptRequireArg, CMD_MAC_FILTER},
	{"userpasswd",		'\0', OptRequireArg, CMD_USERPASSWD},
	{"host-admin",		'\0', OptRequireArg, CMD_HOST_ADMIN},
	{"crypted",		'\0', OptNoArg, CMD_CRYPTED},
	{"usedefanswers",	'\0', OptRequireArg, CMD_USE_DEFAULT_ANSWERS},
	{"tools-autoupdate",	'\0', OptRequireArg, CMD_TOOLS_AUTOUPDATE},
	{"smart-mount",		'\0', OptRequireArg, CMD_SMART_MOUNT},

	{"fw",			'\0', OptRequireArg, CMD_FW},
	{"fw-policy",		'\0', OptRequireArg, CMD_FW_POLICY},
	{"fw-rule",		'\0', OptRequireArg, CMD_FW_RULE},
	{"fw-direction",	'\0', OptRequireArg, CMD_FW_DIRECTION},
	{"template",		'\0', OptRequireArg, CMD_TEMPLATE_SIGN},

	{"network", '\0', OptRequireArg, CMD_VNETWORK},

#if defined(_LIN_)
	{"ioprio",		'\0', OptRequireArg, CMD_IOPRIO},
	{"iolimit",		'\0', OptRequireArg, CMD_IOLIMIT},
	{"iopslimit",		'\0', OptRequireArg, CMD_IOPSLIMIT},
	{"cpuunits",		'\0', OptRequireArg, CMD_CPUUNITS},
	{"cpulimit",		'\0', OptRequireArg, CMD_CPULIMIT},
	{"cpumask",		'\0', OptRequireArg, CMD_CPUMASK},
	{"nodemask",		'\0', OptRequireArg, CMD_NODEMASK},
	{"apptemplate",		'\0', OptRequireArg, CMD_APPTEMPLATE},

	{"swappages",		'\0', OptRequireArg, CMD_SWAPPAGES},
	{"swap",		'\0', OptRequireArg, CMD_SWAP},
	{"quotaugidlimit",	'\0', OptRequireArg, CMD_QUOTAUGIDLIMIT},

	{"capability",	'\0', OptRequireArg, CMD_SET_CAP},

	{"netfilter",	'\0', OptRequireArg, CMD_NETFILTER},

	{"ha-enable", '\0', OptRequireArg, CMD_HA_ENABLE},
		{"ha_enable", '\0', OptRequireArg, CMD_HA_ENABLE},
		{"ha", '\0', OptRequireArg, CMD_HA_ENABLE},
	{"ha-prio", '\0', OptRequireArg, CMD_HA_PRIO},
		{"ha_prio", '\0', OptRequireArg, CMD_HA_PRIO},

	{"backup-add", '\0', OptRequireArg, CMD_ATTACH_BACKUP_ID},
	{"disk", '\0', OptRequireArg, CMD_ATTACH_BACKUP_DISK},
	{"backup-del", '\0', OptRequireArg, CMD_DETACH_BACKUP_ID},
#endif
#if defined(_WIN_)
	{"apptemplate",		'\0', OptRequireArg, CMD_APPTEMPLATE},
	{"cpuunits",		'\0', OptRequireArg, CMD_CPUUNITS},
	{"cpulimit",		'\0', OptRequireArg, CMD_CPULIMIT},
#endif
	{"offline-management",	'\0', OptRequireArg, CMD_OFF_MAN},
		{"offline_management",	'\0', OptRequireArg, CMD_OFF_MAN},
		{"offman",  '\0', OptRequireArg, CMD_OFF_MAN},
	{"offline-service",	'\0', OptRequireArg, CMD_OFF_SRV},
		{"offline_service",	'\0', OptRequireArg, CMD_OFF_SRV},
		{"offsrv",	'\0', OptRequireArg, CMD_OFF_SRV},

	{"rate",		'\0', OptRequireArg, CMD_RATE},
	{"ratebound",		'\0', OptRequireArg, CMD_RATEBOUND},
	{"destroy-image",	'\0', OptNoArg, CMD_DESTROY_HDD },
	{"destroy-image-force",	'\0', OptNoArg, CMD_DESTROY_HDD_FORCE },
	{"detach-only",		'\0', OptNoArg, CMD_DETACH_HDD},
	{"password-to-edit",'\0', OptNoArg, CMD_SET_RESTRICT_EDITING},
	{"autocompact",		'\0', OptRequireArg, CMD_AUTOCOMPACT},
        OPTION_END
};

static Option disp_set_options[] = {
	OPTION_GLOBAL
	{"mem-limit", 'm', OptRequireArg, CMD_MEMORY_LIMIT},
	{"mng-settings", '\0', OptRequireArg, CMD_USER_MNG_SETTINGS},
	{"min-security-level", 's', OptRequireArg, CMD_MIN_SECURITY_LEVEL},
	{"assignment", '\0', OptRequireArg, CMD_ASSIGNMENT},
	{"device", '\0', OptRequireArg, CMD_DEVICE},
	{"cep", 'c', OptRequireArg, CMD_CEP_MECH_SETTINGS},
	{"verbose-log", '\0', OptRequireArg, CMD_VERBOSE_LOG_LEVEL},
	{"backup-path", '\0', OptRequireArg, CMD_BACKUP_PATH},
	{"backup-tmpdir", '\0', OptRequireArg, CMD_BACKUP_TMPDIR},
	{"backup-storage", '\0', OptRequireArg, CMD_BACKUP_STORAGE},
		{"def-backup-storage", '\0', OptRequireArg, CMD_BACKUP_STORAGE},
		{"default-backup-storage", '\0', OptRequireArg, CMD_BACKUP_STORAGE},
	{"backup-timeout", '\0', OptRequireArg, CMD_BACKUP_TIMEOUT},
	{"idle-connection-timeout", '\0', OptRequireArg, CMD_BACKUP_TIMEOUT},

	{"add-offsrv", '\0', OptRequireArg, CMD_UPDATE_OFFLINE_SERVICE},
	{"del-offsrv", '\0', OptRequireArg, CMD_DEL_OFFLINE_SERVICE},

	{"add-network-class", '\0', OptRequireArg, CMD_ADD_NETWORK_CLASS},
	{"del-network-class", '\0', OptRequireArg, CMD_DEL_NETWORK_CLASS},
	{"add-shaping-entry", '\0', OptRequireArg, CMD_ADD_SHAPING_ENTRY},
	{"del-shaping-entry", '\0', OptRequireArg, CMD_DEL_SHAPING_ENTRY},
	{"add-bandwidth-entry", '\0', OptRequireArg, CMD_ADD_BANDWIDTH_ENTRY},
	{"del-bandwidth-entry", '\0', OptRequireArg, CMD_DEL_BANDWIDTH_ENTRY},

	{"shaping", '\0', OptRequireArg, CMD_SHAPING_ENABLE},

	{"log-rotation", '\0', OptRequireArg, CMD_LOG_ROTATION},
	{"allow-attach-screenshots", '\0', OptRequireArg, CMD_ALLOW_ATTACH_SCREENSHOTS},
	{"require-pwd", '\0', OptRequireArg, CMD_REQUIRE_PWD},
	{"lock-edit-settings", '\0', OptRequireArg, CMD_LOCK_EDIT_SETTINGS},
	{"advanced-security-mode", '\0', OptRequireArg, CMD_ADVANCED_SECURITY_MODE},

	{"cpu-features-mask", '\0', OptRequireArg, CMD_CPU_FEATURES_MASK},
	{"vnc-public-key", '\0', OptRequireArg, CMD_VNC_PUBLIC_KEY},
	{"vnc-private-key", '\0', OptRequireArg, CMD_VNC_PRIVATE_KEY},
	{"vm-cpulimit-type", '\0', OptRequireArg, CMD_VM_CPULIMIT_TYPE},
	OPTION_END
};

static Option disp_vnet_options[] = {
	OPTION_GLOBAL
	{"name", 'n', OptRequireArg, CMD_VNET_NEW_NAME},
	{"ifname", 'i', OptRequireArg, CMD_VNET_IFACE},
	{"description", 'd', OptRequireArg, CMD_VNET_DESCRIPTION},
	{"type", 't', OptRequireArg, CMD_VNET_TYPE},
	{"mac", 'm', OptRequireArg, CMD_VNET_MAC},
	{"ip-scope-start", '\0', OptRequireArg, CMD_VNET_IP_SCOPE_START},
	{"ip-scope-end", '\0', OptRequireArg, CMD_VNET_IP_SCOPE_END},
	{"ip6-scope-start", '\0', OptRequireArg, CMD_VNET_IP6_SCOPE_START},
	{"ip6-scope-end", '\0', OptRequireArg, CMD_VNET_IP6_SCOPE_END},
	{"nat-tcp-add", '\0', OptRequireArg, CMD_VNET_NAT_TCP_ADD},
	{"nat-udp-add", '\0', OptRequireArg, CMD_VNET_NAT_UDP_ADD},
	{"nat-tcp-del", '\0', OptRequireArg, CMD_VNET_NAT_TCP_DEL},
	{"nat-udp-del", '\0', OptRequireArg, CMD_VNET_NAT_UDP_DEL},
	{"ip", '\0', OptRequireArg, CMD_VNET_HOST_IP},
	{"ip6", '\0', OptRequireArg, CMD_VNET_HOST_IP6},
	{"dhcp-server", '\0', OptRequireArg, CMD_VNET_DHCP_ENABLED},
	{"dhcp6-server", '\0', OptRequireArg, CMD_VNET_DHCP6_ENABLED},
	{"dhcp-ip", '\0', OptRequireArg, CMD_VNET_DHCP_IP},
	{"dhcp-ip6", '\0', OptRequireArg, CMD_VNET_DHCP_IP6},
	{"json", 'j', OptNoArg, CMD_USE_JSON},
	OPTION_END
};

static Option disp_privnet_options[] = {
	OPTION_GLOBAL
	{"ipadd", 'a', OptRequireArg, CMD_PRIVNET_IPADD},
	{"ipdel", 'd', OptRequireArg, CMD_PRIVNET_IPDEL},
	{"global", '\0', OptRequireArg, CMD_PRIVNET_GLOBAL},
	{"json", 'j', OptNoArg, CMD_USE_JSON},
	OPTION_END
};

#if 0
static Option disp_usb_options[] = {
	OPTION_GLOBAL
	{"list", 0, OptNoArg, CMD_USB_LIST},
	{"del", 0, OptRequireArg, CMD_USB_DELETE},
	{"set", 0, OptRequireArg, CMD_USB_SET},
	OPTION_END
};
#endif

static Option disp_usb_options[] = {
	OPTION_GLOBAL
	{"json", 'j', OptNoArg, CMD_USE_JSON},
	OPTION_END
};

static Option disp_shutdown_options[] = {
	OPTION_GLOBAL
	{"force", 'f', OptNoArg, CMD_FORCE},
	{"suspend-vm-to-pram", '\0', OptNoArg, CMD_SUSPEND_VM_TO_PRAM},
	OPTION_END
};

static Option disp_info_options[] = {
	OPTION_GLOBAL
	{"license", 'l', OptNoArg, CMD_INFO_LICENSE},
	{"json", 'j', OptNoArg, CMD_USE_JSON},
	{"full", 'f', OptNoArg, CMD_INFO_FULL},
	OPTION_END
};

static Option disp_up_listen_iface_options[] = {
	OPTION_GLOBAL
	{"listen-interface", '\0', OptRequireArg, CMD_LISTEN_INTERFACE},
	OPTION_END
};

static Option list_options[] = {
	OPTION_GLOBAL
	{"output", 'o',	OptRequireArg, CMD_LIST_FIELD},
	{"all", 'a',	OptNoArg, CMD_LIST_ALL},
	{"no-header", 'H', OptNoArg, CMD_LIST_NO_HDR},
	{"sort", 's', OptRequireArg, CMD_LIST_SORT},
	{"stopped", 'S', OptNoArg, CMD_LIST_STOPPED},
	{"name", 'n', OptNoArg, CMD_LIST_NAME},
	{"template", 't', OptNoArg, CMD_TMPL},
	{"vmtype", '\0', OptRequireArg, CMD_VMTYPE},
	{"help", '\0', OptNoArg, CMD_HELP},
	{"info", 'i', OptNoArg, CMD_INFO},
	{"json", 'j', OptNoArg, CMD_USE_JSON},
	{"full", 'f', OptNoArg, CMD_INFO_FULL},
	{"list", 'L', OptNoArg, CMD_LIST_ALL_FIELDS},
	OPTION_END
};

static Option user_list_options[] = {
	OPTION_GLOBAL
	{"output", 'o',	OptRequireArg, CMD_LIST_FIELD},
	{"all", 'a',	OptNoArg, CMD_LIST_ALL},
	{"no-header", 'H', OptNoArg, CMD_LIST_NO_HDR},
	{"sort", 's', OptRequireArg, CMD_LIST_SORT},
	{"help", '\0', OptNoArg, CMD_HELP},
	{"json", 'j', OptNoArg, CMD_USE_JSON},
	OPTION_END
};

static Option user_set_options[] = {
	OPTION_GLOBAL
	{"def-vm-home", '\0',	OptRequireArg, CMD_USER_DEF_VM_HOME},
	OPTION_END
};

static Option capture_options[] = {
	OPTION_GLOBAL
	{"file", 'f',	OptRequireArg, CMD_FILE},
	OPTION_END
};

static Option lic_options[] = {
	OPTION_GLOBAL
	{"key", 'k',	OptRequireArg, CMD_KEY},
	{"name", 'n',	OptRequireArg, CMD_NAME},
	{"company", 'c',	OptRequireArg, CMD_COMPANY},
	OPTION_END
};

static Option snapshot_options[] = {
	OPTION_GLOBAL
	{"name", 'n',	OptRequireArg, CMD_SNAPSHOT_NAME},
	{"description", 'd',	OptRequireArg, CMD_SNAPSHOT_DESC},
	{"wait", '\0',	OptNoArg, CMD_SNAPSHOT_WAIT},
	OPTION_END
};

static Option snapshot_switch_options[] = {
	OPTION_GLOBAL
	{"id", 'i',	OptRequireArg, CMD_SNAPSHOT_ID},
	{"wait", '\0',	OptNoArg, CMD_SNAPSHOT_WAIT},
	{"skip-resume", '\0',	OptNoArg, CMD_SNAPSHOT_SKIP_RESUME},
	OPTION_END
};


static Option snapshot_delete_options[] = {
	OPTION_GLOBAL
	{"id", 'i',	OptRequireArg, CMD_SNAPSHOT_ID},
	{"children", 'c', OptNoArg, CMD_SNAPSHOT_CHILDREN},
	OPTION_END
};

static Option snapshot_list_options[] = {
	OPTION_GLOBAL
	{"id", 'i',	OptRequireArg, CMD_SNAPSHOT_ID},
	{"tree", 't',	OptNoArg, CMD_SNAPSHOT_LIST_TREE},
	{"no-header", 'H', OptNoArg, CMD_LIST_NO_HDR},
	OPTION_END
};

static Option migrate_options[] = {
	OPTION_GLOBAL
	{"force", 'f',	OptRequireArg, CMD_FORCE},
	{"securitylevel", '\0',	OptRequireArg, CMD_SECURITY_LEVEL},
	{"location", '\0', OptRequireArg, CMD_LOCATION},
	{"dst", '\0', OptRequireArg, CMD_LOCATION},
	{"sessionid", 's', OptRequireArg, CMD_SESSIONID},
	{"clone", '\0', OptNoArg, CMD_CLONE_MODE},
	{"keep-src", '\0', OptNoArg, CMD_CLONE_MODE},
	{"remove-src", '\0', OptNoArg, CMD_REMOVE_BUNDLE},
	{"switch-template", '\0', OptNoArg, CMD_SWITCH_TEMPLATE},
	{"changesid", '\0', OptNoArg, CMD_CHANGE_SID},
	{"ignore-existing-bundle", '\0', OptNoArg, CMD_IGNORE_EXISTING_BUNDLE},
	{"ssh", '\0', OptRequireArg, CMD_SSH_OPTS},
	{"no-compression", '\0', OptNoArg, CMD_UNCOMPRESSED},
	{"no-tunnel", '\0', OptNoArg, CMD_NO_TUNNEL},
	OPTION_END
};

static Option move_options[] = {
	OPTION_GLOBAL
	{"location", '\0', OptRequireArg, CMD_LOCATION},
		{"dst", '\0', OptRequireArg, CMD_LOCATION},
	OPTION_END
};

static Option backup_options[] = {
	OPTION_GLOBAL
	{"full", 'f',		OptNoArg, CMD_BACKUP_FULL},
		{"full", 'F',	OptNoArg, CMD_BACKUP_FULL},
		{"full", 'I',	OptNoArg, CMD_BACKUP_FULL},
	{"incremental", 'i',	OptNoArg, CMD_BACKUP_INC},
	{"differental", 'd',	OptNoArg, CMD_BACKUP_DIFF},
	{"storage",	's',	OptRequireArg, CMD_BACKUP_STORAGE},
	{"securitylevel", '\0',	OptRequireArg, CMD_SECURITY_LEVEL},
	{"description", '\0',	OptRequireArg, CMD_DESC},
	{"uncompressed", 'u',	OptNoArg, CMD_UNCOMPRESSED},
	{"no-compression", '\0', OptNoArg, CMD_UNCOMPRESSED},
	OPTION_END
};

static Option restore_options[] = {
	OPTION_GLOBAL
	{"storage",	's',	OptRequireArg, CMD_BACKUP_STORAGE},
	{"tag",		't',	OptRequireArg, CMD_BACKUP_ID},
		{"id",	'i',	OptRequireArg, CMD_BACKUP_ID},
	{"vm",		'e',	OptRequireArg, CMD_VM_ID},
	{"name",	'n',	OptRequireArg, CMD_NAME},
	{"list",	'l',	OptNoArg, CMD_LIST},
	{"full",	'f',	OptNoArg, CMD_BACKUP_LIST_FULL},
	{"securitylevel", '\0',	OptRequireArg, CMD_SECURITY_LEVEL},
	{"location", '\0', OptRequireArg, CMD_LOCATION},
		{"dst", '\0', OptRequireArg, CMD_LOCATION},

	OPTION_END
};

static Option backup_list_options[] = {
	OPTION_GLOBAL
	{"full",	'f',	OptNoArg, CMD_BACKUP_LIST_FULL},
	{"storage",	's',	OptRequireArg, CMD_BACKUP_STORAGE},
	{"no-header",	'H',	OptNoArg, CMD_LIST_NO_HDR},
	{"localvms",	'\0',	OptNoArg, CMD_LIST_LOCAL_VM},
		{"localvm",	'\0',	OptNoArg, CMD_LIST_LOCAL_VM},
	{"securitylevel", '\0',	OptRequireArg, CMD_SECURITY_LEVEL},
	{"vmtype", '\0', OptRequireArg, CMD_VMTYPE},
	OPTION_END
};

static Option backup_delete_options[] = {
	OPTION_GLOBAL
	{"storage",	's',	OptRequireArg, CMD_BACKUP_STORAGE},
	{"tag",		't',	OptRequireArg, CMD_BACKUP_ID},
		{"id",	'i',    OptRequireArg, CMD_BACKUP_ID},
	{"securitylevel", '\0',	OptRequireArg, CMD_SECURITY_LEVEL},
	{"keep-chain", 'k',	OptNoArg, CMD_BACKUP_KEEP_CHAIN},
	OPTION_END
};


static Option statistics_options[] = {
	OPTION_GLOBAL
	{"all"     , 'a' , OptNoArg     , CMD_LIST_ALL},
	{"loop"    , 'l' , OptNoArg     , CMD_LOOP},
	{"filter"  , '\0', OptRequireArg, CMD_PERF_FILTER},
	OPTION_END
};

static Option problem_report_options[] = {
	OPTION_GLOBAL
	{"send"     , 's' , OptNoArg     , CMD_SEND_PROBLEM_REPORT},
	{"dump"     , 'd' , OptNoArg     , CMD_DUMP_PROBLEM_REPORT},
	{"no-proxy"     , '\0' , OptNoArg     , CMD_DONT_USE_PROXY},
	{"proxy"    , '\0' , OptRequireArg     , CMD_USE_PROXY},
	{"stand-alone"     , '\0' , OptNoArg     , CMD_CREATE_PROBLEM_REPORT_WITHOUT_SERVER},
	{"name"     , '\0' , OptRequireArg     , CMD_PROBLEM_REPORT_USER_NAME},
	{"email"     , '\0' , OptRequireArg     , CMD_PROBLEM_REPORT_USER_EMAIL},
	{"description"     , '\0' , OptRequireArg     , CMD_PROBLEM_REPORT_DESCRIPTION},
	OPTION_END
};

static Option appliance_options[] = {
	OPTION_GLOBAL
	{"file", '\0', OptRequireArg, CMD_FILE},
	{"batch", 'b', OptNoArg, CMD_BATCH},
	OPTION_END
};

static Option ct_template_copy_options[] = {
	OPTION_GLOBAL
	{"force", 'f',	OptNoArg, CMD_FORCE},
	{"securitylevel", '\0',	OptRequireArg, CMD_SECURITY_LEVEL},
	OPTION_END
};

static Option mount_options[] = {
	OPTION_GLOBAL
	{"options", 'o', OptRequireArg, CMD_MNT_OPT},
	{"info", '\0', OptNoArg, CMD_MNT_INFO},
	OPTION_END
};

static Option start_options[] = {
	OPTION_GLOBAL
	{"wait", '\0', OptNoArg, CMD_WAIT},
	OPTION_END
};

static Option exec_options[] = {
	OPTION_GLOBAL
	{"without-shell", '\0', OptNoArg, CMD_EXEC_NO_SHELL},
	OPTION_END
};

static const char *version()
{
	return VER_PRODUCTVERSION_STR;
}

static void print_version(const char * argv0)
{
	printf("%s version %s"
#ifndef EXTERNALLY_AVAILABLE_BUILD
			" internal build"
#endif
			"\n", prl_basename(argv0), version());
}

static void usage_vm(const char * argv0)
{
	print_version(argv0);
	printf("\n");
	printf(
"Usage: %s ACTION <ID | NAME> [OPTIONS] [-l user[[:passwd]@server[:port]]\n"
"Supported actions are:\n"
"  backup <ID | NAME> [-s,--storage <user[[:passwd]@server[:port]>] [--description <desc>]\n"
"    [-f,--full | -i,--incremental] [--no-compression]\n"
"  backup-list [ID | NAME] [-f,--full] [--vmtype ct|vm|all] [--localvms]\n"
"    [-s,--storage <user[[:passwd]@server[:port]>]\n"
"  backup-delete {<ID> | -t,--tag <backupid>} [--keep-chain] [-s,--storage <user[[:passwd]@server[:port]>]\n"
"  restore {<ID> | -t,--tag <backupid>} [-s,--storage <user[[:passwd]@server[:port]>]\n"
"    [-n,--name <new_name>] [--dst <path>]\n"
"  clone <ID | NAME> --name <NEW_NAME> [--template]] [--dst path] [--changesid] [--linked] [--detach-external-hdd <yes|no>]\n"
"  console <ID | NAME>\n"
"  create <NAME> {--ostemplate <name> | -o, --ostype <name|list> | -d,--distribution <name|list>} [--vmtype ct|vm]\n"
"                [--dst <path>] [--changesid] [--no-hdd]\n"
"  delete <ID | NAME>\n"
//"  installtools <ID | NAME>\n"
"  enter <ID | NAME>\n"
"  exec <ID | NAME> [--without-shell] <command> [arg ...]\n"
"  list [-a,--all] [-t,--template] [--vmtype ct|vm|all] [-L] [-o,--output name[,name...]] [-s,--sort name]\n"
"  list -i,--info [-f,--full] [-j, --json] [<ID | NAME>] [--vmtype ct|vm|all]\n"
"  migrate <[src_node/]ID> <dst_node[/NAME]> [--dst <path>] [--changesid] [--clone|--remove-src] [--no-compression] [--no-tunnel] [--ssh <options>]\n"
"  pause <ID | NAME>\n"
"  register <PATH> [--preserve-uuid | --uuid <UUID>] [--regenerate-src-uuid] [--force]\n"
"  reset <ID | NAME>\n"
"  resume <ID | NAME>\n"
"  restart <ID | NAME>\n"
"  start <ID | NAME>\n"
"  status <ID | NAME>\n"
"  change-sid <ID | NAME>\n"
"  stop <ID | NAME> [--kill | --noforce]\n"
"  snapshot <ID | NAME> [-n,--name <name>] [-d,--description <desc>]\n"
"  snapshot-delete <ID | NAME> -i,--id <snapid> [-c,--children]\n"
"  snapshot-list <ID | NAME> [-t,--tree] [-i,--id <snapid>]\n"
"  snapshot-switch <ID | NAME> -i,--id <snapid> [--skip-resume]\n"
"  suspend <ID | NAME>\n"
//"  statistics <ID | NAME> [--loop] [--filter name]\n"
"  unregister <ID | NAME>\n"
"  reset-uptime <ID | NAME>\n"
#ifdef _LIN_
"  mount <ID | NAME> [{-o ro|rw | --info}]\n"
"  umount <ID | NAME>\n"
#endif
"  move <vm_id|vm_name> --dst <path>\n"
"  problem-report <ID | NAME> <-d,--dump|-s,--send [--proxy [user[:password]@proxyhost[:port]]]> "
	"[--no-proxy] [--name <your name>] [--email <your E-mail>] [--description <problem description>]\n"
"  statistics {<ID | NAME> | <-a,--all>} [--filter <filter>] [--loop]\n"
"  set <ID | NAME>\n"
"    [--memguarantee <auto|value>] [--mem-hotplug <on|off>]\n"
"    [--applyconfig <conf>] [--tools-autoupdate <yes|no>]\n"
"    [--vnc-mode <auto | manual | off>] [--vnc-port <port>] [{--vnc-passwd <passwd> | --vnc-nopasswd}]\n"
"    [--cpu-hotplug <on|off>]\n"
"    [--distribution <name|list>]\n"
"    [--cpuunits <N>] [--cpulimit <N>] [--cpumask <{N[,N,N1-N2]|all}>] [--nodemask <{N[,N,N1-N2]|all}>]\n"
"    [--rate <class:KBits>] [--ratebound <yes|no>]\n"
"    [--ioprio <priority>] [--iolimit <limit>] [--iopslimit <limit>]\n"
"    [--offline-management <on|off>] [--offline-service <service_name>]\n"
"    [--hostname <hostname>] [--nameserver <addr>] [--searchdomain <addr>]\n"
"    [--userpasswd <user:passwd> [--host-admin <name>]]\n"
"    [--usedefanswers <on | off>]\n"
"    [--ha-enable <yes|no>] [--ha-prio <priority>]\n"
"    [--password-to-edit]\n"
"    [--template <yes|no>]\n"
"    [General options]\n"
"    [Container management options]\n"
"    [Boot order management options]\n"
"    [Video options]\n"
"    [Device management options]\n"
"    [Startup and shutdown options]\n"
"    [Optimization options]\n"
"    [Shared folder options]\n"
"General options are:\n"
"    [--name <name>] [--cpus <N>] [--memsize <n>]\n"
"    [--description <desc>]\n"
"    [--template <on | off>]\n"
"    [--rename-ext-disks]\n"
"Container management options are:\n"
"    [--swappages P[:P]] [--swap N] [--quotaugidlimit <n>]\n"
"    [--autocompact <on | off>]\n"
"    [--netfilter <disabled | stateless | stateful | full>]\n"
"    [--features <name>:<on|off>[,<name>:<on:off>...]]\n"
"Boot order management options are:\n"
"	 [--device-bootorder \"<name1 name2 ...>\"]\n"
"    [--efi-boot <on | off>] [--select-boot-device <on | off>]\n"
"    [--external-boot-device <name>]\n"
"Video options are:\n"
"    [--videosize <n>] [--3d-accelerate <off|highest|dx9>]\n"
"    [--vertical-sync <on | off>]\n"
"Device management options are:\n"
"	--device-connect <name>\n"
"	--device-disconnect <name>\n"
"	--device-del <name> [--destroy-image|--destroy-image-force|--detach-only]\n"
"	--device-set <name> <Set options>\n"
"		[Set options] [--enable|--disable] [--connect|--disconnect]\n"
#ifdef _DEBUG
"	--device-set hddN --size <n> [--offline]\n"
#endif
"	--device-add <hdd | cdrom | net | fdd | serial | usb | pci>\n"
"		[Device options] [--enable|--disable] [--connect|--disconnect]\n"
"	--device-add hdd [--image <image>] [--recreate]\n"
"		[--size <n>] [--split]\n"
"		[--iface <ide|scsi|virtio>] [--position <n>]\n"
"		[--mnt <path>]\n"
"	--device-add hdd --device <real_name>\n"
"		[--iface <ide|scsi|virtio>] [--passthr] [--position <n>]\n"
"	--backup-add <ID> [--disk <disk_name>]\n"
"	--backup-del <ID|all>\n"
"	--device-add cdrom {--device <name> | --image <image>}\n"
"		[--iface <ide|scsi>] [--passthr] [--position <n>]\n"
"	--device-add net {--type routed | --network <network_id>}\n"
"       [--iface <name>]\n"
"		[--mac <addr|auto>] [--ipadd <addr[/mask]> | --ipdel <addr[/mask]> |\n"
"		--dhcp <yes|no> | --dhcp6 <yes|no>] [--gw <gw>] [--gw6 <gw>]\n"
"		[--nameserver <addr>] [--searchdomain <addr>] [--configure <yes|no>]\n"
"		[--apply-iponly <yes|no>] [--ipfilter <yes|no>] [--macfilter <yes|no>]\n"
"		[--preventpromisc <yes|no>]\n"
"		[--adapter-type <virtio|e1000|rtl>]\n"
#ifndef EXTERNALLY_AVAILABLE_BUILD
"		[--fw <on|off>] [--fw-policy <accept|deny>] [--fw-direction <in|out>]\n"
"		[--fw-rule <tcp|udp|* srcip|* port|* dstip|* port|*>]\n"
#endif
"	--device-add fdd [--device <real_name>]\n"
"	--device-add fdd --image <image> [--recreate]\n"
"	--device-add serial {--device <name> | --output <file>\n"
"		|--socket <name> [--socket-mode <server | client>]\n"
"		|--socket-tcp <ip:port> [--socket-mode <server | client>]\n"
"		|--socket-udp <ip:port>}\n"
"	--device-add pci --device <name>\n"
"Startup and shutdown options are:\n"
"    [--autostart <on|off|auto>] [--autostart-delay <n>]\n"
"    [--autostop <stop|suspend|shutdown>]\n"
"Optimization options are:\n"
"    [--faster-vm <on|off>] [--adaptive-hypervisor <on|off>]\n"
"    [--nested-virt <on|off>] [--pmu-virt <on|off>]\n"
, prl_basename(argv0));
}

static void usage_disp(const char * argv0)
{
	print_version(argv0);
	printf("\n");
	printf(
"Usage: %s ACTION [OPTIONS] [-l user[[:passwd]@server[:port]]\n"
"Supported actions are:\n"
"  info [-j, --json] [--license] [-f, --full]"
"\n"
"  install-license -k,--key <key> [-n,--name <name>] [-c,--company <name>]\n"
"  update-license\n"
"  set [--mem-limit <auto|size>] [-s,--min-security-level <low|normal|high>]\n"
"	[--mng-settings <allow|deny>] [{--device <device> --assignment <host|vm>}]\n"
"	[-c,--cep <on|off>] [--backup-path <path>] [--idle-connection-timeout <timeout>]\n"
"	[--backup-tmpdir <tmpdir>] [--backup-storage <user[[:passwd]@server[:port]]>]\n"
"	[--verbose-log <on|off>]\n"
"	[--cpu-features-mask <mask|off>]\n"
"   [--vm-cpulimit-type <full|guest>]\n"
"	[--allow-attach-screenshots <on|off>]\n"
"  shutdown [-f,--force] [--suspend-vm-to-pram]\n"
"  user list [-o,--output name[,name...]] [-j, --json]\n"
"  user set --def-vm-home <path>\n"
//"  statistics [-a, --all] [--loop] [--filter name]\n"
"  problem-report <-d,--dump|-s,--send [--proxy [user[:password]@proxyhost[:port]]] [--no-proxy]> "
	"[--stand-alone] [--name <your name>] [--email <your E-mail>] [--description <problem description>]\n"
"  net add <vnetwork_id> [-i,--ifname <if>] [-m,--mac <mac_address>]\n"
#ifdef _LIN_
"                   [-t,--type <bridged|host-only>]\n"
#else
"                   [-t,--type <bridged|host-only|shared>]\n"
#endif
"                   [-d,--description <description>]\n"
"                   [--ip <addr[/mask]>] [--dhcp-server <on|off>] [--dhcp-ip <ip>]\n"
"                   [--ip-scope-start <ip>] [--ip-scope-end <ip>}]\n"
"                   [--ip6 <addr[/mask]>] [--dhcp6-server <on|off>] [--dhcp-ip6 <ip>]\n"
"                   [--ip6-scope-start <ip>] [--ip6-scope-end <ip>}]\n"
#ifndef _LIN_
"                   [--nat-<tcp|udp>-add <rule_name,<redir_ip|redir_vm>,in_port,redir_port>]\n"
#endif
"  net set <vnetwork_id> [-i,--ifname <if>] [-m,--mac <mac_address>]\n"
#ifdef _LIN_
"                   [-t,--type <bridged|host-only>]\n"
#else
"                   [-t,--type <bridged|host-only|shared>]\n"
#endif
"                   [-d,--description <description>]\n"
"                   [--ip <addr[/mask]>] [--dhcp-server <on|off>] [--dhcp-ip <ip>]\n"
"                   [--ip-scope-start <ip>] [--ip-scope-end <ip>]\n"
"                   [--ip6 <addr[/mask]>] [--dhcp6-server <on|off>] [--dhcp-ip6 <ip>]\n"
"                   [--ip6-scope-start <ip>] [--ip6-scope-end <ip>}]\n"
#ifndef _LIN_
"                   [--nat-<tcp|udp>-add <rule_name,<redir_ip|redir_vm>,in_port,redir_port>]\n"
"                   [--nat-<tcp|udp>-del <rule_name>]\n"
#endif
"  net del <vnetwork_id>\n"
"  net info <vnetwork_id>\n"
"  net list [-j, --json]\n"
"  privnet add <private_network_id> [-a,--ipadd <addr[/mask]>] [--global <yes|no>]\n"
"  privnet set <private_network_id> [-a,--ipadd <addr[/mask]>] [-d,--ipdel <addr[/mask]>]\n"
"                                   [--global <yes|no>]\n"
"  privnet del <private_network_id>\n"
"  privnet list [-j, --json]\n"
"  usb list [-j, --json]\n"
"  usb del <usb-device-id>\n"
"  usb set <usb-device-id> <vm-uuid | vm-name>\n"
"  cttemplate list [-j, --json]\n"
"  cttemplate remove <name> [<os_template_name>]\n"
"  cttemplate copy <dst_node> <name> [<os_template_name>] [-f,--force]\n"
"  tc restart\n"
, prl_basename(argv0));
}

const char *capnames[NUMCAP] = {
	"CHOWN",
	"DAC_OVERRIDE",
	"DAC_READ_SEARCH",
	"FOWNER",
	"FSETID",
	"KILL",
	"SETGID",
	"SETUID",
	"SETPCAP",
	"LINUX_IMMUTABLE",
	"NET_BIND_SERVICE",
	"NET_BROADCAST",
	"NET_ADMIN",
	"NET_RAW",
	"IPC_LOCK",
	"IPC_OWNER",
	"SYS_MODULE",
	"SYS_RAWIO",
	"SYS_CHROOT",
	"SYS_PTRACE",
	"SYS_PACCT",
	"SYS_ADMIN",
	"SYS_BOOT",
	"SYS_NICE",
	"SYS_RESOURCE",
	"SYS_TIME",
	"SYS_TTY_CONFIG",
	"MKNOD",
	"LEASE",
	"AUDIT_WRITE",
	"VE_ADMIN",
	"SETFCAP",
	"FS_MASK"
};

int CmdParamData::set_dev_mode(const std::string &str)
{
	if (str == "expand")
		dev.mode = DEV_TYPE_HDD_EXPAND;
	else if (str == "host" || str == "host-only")
		dev.mode = DEV_TYPE_NET_HOST;
	else if (str == "bridged")
		dev.mode = DEV_TYPE_NET_BRIDGED;
	else if (str == "shared")
		dev.mode = DEV_TYPE_NET_SHARED;
	else if (str == "routed")
		dev.mode = DEV_TYPE_NET_ROUTED;
	else
		return 1;
	return 0;
}

int CmdParamData::set_cpu_hotplug(const std::string &str)
{
	if (str == "yes" || str == "on")
		cpu_hotplug = "on";
	else if (str == "no" || str == "off")
		cpu_hotplug = "off";
	else
		return 1;
	return 0;
}

int CmdParamData::set_autostart(const std::string &str)
{
	if (str == "yes" || str == "on" || str == "start-host")
		autostart = "on";
	else if (str == "no" || str == "off")
		autostart = "off";
	else if (str == "auto")
		autostart = str ;
	else if (str == "start-app")
		autostart = str;
	else if (str == "open-window")
		autostart = str;
	else
		return 1;
	return 0;
}

int CmdParamData::set_autostop(const std::string &str)
{
	if (str != "stop" &&
	    str != "suspend")
		return 1;
	autostop = str;

	return 0;
}

/* parse --features name:<on|off>[,name:<on|off>...] */
int CmdParamData::set_features(const std::string &str)
{
	int type, n, ret = 0;
	unsigned long id;
	std::string::size_type sp = 0, ep = 0, p;

	while (ep < str.length()) {
		p = str.find(",", ep);
		if (p == std::string::npos)
			p = str.find(" ", ep);
		if (p == std::string::npos)
			p = str.length();
		ep = p;
		std::string token = str.substr(sp, ep - sp);
		p = token.find(":");
		if (p == std::string::npos) {
			fprintf(stderr, "An incorrect value '%s' for"
					" --features is specified (mode is not specified)\n",
					token.c_str());
			return 1;
		}
		std::string feature = token.substr(0, p);
		std::string mode = token.substr(p + 1, token.length() - p - 1);

		if ((id = feature2id(feature, type)) == 0) {
			fprintf(stderr, "An incorrect feature '%s' for"
				" --features is specified\n",
				feature.c_str());
			return 1;
		}
		if (features.type != -1 && features.type != type) {
			fprintf(stderr, "An incorrect feature '%s', mixed features\n",
				feature.c_str());
			return 1;
		}
		features.type = type;

		features.known |= id;
		if (type == PVT_VM) {
			if (id == FT_TimeSyncInterval)
				ret = parse_ui(mode.c_str(), &features.time_sync_interval);
			else if (id == FT_SmartGuardInterval)
				ret = parse_ui(mode.c_str(), &features.smart_guard_interval);
			else if (id ==  FT_SmartGuardMaxSnapshots)
				ret = parse_ui(mode.c_str(), &features.smart_guard_max_snapshots);
			else {
				n = str2on_off(mode);
				if (n == -1)
					ret = 1;
				else if (n == 1)
					features.mask |= id;
			}
		} else {
			n = str2on_off(mode);
			if (n == -1)
				ret = 1;
			else if (n == 1)
				features.mask |= id;

		}
		if (ret) {
			fprintf(stderr, "An incorrect mode '%s' for"
				" features '%s' is specified\n",
				mode.c_str(), feature.c_str());
			return 1;
		}

		sp = ++ep;
	}

	return 0;
}

/* parse --capability name:<on|off>[,name:<on|off>...] */
int CmdParamData::set_cap(const std::string &str)
{
	str_list_t cap_list = split(str, ",");

	str_list_t::iterator it;
	for (it = cap_list.begin(); it != cap_list.end(); ++it)
	{

		std::string::size_type pos = it->find_first_of(":");
		if (pos == std::string::npos)
		{
			fprintf(stderr, "An invalid value ('%s') for --capability:"
					" the mode is not specified.\n",
					str.c_str());
			return 1;
		}

		std::string name = it->substr(0, pos);
		std::string mode = it->substr(++pos, std::string::npos);

		std::string searched_name(name);
		std::transform(searched_name.begin(), searched_name.end(),
				searched_name.begin(), ::toupper);

		for (pos = 0; pos < NUMCAP; ++pos)
		{
			if (searched_name == capnames[pos])
				break;
		}

		if (pos == NUMCAP)
		{
			fprintf(stderr, "An incorrect capability ('%s') is specified.\n",
				name.c_str());
			return 1;
		}

		if (mode == "on")
			cap.set_mask_on(pos);
		else if (mode == "off")
			cap.set_mask_off(pos);
		else
		{
			fprintf(stderr, "An incorrect mode ('%s')"
				" is specified for the capability '%s'.\n",
				mode.c_str(), name.c_str());
			return 1;
		}
	}

	return 0;
}

static int read_passwd(const char *fname, std::string &passwd)
{
	if (file2str(fname, passwd))
		return 1;
	passwd = passwd.substr(0, passwd.find_first_of("\n\r"));

	return 0;
}

static int parse_fw_rule(const char *str, struct fw_rule_s &rule)
{
	char proto[11];
	char srcip[32];
	char srcport[11];
	char dstip[32];
	char dstport[11];

	// proto srcip port dstip port"
	int res = sscanf(str, "%10s %31s %10s %31s %10s",
		proto, srcip, srcport, dstip, dstport);

	if (res != 5) {
		fprintf(stderr, "An incorrect firewall rule syntax: %s\n", str);
		return -1;
	}
	rule.proto = proto;
	if (rule.proto == "*")
		rule.proto = "";
	else if (rule.proto != "tcp" && rule.proto != "udp")
		return -1;
	if (strcmp(srcip, "*") != 0)
		rule.src_ip = srcip;
	if (strcmp(srcport, "*") != 0)
		rule.src_port = atoi(srcport);
	if (strcmp(dstip, "*") != 0)
		rule.dst_ip = dstip;
	if (strcmp(dstport, "*") != 0)
		rule.dst_port = atoi(dstport);
	if (rule.proto[0] == '\0' &&
		(rule.src_port != 0 || rule.dst_port != 0)) {
		fprintf(stderr, "Port number may only be specified for protocol 'tcp' or 'udp'\n");
		return -1;
	}

	return 0;
}

static int parse_offline_service(const char *str, OfflineSrvParam &param)
{
	char name[256];
	char tail[256];
	int res;
	int port;

	res = sscanf(str, "%255[^:]:%d%255s", name, &port, tail);
	if (res == 3 && !strcmp(tail, ":default"))
		param.used_by_default = true;
	else if (res != 2)
		return -1;
	param.name = name;
	param.port = port;

	return 0;
}

static int parse_network_shaping(const char *str, NetworkShapingParam &param)
{
	char dev[256];
	char tail[256];
	int res;

	res = sscanf(str, "%255[^:]:%u:%u:%u%255s",
			dev,
			&param.net_class,
			&param.totalrate,
			&param.rate,
			tail);
	if (res != 4)
		return -1;
	param.dev = dev;
	return 0;
}

static int parse_network_bandwidth(const char *str, NetworkBandwidthParam &param)
{
	char dev[256];
	char tail[256];
	int res;

	res = sscanf(str, "%255[^:]:%u%255s",
			dev,
			&param.bandwidth,
			tail);
	if (res != 2)
		return -1;
	param.dev = dev;
	return 0;
}

static int parse_rate(const char *str, struct rate_data &rate)
{
	char tail[256];
	int res;

	res = sscanf(str, "%u:%u%255s",
			&rate.classid,
			&rate.rate,
			tail);
	if (res != 2)
		return -1;
	return 0;
}

static int parse_network_class(const char *str, NetworkClassParam &param)
{
	char net[256];
	int res;

	res = sscanf(str, "%u:%255[^:]", &param.net_class, net);
	if (res != 2)
		return -1;
	param.net = net;
	return 0;
}

static int parse_mnt_opt(const std::string &val, int *flags)
{
	if (val == "ro")
		*flags = PMVD_READ_ONLY;
	else if (val == "rw")
		*flags = PMVD_READ_WRITE;
	else
		return -1;
	return 0;
}

#define CASE_PARSE_OPTION_GLOBAL(val, param)	\
		case CMD_LOGIN: \
			if (parse_auth(val, param.login)) { \
				fprintf(stderr, "An incorrect value is specified for --login: %s\n", \
					val.c_str()); \
				return invalid_action; \
			} \
			opt.hide_arg(); \
			break; \
		case CMD_PASSWD: \
			if (read_passwd(val.c_str(), param.login.get_passwd_buf())) \
				return invalid_action; \
			break; \
		case CMD_VERBOSE: \
			prl_set_log_verbose(atoi(val.c_str())); \
			break; \
		case CMD_VZCOMPAT: \
			g_vzcompat_mode = true; \
			break; \
		case CMD_TIMEOUT: \
			g_nJobTimeout = atoi(val.c_str()) * 1000; \
		break; \


CmdParamData cmdParam::get_xmlrpc_param(int argc, char **argv, Action action,
	const Option *options, int offset)
{
	std::string val;

	CmdParamData param;

	param.action = action;
	param.xmlrpc.action_provided = true;

	GetOptLong opt(argc, argv, options, offset);
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_XMLRPC_MNG_URL:
			param.xmlrpc.manager_url = val;
			break;
		case CMD_XMLRPC_USER_LOGIN:
			param.xmlrpc.user_login = val;
			break;
		case CMD_XMLRPC_PUD_DATAKEY:
			param.xmlrpc.data_key = val;
			break;
		case CMD_XMLRPC_PUD_DATAVERSION:
			if (parse_ui(val.c_str(), (unsigned int *)&param.xmlrpc.data_version )) {
				fprintf(stderr, "An incorrect value for"
					" --data-version is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_XMLRPC_PUD_USERDATA:
			param.xmlrpc.user_data = val;
			break;
		case CMD_XMLRPC_GUD_QUERY:
			param.xmlrpc.query = val;
			break;
		case CMD_XMLRPC_USE_JSON:
			param.xmlrpc.use_json = true;
			break;
		case CMD_XMLRPC_PI_ICONPATH:
			param.xmlrpc.icon_path = val;
			break;
		case CMD_XMLRPC_VIC_ICON_HASH:
			param.xmlrpc.hashes_list.insert(val);
			break;
		case GETOPTERROR:
		default:
			return invalid_action;
		}
	}

	return param;
}

CmdParamData cmdParam::get_disp_param(int argc, char **argv, Action action,
		const Option *options, int offset)
{
	std::string val;
	CmdParamData param;
	bool vnc_public_key = false;
	bool vnc_private_key = false;

	param.action = action;

	GetOptLong opt(argc, argv, options, offset);
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_MEMORY_LIMIT:
			if (val == "auto")
				param.disp.mem_limit = UINT_MAX;
			else if (parse_ui_x(val.c_str(), &param.disp.mem_limit)) {
				fprintf(stderr, "An incorrect value for"
					" --memory-limit is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_USER_MNG_SETTINGS:
			if (val == "allow")
				param.disp.allow_mng_settings = 1;
			else if (val == "deny")
				param.disp.allow_mng_settings = 0;
			else {
				fprintf(stderr, "An incorrect value for"
					" --mng-settings is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_CEP_MECH_SETTINGS:
			if ((param.disp.cep_mechanism = str2on_off(val)) == -1) {
				fprintf(stderr, "An invalid value was"
					" specified for --cep: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_VERBOSE_LOG_LEVEL:
			if ((param.disp.verbose_log_level = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --verbose-log is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_BACKUP_PATH:
			param.disp.backup_path = val;
			break;
		case CMD_BACKUP_TIMEOUT:
			if (parse_ui(val.c_str(), &param.disp.backup_timeout )) {
				fprintf(stderr, "An incorrect value for"
					" --idle-connection-timeout is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_BACKUP_TMPDIR:
			param.disp.backup_tmpdir = val;
			break;
		case CMD_VM_CPULIMIT_TYPE:
			if (val.compare("full") == 0) {
				param.disp.vm_cpulimit_type = PRL_VM_CPULIMIT_FULL;
			} else if (val.compare("guest") == 0) {
				param.disp.vm_cpulimit_type = PRL_VM_CPULIMIT_GUEST;
			} else {
				fprintf(stderr, "An incorrect value for"
						" --vm-cpulimit-type is specified: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		case CMD_BACKUP_STORAGE:
			if (parse_auth(val, param.disp.def_backup_storage)) {
				fprintf(stderr, "An incorrect value is"
						" specified for backup storage: %s\n",
						val.c_str());
				return invalid_action;
			}
			param.disp.change_backup_settings = true;
			opt.hide_arg();
			break;
		case CMD_LISTEN_INTERFACE:
			param.disp.listen_interface = val;
			break;
		case CMD_UPDATE_OFFLINE_SERVICE:
			if (parse_offline_service(val.c_str(), param.disp.offline_service)) {
				fprintf(stderr, "An incorrect value is"
						" specified for offline service: %s\n",
						val.c_str());

				return invalid_action;
			}
			break;
		case CMD_DEL_OFFLINE_SERVICE:
			param.disp.offline_service.name = val;
			param.disp.offline_service.del = true;
			break;
		case CMD_ADD_NETWORK_CLASS:
			if (parse_network_class(val.c_str(), param.disp.network_class)) {
				fprintf(stderr, "An incorrect value is"
						" specified for network shaping: %s\n",
						val.c_str());

				return invalid_action;
			}
			break;
		case CMD_DEL_NETWORK_CLASS:
			if (parse_network_class(val.c_str(), param.disp.network_class)) {
				fprintf(stderr, "An incorrect value is"
						" specified for network shaping: %s\n",
						val.c_str());

				return invalid_action;
			}
			param.disp.network_class.del = 1;
			break;
		case CMD_ADD_BANDWIDTH_ENTRY:
			if (parse_network_bandwidth(val.c_str(), param.disp.network_bandwidth)) {
				fprintf(stderr, "An incorrect value is"
						" specified for network bandwidth: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		case CMD_ADD_SHAPING_ENTRY:
			if (parse_network_shaping(val.c_str(), param.disp.network_shaping)) {
				fprintf(stderr, "An incorrect value is"
						" specified for network shaping: %s\n",
						val.c_str());

				return invalid_action;
			}
			break;
		case CMD_DEL_SHAPING_ENTRY:
			if (parse_network_shaping(val.c_str(), param.disp.network_shaping)) {
				fprintf(stderr, "An incorrect value is"
						" specified for network shaping: %s\n",
						val.c_str());

				return invalid_action;
			}
			param.disp.network_shaping.del = 1;
			break;
		case CMD_SHAPING_ENABLE:
			if ((param.disp.network_shaping.enable = str2on_off(val)) == -1) {
				fprintf(stderr, "An invalid value was"
					" specified for --shaping: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_MIN_SECURITY_LEVEL:
			param.disp.min_security_level = val;
			break;
		case CMD_KEY:
			param.key = val;
			break;
		case CMD_NAME:
			param.new_name = val;
			break;
		case CMD_COMPANY:
			param.company = val;
			break;
		case CMD_FORCE:
			param.disp.force = true;
			break;
		case CMD_SUSPEND_VM_TO_PRAM:
			param.disp.suspend_vm_to_pram = true;
			break;
		case CMD_DEVICE:
			param.disp.device = val;
			break;
		case CMD_ASSIGNMENT:
			if ((param.disp.assign_mode = str2dev_assign_mode(val)) == AM_NONE) {
				fprintf(stderr, "An incorrect value for"
					"--assignment  is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_INFO_LICENSE:
			param.disp.info_license = true;
			break;
		case GETOPTUNKNOWN:
			fprintf(stderr, "Unrecognized option: %s\n",
				opt.get_next());
			return invalid_action;
		case CMD_FILE:
			param.file = val;
			break;
		case CMD_BATCH:
			param.batch = true;
			break;
		case CMD_LOG_ROTATION:
			if ((param.disp.log_rotation = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --log-rotation is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_ALLOW_ATTACH_SCREENSHOTS:
			if ((param.disp.allow_attach_screenshots = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --allow-attach-screenshots is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_REQUIRE_PWD:
			{
				bool bOk = false;
				str_list_t opts = split(val, ":");
				if (opts.size() == 2)
				{
					std::string cmd = opts.front();
					int on_off = str2on_off(opts.back());
					if (   on_off != -1
						&& (   cmd == "create-vm"
							|| cmd == "add-vm"
							|| cmd == "remove-vm"
							|| cmd == "clone-vm"
							)
						)
					{
						param.disp.cmd_require_pwd_list.insert(
							std::make_pair( cmd, on_off ? true : false ));
						bOk = true;
					}
				}

				if ( ! bOk )
				{
					fprintf(stderr, "An incorrect value for"
						" --require-pwd is specified: %s\n",
						val.c_str());
					return invalid_action;
				}
			}
			break;
		case CMD_LOCK_EDIT_SETTINGS:
			if ((param.disp.lock_edit_settings = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --lock-edit-settings is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_HOST_ADMIN:
			param.disp.host_admin = val;
			opt.hide_arg();
			break;
		case CMD_ADVANCED_SECURITY_MODE:
			if (parse_adv_security_mode(val.c_str(), &param.disp.adv_security_mode))
			{
				fprintf(stderr, "An incorrect value for"
					" --advanced-security-mode is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_USE_JSON:
			param.use_json = true;
			break;
		case CMD_INFO_FULL:
			set_full_info_mode();
			break;
		case CMD_CPU_FEATURES_MASK:
			param.disp.cpu_features_mask_changes = val;
			break;
		case CMD_VNC_PUBLIC_KEY:
			if (!val.empty() && file2str(val.c_str(), param.disp.vnc_public_key))
				return invalid_action;
			vnc_public_key = true;
			param.disp.set_vnc_encryption = true;
			break;
		case CMD_VNC_PRIVATE_KEY:
			if (!val.empty() && file2str(val.c_str(), param.disp.vnc_private_key))
				return invalid_action;
			vnc_private_key = true;
			param.disp.set_vnc_encryption = true;
			break;
		case GETOPTERROR:
		default:
			return invalid_action;
		}
	}
	if (!param.disp.device.empty() && param.disp.assign_mode == AM_NONE) {
		fprintf(stderr, "The --assignment option have to be"
			" specified simultaneously with --device. <%s>\n", param.dev.device.c_str());
		return invalid_action;
	}
	if (param.disp.device.empty() && param.disp.assign_mode != AM_NONE) {
		fprintf(stderr, "The --device option have to be"
			" specified simultaneously with --assignment.\n");
		return invalid_action;
	}
	if (vnc_public_key != vnc_private_key) {
		fprintf(stderr, "The --vnc-public-key and --vnc-private-key option have to be"
			" specified simultaneously.\n");
		return invalid_action;
	}

	return param;
}

int CmdParamData::check_consistence(int id) const
{
#define CHECK_SYMUL(expr, opt1, opt2)					\
if (expr) {								\
	fprintf(stderr, "Unable to simultaneously use %s and %s\n",	\
		opt1, opt2);						\
	return 1;							\
}

	switch (id) {
	case CMD_DEVICE_ADD:
		if (dev.cmd != None) {
			fprintf(stderr, "The --device-add option is specified"
				"  several times.\n");
			return 1;
		}
		break;
	case CMD_DEVICE_SET:
		if (dev.cmd != None) {
			fprintf(stderr, "The --device-set option is specified"
				"  several times.\n");
			return 1;
		}
		break;
	case CMD_DEVICE:
		CHECK_SYMUL(!dev.image.empty(),
			"--device", "--image")
		break;
	case CMD_IMAGE:
		CHECK_SYMUL(!dev.device.empty(),
			"--device", "--image")
		break;
	case CMD_ENABLE:
		CHECK_SYMUL(dev.disable,
			"--enable", "--disable")
		break;
	case CMD_DISABLE:
		CHECK_SYMUL(dev.enable,
			"--enable", "--disable")
		break;
	case CMD_CONNECT:
		CHECK_SYMUL(dev.disconnect,
			"--connect", "--disconnect")
		break;
	case CMD_DISCONNECT:
		CHECK_SYMUL(dev.disconnect,
			"--disconnect", "--connect")
		break;
	case CMD_ATTACH_BACKUP_ID:
	case CMD_DETACH_BACKUP_ID:
		if (backup_cmd != None) {
			fprintf(stderr, "Multiple --backup-add or --backup-del options are specified.\n");
			return 1;
		}
	default:
		break;
	}
	return 0;
}

bool CmdParamData::get_realpath(std::string &path, bool check)
{
	if (path.empty())
		return true;
#ifdef _WIN_
	if (path[0] != '\\' && path[1] != ':')
#else
	if (path[0] != '/')
#endif
	{
		if (!login.server.empty()) {
			fprintf(stderr, "Relative path '%s' is not allowed.\n",
				path.c_str());
			return false;
		}
		char buf[4096];
#ifdef _WIN_
		if (_fullpath(buf, path.c_str(), sizeof(buf)) == NULL)
#else
		if (realpath(path.c_str(), buf) == NULL)
#endif
		{
			fprintf(stderr, "Unable to get full path for %s\n",
				path.c_str());
			return false;
		}
		path = buf;
	}
	if (check && login.server.empty()) {
		if (access(path.c_str(), 0)) {
			fprintf(stderr, "Unable to use '%s' because it does not"
				" exist.\n",
				path.c_str());
			return false;
		}
	}

	return true;
}

bool CmdParamData::is_valid()
{
	const char* error_descr = "Unable to simultaneously use";

	if (vmtype == PVTF_VM) {

		if (!config_sample.empty() && dist) {
			fprintf(stderr, "%s --config and --distribution"
				" or --ostype\n", error_descr);
			return false;
		}

		if (!config_sample.empty() && !ostemplate.empty()) {
			fprintf(stderr, "%s --config and --ostemplate\n", error_descr);
			return false;
		}

		if (!ostemplate.empty() && dist) {
			fprintf(stderr, "%s --ostemplate and --distribution"
				" or --ostype\n", error_descr);
			return false;
		}
	}

	if (vmtype == PVTF_CT && dist)
		fprintf(stderr, "The options --distribution and --ostype"
			" are ignored when creating Containers.\n");

	if (action == VmSetAction) {
		if (!is_dev_valid())
			return false;
	}
	if (action == VmRegisterAction) {
		if (preserve_uuid && !uuid.empty()) {
			fprintf(stderr, "%s --preserve-uuid and --uuid\n",
					error_descr);
			return false;
		}
	}
	if (!get_realpath(vm_location, false))
		return false;
	if (!get_realpath(dev.image, false))
		return false;
	if (dev.recreate && dev.type != DEV_FDD && dev.type != DEV_HDD) {
		fprintf(stderr, "The --recreate option have to be"
			" specified to setup fdd/hdd device.\n");
		return false;
	}
	if (dev.cmd == None && dev.net.is_updated()) {
		fprintf(stderr, "Unable to configure network parameters,"
			" the network interface is not specified.\n");
		return false;
	}
	if (dev.cmd == None && backup_cmd != Add && dev.is_updated()) {
		fprintf(stderr, "The device is not specified.\n");
		return false;
	}
	if (!dev.vnetwork.empty() && !dev.iface.empty()) {
		fprintf(stderr, "Assigning a virtual network adapter to both a"
			" Virtual Network and a NIC at the same time is not"
			" supported.\n");
		return false;
	}
	if (!dev.vnetwork.empty() && dev.mode != DEV_TYPE_NONE) {
		fprintf(stderr, "Assigning a virtual network adapter to a"
			" Virtual Network and specifying its type at the same"
			" time is not supported.\n");
		return false;
	}

	if (dev.net.fw_direction != -1 && dev.net.fw_policy == -1 &&
			dev.net.fw_rules.empty()) {
		fprintf(stderr, "The --fw-direction option must be used either"
				" with the --fw-policy or --fw-rule option.\n");
		return false;
	}
	if (!dev.net.fw_rules.empty() && dev.net.fw_direction == -1) {
		fprintf(stderr, "You must specify the --fw-direction option.\n");
		return false;
	}

	if (backup_cmd != None && dev.cmd != None) {
		fprintf(stderr, "The --backup-* options cannot be used with the "
			"--device-* options.\n");
		return false;
	}

	if (backup_cmd == None && !backup_disk.empty()) {
		fprintf(stderr, "You must specify the --backup-add option.\n");
		return false;
	}

	if (backup_cmd == Add && backup_disk.empty() && dev.is_updated()) {
		fprintf(stderr, "You must specify the --disk option.\n");
		return false;
	}

	if ((dev.cmd == Add || backup_cmd == Add) && dev.position != -1 && dev.iface.empty()) {
		fprintf(stderr, "The --position option must be used with the --iface option.\n");
		return false;
	}

	return true;
}

bool CmdParamData::is_dev_valid() const
{
	DevMode mode = dev.mode;
	DevType type = dev.type;

	if (mode != DEV_TYPE_NONE) {
		switch (type) {
		case DEV_HDD:
			if (mode != DEV_TYPE_HDD_EXPAND)
			{
				fprintf(stderr, "An invalid device type for %s is"
					" specified.\n", devtype2str(dev.type));
				return false;
			}
			break;
		case DEV_NET:
			if (!dev.vnetwork.empty())
			{
				fprintf(stderr, "The '--type' parameter cannot be used with"
					" the '--network' parameter.");
				return false;
			}
			if (mode != DEV_TYPE_NET_ROUTED)
			{
				fprintf(stderr, "An invalid device type is specified for %s",
						devtype2str(dev.type));
				return false;
			}
			break;
		default:
			fprintf(stderr, "The --type option is used incorrectly."
				" It can be used with 'hdd' or 'net' devices only.\n");
			return false;
		}
	}
	if (!dev.output.empty() &&
	    type != DEV_SERIAL && type != DEV_PARALLEL && type != DEV_SOUND)
	{
		fprintf(stderr, "The --output option is used incorrectly.\n");
		return false;
	}
	if (!dev.socket.empty() &&
	    type != DEV_SERIAL)
	{
		fprintf(stderr, "The --socket option is used incorrectly.\n");
		return false;
	}
	if (!dev.mixer.empty() &&
	    type != DEV_SOUND)
	{
		fprintf(stderr, "The --mixer option is used incorrectly.\n");
		return false;
	}


	return true;
}

bool CmdParamData::set_net_param(NetParam &param)
{
	if (dev.type != DEV_NET) {
		searchdomain = param.searchdomain;
		param.searchdomain.clear();
		nameserver = param.nameserver;
		param.nameserver.clear();
	}

	dev.net = param;

	/* set network params to the first network device in the list */
	if (dev.net.is_updated() && dev.type == DEV_NONE) {
		dev.type = DEV_NET;
		dev.cmd = Set;
	}

	return true;
}

#define DEVICE_DEL_MASK (PVCF_DESTROY_HDD_BUNDLE | PVCF_DESTROY_HDD_BUNDLE_FORCE | PVCF_DETACH_HDD_BUNDLE)

void CmdParamData::set_device_del_flags(unsigned int flags)
{
	commit_flags &= ~DEVICE_DEL_MASK;
	flags &= DEVICE_DEL_MASK;
	commit_flags |= flags;
}

CmdParamData cmdParam::get_param(int argc, char **argv, Action action,
		const Option *options, int offset)

{
	unsigned int ui;
	unsigned long barrier, limit;
	std::string val;
	std::bitset<8 * sizeof(unsigned int)> device_del_flags = 0;

	CmdParamData param;
	NetParam net;

	param.action = action;
	if (action != VmListAction)
		param.id = argv[offset];

	GetOptLong opt(argc, argv, options, offset + 1);
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		if (param.check_consistence(id))
			return invalid_action;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_FAST:
			param.fast = true;
			break;
		case CMD_USE_ACPI:
			param.use_acpi = true;
			break;
		case CMD_FORCE:
			param.force = true;
			break;
		case CMD_NOFORCE:
			param.noforce = true;
			break;
		case CMD_LIST_FIELD:
			param.list_field = val;
			break;
		case CMD_LIST_ALL:
			param.list_all = true;
			break;
		case CMD_LIST_NO_HDR:
			param.list_no_hdr = true;
			break;
		case CMD_LIST_SORT:
			param.list_sort = val;
			break;
		case CMD_LIST_STOPPED:
			param.list_stopped = true;
			break;
		case CMD_LIST_NAME:
			param.list_name = true;
			break;
		case CMD_LIST_ALL_FIELDS:
			param.list_all_fields = true;
			break;
		case CMD_INFO:
			param.info = true;
			break;
		case CMD_INFO_FULL:
			set_full_info_mode();
			break;
		case CMD_USE_JSON:
			param.use_json = true;
			break;
		case CMD_CONFIG:
			param.config_sample = val;
			break;
		case CMD_LOCATION:
			if (val[0] == '.') {
				fprintf(stderr, "An incorrect value for --dst is specified;"
					" relative paths are not allowed.\n");
				return invalid_action;
			}
			param.vm_location = val;
			break;
		case CMD_OSTYPE:
			if (!(param.dist = get_def_dist(val))) {
				if (val != "list")
					fprintf(stderr, "An incorrect value for --ostype"
						" is specified:%s\n", val.c_str());
				fprintf(stdout, "The following values are allowed: ");
				print_dist(true);
				return invalid_action;
			}
			break;
		case CMD_DIST:
			if (!(param.dist = get_dist(val))) {
				if (val != "list")
					fprintf(stderr, "An incorrect value for"
						" --distribution is specified: %s\n", val.c_str());
				fprintf(stdout, "The following values are allowed: ");
				print_dist(false);
				if (val == "list")
					exit(0);
				return invalid_action;
			}
			break;
		case CMD_OSTEMPLATE:
			param.ostemplate = val;
			break;
		case CMD_VMTYPE:
			if (val == "ct" || val == "c") {
				param.vmtype = PVTF_CT;
			} else if (val == "vm" || val == "v") {
				param.vmtype = PVTF_VM;
			} else if ((val == "all" || val == "a") &&
				(param.action == VmListAction || param.action == VmBackupListAction)) {
				param.vmtype = PVTF_VM | PVTF_CT;
			} else {
				 fprintf(stderr, "An incorrect value for"
					" --vmtype is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_NO_HDD:
			param.nohdd = true;
			break;
		case CMD_LION_RECOVERY:
			param.lion_recovery = true;
			break;
		case CMD_NAME:
			param.new_name = val;
			break;
		case CMD_TMPL:
			param.tmpl = true;
			param.clone_flags |= PCVF_CLONE_TO_TEMPLATE;
			break;
		case CMD_CHANGE_SID:
			param.clone_flags |= PCVF_CHANGE_SID;
			break;
		case CMD_LINKED_CLONE:
			param.clone_flags |= PCVF_LINKED_CLONE;
			break;
		case CMD_DETACH_EXTERNAL_HDD:{
			int detach = str2on_off(val);
			if (detach == -1) {
				fprintf(stderr, "An invalid value was"
					" specified for --detach-external-hdd: %s\n",
					val.c_str());
				return invalid_action;
			}
			if( detach )
				param.clone_flags |= PCVF_DETACH_EXTERNAL_VIRTUAL_HDD;
			else
				param.clone_flags ^= PCVF_DETACH_EXTERNAL_VIRTUAL_HDD;
			break;
		}
		case CMD_ONLINE_CLONE:
			/*
			 * Added this for compatibility with "vzmlocal --online".
			 * See https://jira.sw.ru/browse/PSBM-14283.
			 */
			break;
		case CMD_DEVICE_ADD:
			if ((param.dev.type = str2devtype(val)) == DEV_NONE) {
				fprintf(stderr, "An incorrect parameter for"
					" --device-add is specified: %s\n", val.c_str());
				return invalid_action;
			}
			param.dev.cmd = Add;
			break;
		case CMD_NETIF_ADD:
			param.dev.type = DEV_NET;
			net.ifname = val;
			param.dev.cmd = Add;
			break;
		case CMD_IFNAME:
			param.dev.cmd = Set;
			param.dev.type = DEV_NET;
			param.dev.name = val;
			net.ifname = val;
			break;
		case CMD_DEVICE_SET:
#ifdef _LIN_
			// FIXME: venet0 is net4294967295
			if (val == VENET0_STR)
				val = VENET0_ID;
#endif
			param.dev.type = str2devtype(val.substr(0,
					val.find_first_of("0123456789")));
			if (param.dev.type == DEV_NONE) {
				fprintf(stderr, "An incorrect parameter for"
					" %s is specified: %s\n",
					id == CMD_IFNAME ? "--ifname" : "--device-set",
					val.c_str());
				return invalid_action;
			}
			param.dev.name = val;
			param.dev.cmd = Set;
			break;
		case CMD_DEVICE_CONNECT:
			param.dev.name = val;
			param.dev.cmd = Connect;
			break;
		case CMD_DEVICE_DISCONNECT:
			param.dev.name = val;
			param.dev.cmd = Disconnect;
			break;
		case CMD_DEVICE_DEL:
#ifdef _LIN_
			// FIXME: venet0 is net4294967295
			if (val == VENET0_STR)
				val = VENET0_ID;
#endif
			param.dev.name = val;
			param.dev.cmd = Del;
			break;
		case CMD_NETIF_DEL:
			param.dev.type = DEV_NET;
			param.dev.name = val;
			param.dev.cmd = Del;
			break;
		case CMD_DEVICE:
			param.dev.device = val;
			break;
		case CMD_BOOTORDER:
			param.dev.name = val;
			param.dev.cmd = BootOrder;
			break;
		case CMD_IMAGE:
			if (val.empty())
				param.dev.empty_image = true;
			else
				param.dev.image = val;
			break;
		case CMD_MNT:
			if (val.empty())
				param.dev.empty_mnt = true;
			else
				param.dev.mnt = val;
			break;
		case CMD_RECREATE:
			param.dev.recreate = true;
			break;
		case CMD_SIZE:
			if (parse_ui_x(val.c_str(), &param.dev.size)) {
				fprintf(stderr, "An incorrect value for"
					" --size is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_OFFLINE:
			param.dev.offline = true;
			break;
		case CMD_NO_FS_RESIZE:
			param.dev.no_fs_resize = 1;
			break;
		case CMD_SPLIT:
			param.dev.split = true;
			break;
		case CMD_ENABLE:
			param.dev.enable = true;
			break;
		case CMD_DISABLE:
			param.dev.disable = true;
			break;
		case CMD_CONNECT:
			param.dev.connect = true;
			break;
		case CMD_DISCONNECT:
			param.dev.disconnect = true;
			break;
		case CMD_AUTOSTART:
			if (param.set_autostart(val)) {
				fprintf(stderr, "An incorrect value for"
					" --onboot is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_AUTOSTART_DELAY:
			if (parse_ui(val.c_str(), &param.autostart_delay)) {
				fprintf(stderr, "An incorrect value for"
					" --autostart-delay is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_AUTOSTOP:
			if (param.set_autostop(val)) {
				fprintf(stderr, "An incorrect value for"
					" --autostop is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_STARTUP_VIEW:
			param.startup_view = val;
			break;
		case CMD_ON_SHUTDOWN:
			param.on_shutdown = val;
			break;
		case CMD_ON_WINDOW_CLOSE:
			param.on_window_close = val;
			break;
		case CMD_IFACE:
			param.dev.iface = val;
			break;
		case CMD_SUBTYPE:
			param.dev.subtype = val;
			break;
		case CMD_VNETWORK:
			param.dev.vnetwork = val;
			break;
		case CMD_PASSTHR:
			param.dev.passthr = atoi(val.c_str());
			break;
		case CMD_DEV_TYPE:
			if (param.set_dev_mode(val)) {
				fprintf(stderr, "An incorrect value for --type is"
					" specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_ADAPTER_TYPE:
			if (val == "virtio")
				net.adapter_type = PNT_VIRTIO;
			else if (val == "e1000")
				net.adapter_type = PNT_E1000;
			else if (val == "rtl")
				net.adapter_type = PNT_RTL;
			else {
				fprintf(stderr, "An incorrect value for --adapter-type is"
					" specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_POSITION:
			param.dev.position = atoi(val.c_str());
			break;
		case CMD_OUTPUT:
			param.dev.output = val;
			break;
		case CMD_SOCKET:
			param.dev.socket = val;
			break;
		case CMD_SOCKET_TCP:
			if (!check_address(val))
			{
				fprintf(stderr, "An incorrect value for --socket-tcp is"
					" specified: %s.\n"
					"The value must be specified as host:port.\n",
					val.c_str());
				return invalid_action;
			}
			param.dev.socket_tcp = val;
			break;
		case CMD_SOCKET_UDP:
			param.dev.socket_udp = val;
			break;
		case CMD_SOCKET_MODE: {
			if (val == "server" )
				param.dev.socket_mode = PSP_SERIAL_SOCKET_SERVER;
			else if (val == "client")
				param.dev.socket_mode = PSP_SERIAL_SOCKET_CLIENT;
			else {
				fprintf(stderr, "An incorrect value for"
						" --socket-mode is specified: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		}
		case CMD_MIXER:
			param.dev.mixer = val;
			break;
		case CMD_CPUS:
			if (parse_ui(val.c_str(), &param.cpus))
			{
				fprintf(stderr, "An incorrect value for"
					" --cpus is specified: %s\n", val.c_str());
				return invalid_action;
			}
			param.cpus_present = true;
			break;
		case CMD_CPU_HOTPLUG:
			if (param.set_cpu_hotplug(val)) {
				fprintf(stderr, "An incorrect value for"
					" --cpu-hotplug is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_CPUUNITS:
			if (parse_ui(val.c_str(), &param.cpuunits) ||
			    param.cpuunits == 0)
			{
				fprintf(stderr, "An incorrect value for"
					" --cpuunits is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_CPUMASK:
			param.cpumask = val;
			break;
		case CMD_NODEMASK:
			param.nodemask = val;
			break;
		case CMD_CPULIMIT:
			if (parse_cpulimit(val.c_str(), &param.cpulimit)) {
				fprintf(stderr, "An incorrect value for"
					" --cpulimit is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_IOPRIO:
			if (parse_ui(val.c_str(), &param.ioprio) || param.ioprio > PRL_IOPRIO_MAX) {
				fprintf(stderr, "An incorrect value for"
					" --ioprio is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_IOLIMIT:
			if (parse_ui_x(val.c_str(), &param.iolimit, false)) {
				fprintf(stderr, "An incorrect value for"
					" --iolimit is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_IOPSLIMIT:
			if (parse_ui(val.c_str(), &param.iopslimit)) {
				fprintf(stderr, "An incorrect value for"
					" --iopslimit is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_MEMSIZE:
			if (parse_ui_x(val.c_str(), &param.memsize) ||
			    param.memsize == 0)
			{
				fprintf(stderr, "An incorrect value for"
					" --memsize is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_VIDEOSIZE:
			if (parse_ui_x(val.c_str(), &param.videosize) ||
					param.videosize == 0)
			{
				fprintf(stderr, "An incorrect value for"
					" --videosize is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_3D_ACCELERATE:
			if (val == "off") {
				param.v3d_accelerate = P3D_DISABLED;
			} else if (val == "highest") {
				param.v3d_accelerate = P3D_ENABLED_HIGHEST;
			} else if (val == "dx9") {
				param.v3d_accelerate = P3D_ENABLED_DX9;
			} else {
				fprintf(stderr, "An incorrect value for"
					" --3d-accelerate is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_VERTICAL_SYNC:
			if (val == "on") {
				param.vertical_sync = 1;
			} else if (val == "off") {
				param.vertical_sync = 0;
			} else {
				fprintf(stderr, "An incorrect value for"
					" --vertical-sync is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_HIGH_RESOLUTION:
			if ((param.high_resolution = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --high-resolution is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_MEMQUOTA:
			fprintf(stderr, "The --memquota is deprecated, "
					"please use --memguarantee instead\n");
			return invalid_action;
		case CMD_MEMGUARANTEE:
			if (parse_memguarantee(val.c_str(), param))
			{
				fprintf(stderr, "An incorrect value for"
						" --memguarantee is specified: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		case CMD_MEM_HOTPLUG:
			if ((param.mem_hotplug = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --mem-hotplug is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_DESC:
			param.desc = val;
			break;
		case CMD_VM_NAME:
			param.name = val;
			break;
		case CMD_TEMPLATE:
			if ((param.is_template = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --template is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_VM_RENAME_EXT_DISKS:
			param.commit_flags |= PVCF_RENAME_EXT_DISKS;
			break;
		case CMD_MAC:
			param.dev.mac = parse_mac(val.c_str());
			if (param.dev.mac.empty()) {
				fprintf(stderr, "An incorrect value for"
					" --mac is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_HELP:
			exit(0);
		case CMD_FILE:
			param.file = val;
			break;
		case CMD_USER_DEF_VM_HOME:
			param.user.def_vm_home = val;
			break;
		case CMD_FLAGS:
			param.system_flags = val;
			break;
		case CMD_SNAPSHOT_ID:
			normalize_uuid(val, param.snapshot.id);
			break;
		case CMD_SNAPSHOT_WAIT:
			param.snapshot.wait = true;
			break;
		case CMD_SNAPSHOT_SKIP_RESUME:
			param.snapshot.skip_resume = true;
			break;
		case CMD_SNAPSHOT_NAME:
			param.snapshot.name = val;
			break;
		case CMD_SNAPSHOT_DESC:
			param.snapshot.desc = val;
			break;
		case CMD_SNAPSHOT_LIST_TREE:
			param.snapshot.tree = true;
			break;
		case CMD_SNAPSHOT_CHILDREN:
			param.snapshot.del_with_children = true;
			break;
		case CMD_FASTER_VM:
			if ((param.faster_vm = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --faster-vm is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_ADAPTIVE_HYPERVISOR:
			if ((param.adaptive_hypervisor = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --adaptive-hypervisor is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_AUTO_COMPRESS:
			if ((param.auto_compress = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --auto-compress is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_NESTED_VIRT:
			if ((param.nested_virt = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --nested-virt is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_PMU_VIRT:
			if ((param.pmu_virt = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --pmu-virt is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_LONGER_BATTERY_LIFE:
			if ((param.longer_battery_life = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --longer-battery-life is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_BATTERY_STATUS:
			if ((param.battery_status = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --battery-status is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case CMD_WINSYSTRAY_IN_MACMENU:
			if ((param.winsystray_in_macmenu = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --winsystray-in-macmenu is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_AUTO_SWITCH_FULLSCREEN:
			if ((param.auto_switch_fullscreen = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --auto-switch-fullscreen is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_DISABLE_AERO:
			if ((param.disable_aero = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --disable-aero is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_HIDE_MIN_WINDOWS:
			if ((param.hide_min_windows = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --hide-min-windows is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_REQUIRE_PWD:
			{
				bool bOk = false;
				str_list_t opts = split(val, ":");
				if (opts.size() == 2)
				{
					std::string cmd = opts.front();
					int on_off = str2on_off(opts.back());
					if (   on_off != -1
						&& (   cmd == "exit-fullscreen"
							|| cmd == "change-vm-state"
							|| cmd == "manage-snapshots"
							|| cmd == "change-guest-pwd"
							)
						)
					{
						param.disp.cmd_require_pwd_list.insert(
							std::make_pair( cmd, on_off ? true : false ));
						bOk = true;
					}
				}

				if ( ! bOk )
				{
					fprintf(stderr, "An incorrect value for"
						" --require-pwd is specified: %s\n",
						val.c_str());
					return invalid_action;
				}
			}
			break;
		case CMD_EXPIRATION:
			{
				int on_off = str2on_off(val);
				if (on_off != -1)
				{
					param.expiration.enabled = on_off;
					break;
				}

				bool bOk = false;
				str_list_t opts = split(val, ":", true);
				if (opts.size() == 2)
				{
					bOk = true;
					std::string cmd = opts.front();
					std::string arg = opts.back();
					if (cmd == "date")
						param.expiration.date =
							arg.replace(strlen(XML_DATE_FORMAT), 1, 1, ' ');
					else if (cmd == "time-check")
						param.expiration.time_check = atoi(arg.c_str());
					else if (cmd == "offline-time")
						param.expiration.offline_time = atoi(arg.c_str());
					else if (cmd == "time-server")
						param.expiration.time_server = arg;
					else if (cmd == "note")
					{
						param.expiration.note = arg;
						param.expiration.note_edit = true;
					}
					else
						bOk = false;
				}

				if ( ! bOk )
				{
					fprintf(stderr, "An incorrect value for"
						" --expiration is specified: %s\n",
						val.c_str());
					return invalid_action;
				}
			}
			break;
		case CMD_LOCK_EDIT_SETTINGS:
			if ((param.disp.lock_edit_settings = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --lock-edit-settings is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_LOCK_ON_SUSPEND:
			if ((param.lock_on_suspend = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --lock-on-suspend is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_ISOLATE_VM:
			if ((param.isolate_vm = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --isolate-vm is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_SMART_GUARD:
			if ((param.smart_guard = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --smart-guard is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_SG_NOTIFY_BEFORE_CREATE:
			if ((param.sg_notify_before_create = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --sg-notify-before-create is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_SG_INTERVAL:
			if (parse_ui(val.c_str(), &param.sg_interval))
			{
				fprintf(stderr, "An incorrect value for"
					" --sg-interval is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_SG_MAX_SNAPSHOTS:
			if (parse_ui(val.c_str(), &param.sg_max_snapshots))
			{
				fprintf(stderr, "An incorrect value for"
					" --sg-max-snapshots is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_VNC_MODE:
			if (param.vnc.set_mode(val)) {
				fprintf(stderr, "An incorrect value for"
						" --vnc-mode is specified: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		case CMD_VNC_PORT:
			if (parse_ui(val.c_str(), &param.vnc.port)) {
				fprintf(stderr, "An incorrect value for"
						" --vnc-port is specified: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		case CMD_VNC_PASSWD:
			param.vnc.passwd = val;
			opt.hide_arg();
			break;
		case CMD_VNC_NOPASSWD:
			param.vnc.nopasswd = 1;
			break;
		case CMD_VNC_ADDRESS:
			param.vnc.address = val;
			break;
		case CMD_USERNAME:
			param.user_name = val;
			break;
		case CMD_USERPASSWORD:
			param.user_password = val;
			opt.hide_arg();
			break;
		case CMD_FEATURES:
			if (param.set_features(val))
				return invalid_action;
			break;
		case CMD_SEARCHDOMAIN:
			net.searchdomain = split(val, " ,");
			break;
		case CMD_HOSTNAME:
			param.hostname = val;
			break;
		case CMD_NAMESERVER:
			net.nameserver= split(val, " ,");
			break;
		case CMD_IP_SET:
			net.set_ip = true;
		case CMD_IP_ADD:
			normalize_ip(val);
			net.ip.add(val);
			break;
		case CMD_IP_DEL:
			if (val == "all")
				net.delall_ip = true;
			else {
				normalize_ip(val);
				net.ip_del.add(val);
			}
			break;
		case CMD_RATE: {
			struct rate_data rate;
			if (parse_rate(val.c_str(), rate)) {
				fprintf(stderr, "An incorrect value for"
					" --rate is specified: %s\n",
					val.c_str());
				return invalid_action;
			}

			param.rate.add(rate);
			break;
		}
		case CMD_RATEBOUND:
			if ((param.ratebound = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --ratebound is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_SMART_MOUSE_OPTIMIZE:
			if (val == "off") {
				param.smart_mouse_optimize = 0;
			} else if (val == "on") {
				param.smart_mouse_optimize = 1;
			} else if (val == "auto") {
				param.smart_mouse_optimize = 2;
			} else {
				fprintf(stderr, "An incorrect value for"
					" --smart-mouse-optimize is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_STICKY_MOUSE:
			if ((param.sticky_mouse = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --sticky-mouse is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_KEYBOARD_OPTIMIZE:
			if ((param.keyboard_optimize = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --keyboard-optimize is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_SYNC_HOST_PRINTERS:
			if (val == "on") {
				param.sync_host_printers = 1;
			} else if (val == "off") {
				param.sync_host_printers = 0;
			} else {
				fprintf(stderr, "An incorrect value for"
					" --sync-host-printers is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_SYNC_DEFAULT_PRINTER:
			if (val == "on") {
				param.sync_default_printer = 1;
			} else if (val == "off") {
				param.sync_default_printer = 0;
			} else {
				fprintf(stderr, "An incorrect value for"
					" --sync-default-printer is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_AUTO_SHARE_CAMERA:
			if ((param.auto_share_camera = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --auto-share-camera is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_AUTO_SHARE_BLUETOOTH:
			if ((param.auto_share_bluetooth = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --auto-share-bluetooth is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_SUPPORT_USB30:
			if ((param.support_usb30 = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --support-usb30 is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_EFI_BOOT:
			if ((param.efi_boot = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --efi-boot is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_SET_RESTRICT_EDITING:
			param.restrict_editing = true;
			break;
		case CMD_SELECT_BOOT_DEV:
			if ((param.select_boot_dev = str2on_off(val)) == -1)
			{
				fprintf(stderr, "An incorrect value for"
					" --select-boot-device is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_EXT_BOOT_DEV:
			param.ext_boot_dev = val;
			break;
		case CMD_GW:
			if (val.empty())
				net.gw = "x";
			else
				net.gw = val;
			break;
		case CMD_GW6:
			if (val.empty())
				net.gw6 = "x";
			else
				net.gw6 = val;
			break;
		case CMD_DHCP:
			if ((net.dhcp = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --dhcp is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_DHCP6:
			if ((net.dhcp6 = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --dhcp6 is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_FW:
			if ((net.fw_enable = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --fw is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_FW_POLICY:
			if (val == "accept")
				net.fw_policy = PFP_ACCEPT;
			else if (val == "deny")
				net.fw_policy = PFP_DENY;
			else {
				fprintf(stderr, "An incorrect value for"
					" --fw-policy is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_FW_DIRECTION:
			if (val == "in")
				net.fw_direction = PFD_INCOMING;
			else if (val == "out")
				net.fw_direction = PFD_OUTGOING;
			else {
				fprintf(stderr, "An incorrect value for"
					" --fw-direction is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_TEMPLATE_SIGN:
			if ((param.template_sign = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --template is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_FW_RULE: {
			struct fw_rule_s fw_rule;
			if (parse_fw_rule(val.c_str(), fw_rule))
				return invalid_action;
			net.fw_rules.add(fw_rule);
			break;
		}
		case CMD_CONFIGURE:
			if ((net.configure = str2on_off(val)) == -1) {
				fprintf(stderr, "An invalid value was"
						" specified for --configure: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		case CMD_APPLY_IPONLY:
			if ((param.apply_iponly = str2on_off(val)) == -1) {
				fprintf(stderr, "An invalid value was"
						" specified for --apply-iponly: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		case CMD_IP_FILTER:
			if ((net.ip_filter = str2on_off(val)) == -1) {
				fprintf(stderr, "An invalid value was"
					" specified for --ipfilter: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_MAC_FILTER:
			if ((net.mac_filter = str2on_off(val)) == -1) {
				fprintf(stderr, "An invalid value was"
					" specified for --macfilter: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_PREVENT_PROMISC:
			if ((net.prevent_promisc = str2on_off(val)) == -1) {
				fprintf(stderr, "An invalid value was"
					" specified for --preventpromisc: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_OFF_MAN:
			if ((param.off_man = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --offline_management is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_OFF_SRV:
			param.off_srv.push_back(val);
			break;
		case CMD_USERPASSWD:
			if (val.find_first_of(":") == std::string::npos) {
				fprintf(stderr, "An incorrect value for"
						" --userpasswd is specified: %s\n",
						val.c_str());
				return invalid_action;
			}
			param.userpasswd = val;
			opt.hide_arg();
			break;
		case CMD_HOST_ADMIN:
			param.disp.host_admin = val;
			opt.hide_arg();
			break;
		case CMD_CRYPTED:
			param.crypted = true;
			break;
		case CMD_USE_DEFAULT_ANSWERS:
			if ((param.use_default_answers = str2on_off(val)) == -1) {
				fprintf(stderr, "An invalid value was"
					" specified for --usedefanswers: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_TOOLS_AUTOUPDATE:
			if ((param.tools_autoupdate = str2on_off(val)) == -1) {
				fprintf(stderr, "An invalid value was"
					" specified for --tools-autoupdate: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_SMART_MOUNT:
			if ((param.smart_mount = str2on_off(val)) == -1) {
				fprintf(stderr, "An invalid value was"
					" specified for --smart-mount: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_PRESERVE_UUID:
			param.preserve_uuid = true;
			break;
		case CMD_REGENERATE_SRC_UUID:
			param.preserve_src_uuid = false;
			break;
		case GETOPTUNKNOWN:
			if (action == VmListAction) {
				param.id = opt.get_next();
			} else if (action == VmRegisterAction) {
				param.vm_location = opt.get_next();;
			} else if (action == VmExecAction || action == VmInternalCmd) {
				param.argv = opt.get_args();
				return param;
			} else {
				fprintf(stderr, "Unrecognized option: %s\n",
					opt.get_next());
				return invalid_action;
			}
			break;
		case GETOPTERROR:
			return invalid_action;
		case CMD_APPTEMPLATE:
			param.app_templates.push_back(val);
			break;
		case CMD_SWAPPAGES:
			if (parse_two_longs_N(val.c_str(), &barrier, &limit, 4096, 1)) {
				fprintf(stderr, "an invalid value was"
						" specified for --swappages: %s\n",
						val.c_str());
				return invalid_action;
			}
			param.ct_resource.add(PCR_SWAPPAGES, barrier, limit);
			break;
		case CMD_SWAP:
			if (parse_two_longs_N(val.c_str(), &barrier, &limit, 4096, 4096)) {
				fprintf(stderr, "an invalid value was"
						" specified for --swap: %s\n",
						val.c_str());
				return invalid_action;
			}
			param.ct_resource.add(PCR_SWAPPAGES, barrier, limit);
			break;
		case CMD_QUOTAUGIDLIMIT:
			if (parse_ui(val.c_str(), &ui)) {
				fprintf(stderr, "an invalid value was"
						" specified for --quotaugidlimit: %s\n",
						val.c_str());
				return invalid_action;
			}
			param.ct_resource.add(PCR_QUOTAUGIDLIMIT, ui);
			break;
		case CMD_SET_CAP:
			if (param.set_cap(val.c_str()))
				return invalid_action;
			break;
		case CMD_NETFILTER:
			param.netfilter = Netfilter::fromString(val);
			if (!param.netfilter.isValid()) {
				fprintf(stderr, "An incorrect netfilter mode ('%s') was specified.\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_MNT_OPT:
			if (parse_mnt_opt(val, &param.mnt_opts)) {
				fprintf(stderr, "Unknown mount option: %s\n",
					opt.get_next());
				return invalid_action;
			}
			break;
		case CMD_MNT_INFO:
			param.mnt_opts = PMVD_INFO;
			break;
		case CMD_HDD_BLOCK_SIZE:
			if (parse_ui(val.c_str(), &param.hdd_block_size)) {
				fprintf(stderr, "An invalid value was"
						" specified for --hdd-block-size: %s\n",
						val.c_str());
			}
			break;
		case CMD_UUID:
			if (normalize_uuid(val, param.uuid)) {
				fprintf(stderr, "An invalid value was"
							" specified for --uuid: %s\n",
							val.c_str());
				return invalid_action;
			}
			break;
		case CMD_IGNORE_HA_CLUSTER:
			param.ignore_ha_cluster = true;
			break;
		case CMD_WAIT:
			param.start_opts |= PNSF_VM_START_WAIT;
			break;
		case CMD_DESTROY_HDD:
			device_del_flags |= PVCF_DESTROY_HDD_BUNDLE;
			break;
		case CMD_DESTROY_HDD_FORCE:
			device_del_flags |= PVCF_DESTROY_HDD_BUNDLE_FORCE;
			break;
		case CMD_DETACH_HDD:
			device_del_flags |= PVCF_DETACH_HDD_BUNDLE;
			break;
		case CMD_AUTOCOMPACT:
			if ((param.autocompact = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
						" --autocompact is specified: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		case CMD_HA_ENABLE:
			if ((param.ha_enable = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value ('%s') is specified for"
					" --ha-enable\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_HA_PRIO:
			if (parse_ui(val.c_str(), &ui)) {
				fprintf(stderr, "An incorrect value ('%s') is specified for"
						" --ha-prio\n",
						val.c_str());
				return invalid_action;
			}
			param.ha_prio = (long long)ui;
			break;
		case CMD_ATTACH_BACKUP_ID:
			param.backup_cmd = Add;
			if (normalize_uuid(val, param.backup_id))
				param.backup_id = val;
			break;
		case CMD_DETACH_BACKUP_ID:
			param.backup_cmd = Del;
			if (normalize_uuid(val, param.backup_id))
				param.backup_id = val;
			break;
		case CMD_ATTACH_BACKUP_DISK:
			param.backup_disk = val;
			break;
		case CMD_EXEC_NO_SHELL:
			param.exec_in_shell = false;
			break;
		default:
			fprintf(stderr, "Unhandled option: %d\n", id);
			return invalid_action;
		}
	}

	if (device_del_flags.count() > 1) {
		fputs("Only one of options --detach-only, --destroy-image,"
				" --destroy-image-force can be specified.\n", stderr);
		return invalid_action;
	}

	if (device_del_flags.any())
		param.set_device_del_flags(device_del_flags.to_ulong());

	if (param.hdd_block_size != 0 && param.dev.type == DEV_HDD)
		param.dev.hdd_block_size = param.hdd_block_size;

	if (param.dev.type == DEV_HDD && param.autocompact != -1) {
		param.dev.autocompact = param.autocompact;
		param.autocompact = -1;
	}

	/* support of legacy 'set --diskspace XX' syntax */
	if (param.dev.size != 0 && param.dev.type == DEV_NONE) {
		param.dev.name = "hdd0";
		param.dev.type = DEV_HDD;
		param.dev.cmd = Set;
	}
	if (!param.set_net_param(net))
		 return invalid_action;
	if (!param.is_valid())
		return invalid_action;
	return param;
}

static int get_security_level(const std::string name, unsigned int *level)
{
	if (name == "low")
		*level = PSL_LOW_SECURITY;
	else if (name == "normal" )
		*level = PSL_NORMAL_SECURITY;
	else if (name == "high" )
		*level = PSL_HIGH_SECURITY;
	else
		return -1;
	return 0;
}

CmdParamData cmdParam::get_migrate_param(int argc, char **argv, Action action,
		const Option *options, int offset)
{
	std::string val;

	CmdParamData param;

	if (argc - offset < 2) {
		fprintf(stderr, "Incorrect migrate usage.\n");
		return invalid_action;
	}
	param.action = action;
	/* src */
	val = argv[offset];
	std::string::size_type pos = val.find_first_of("/");
	if (pos != std::string::npos) {
		param.id = val.substr(pos + 1, val.length() - pos);
		if (parse_auth(val.substr(0, pos), param.login, argv[offset])) {
			fprintf(stderr, "An incorrect value is"
				" specified for src: %s\n",
				val.c_str());
			return invalid_action;
		}
	} else {
		param.id = val;
	}
	/* dst */
	val = argv[++offset];
	pos = val.find_first_of("/");
	if (pos != std::string::npos) {
		param.migrate.dst_id = val.substr(pos + 1, val.length() - pos);
		val = val.substr(0, pos);
	}
	if (parse_auth(val, param.migrate.dst, argv[offset])) {
		fprintf(stderr, "An incorrect value is"
				" specified for dst: %s\n",
				val.c_str());
		return invalid_action;
	}

	GetOptLong opt(argc, argv, options, ++offset);
	// tunneled by default
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_FORCE:
			param.migrate.force = true;
			break;
		case CMD_SECURITY_LEVEL:
			if (get_security_level(val, &param.migrate.security_level)) {
				 fprintf(stderr, "An incorrect security level is"
					" specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_LOCATION:
			if (val[0] == '.') {
				fprintf(stderr, "An incorrect value for --dst is specified;"
					" relative paths are not allowed.\n");
				return invalid_action;
			}
			param.migrate.vm_location = val;
			break;
		case CMD_SESSIONID:
			param.migrate.sessionid = val;
			break;
		case CMD_CLONE_MODE:
			param.migrate.flags |= PVMT_CLONE_MODE;
			break;
		case CMD_REMOVE_BUNDLE:
			param.migrate.flags |= PVMT_REMOVE_SOURCE_BUNDLE;
			break;
		case CMD_SWITCH_TEMPLATE:
			param.migrate.flags |= PVMT_SWITCH_TEMPLATE;
			break;
		case CMD_CHANGE_SID:
			param.migrate.flags |= PVMT_CHANGE_SID;
			break;
		case CMD_IGNORE_EXISTING_BUNDLE:
			param.migrate.flags |= PVMT_IGNORE_EXISTING_BUNDLE;
			break;
		case CMD_SSH_OPTS:
			param.migrate.ssh_opts = split(val.c_str(), " ");
			break;
		case CMD_UNCOMPRESSED:
			param.migrate.flags |= PVMT_UNCOMPRESSED;
			break;
		case CMD_NO_TUNNEL:
			param.migrate.flags |= PVMT_DIRECT_DATA_CONNECTION;
			break;

		case GETOPTUNKNOWN:
			fprintf(stderr, "Unrecognized option: %s\n",
					opt.get_next());
			return invalid_action;
		case GETOPTERROR:
		default:
			return invalid_action;
		}
	}

	if ((param.migrate.flags & (PVMT_CHANGE_SID | PVMT_CLONE_MODE)) == PVMT_CHANGE_SID) {
		fprintf(stderr, "The --changesid option can be used with --clone only\n");
		return invalid_action;
	}

	if ((param.migrate.flags & PVMT_REMOVE_SOURCE_BUNDLE)
			&& (param.migrate.flags & PVMT_CLONE_MODE)) {
		fprintf(stderr, "The --remove-src option cannot be used with --clone\n");
		return invalid_action;
	}

	return param;
}

CmdParamData cmdParam::get_backup_param(int argc, char **argv, Action action,
		const Option *options, int offset)
{
	if (argc - offset < 1) {
		fprintf(stderr, "Incorrect backup usage.\n");
		return invalid_action;
	}

	CmdParamData param = parse_backup_args(argc, argv, action, options, offset + 1);
	if (param.action == InvalidAction)
		return param;

	/* backup node */
	std::string val = argv[offset];
	std::string::size_type pos = val.find_first_of("/");
	if (pos != std::string::npos) {
		param.id = val.substr(pos + 1, val.length() - pos);
		if (parse_auth(val.substr(0, pos), param.backup.login, argv[offset])) {
			fprintf(stderr, "An incorrect value is"
					" specified for node: %s\n",
					val.c_str());
			return invalid_action;
		}
	} else {
		param.id = val;
	}

	if ((param.backup.flags & (PBT_INCREMENTAL | PBT_FULL)) == 0 )
		param.backup.flags |= PBT_INCREMENTAL;

	return param;
}

CmdParamData cmdParam::get_restore_param(int argc, char **argv, Action action,
		const Option *options, int offset)
{
	std::string val;

	CmdParamData param;

	if (argc - offset < 1) {
		fprintf(stderr, "Incorrect backup usage.\n");
		return invalid_action;
	}

	param.action = action;

	GetOptLong opt(argc, argv, options, offset);
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_BACKUP_ID:
			if (normalize_uuid(val, param.backup.id))
				param.backup.id = val;
			break;
		case CMD_VM_ID:
			param.id = val;
			break;
		case CMD_NAME:
			param.backup.name = val;
			break;
		case CMD_LIST:
			param.action = VmBackupListAction;
			break;
		case CMD_BACKUP_LIST_FULL:
			param.backup.list_full = true;
			break;
		case CMD_BACKUP_STORAGE:
			if (parse_auth(val, param.backup.storage)) {
				fprintf(stderr, "An incorrect value is"
						" specified for backup storage: %s\n",
						val.c_str());
				return invalid_action;
			}
			opt.hide_arg();
			break;
		case CMD_SECURITY_LEVEL:
			if (get_security_level(val, &param.security_level)) {
				fprintf(stderr, "An incorrect security level is"
						" specified: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		case CMD_LOCATION:
			if (val[0] == '.') {
				fprintf(stderr, "An incorrect value for --dst is specified;"
					" relative paths are not allowed.\n");
				return invalid_action;
			}
			param.backup.vm_location = val;
			break;
		case GETOPTUNKNOWN:
		{
			const char *p = opt.get_next();
			if (*p == '-') {
				fprintf(stderr, "Unrecognized option: %s\n",
					opt.get_next());
				return invalid_action;
			}
			if (!param.id.empty()) {
				fprintf(stderr, "Incorrect usage.\n");
				return invalid_action;
			}
			param.id = p;
			break;
		}
		case GETOPTERROR:
		default:
			return invalid_action;
		}
	}

	return param;
}

CmdParamData cmdParam::get_backup_delete_param(int argc, char **argv, Action action,
		const Option *options, int offset)
{
	std::string val;

	CmdParamData param;

	if (argc - offset < 1) {
		fprintf(stderr, "Incorrect backup usage.\n");
		return invalid_action;
	}

	param.action = action;

	GetOptLong opt(argc, argv, options, offset);
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_BACKUP_ID:
			if (normalize_uuid(val, param.backup.id))
				param.backup.id = val;
			break;
		case CMD_BACKUP_STORAGE:
			if (parse_auth(val, param.backup.storage)) {
				fprintf(stderr, "An incorrect value is"
						" specified for backup storage: %s\n",
						val.c_str());
				return invalid_action;
			}
			opt.hide_arg();
			break;
		case CMD_SECURITY_LEVEL:
			if (get_security_level(val, &param.security_level)) {
				fprintf(stderr, "An incorrect security level is"
						" specified: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		case CMD_BACKUP_KEEP_CHAIN:
			param.backup.flags |= PBT_KEEP_CHAIN;
			break;
		case GETOPTUNKNOWN:
		{
			const char *p = opt.get_next();
			if (*p == '-') {
				fprintf(stderr, "Unrecognized option: %s\n",
					opt.get_next());
				return invalid_action;
			}
			if (!param.id.empty()) {
				fprintf(stderr, "Incorrect usage.\n");
				return invalid_action;
			}
			param.id = p;
			break;
		}
		case GETOPTERROR:
		default:
			return invalid_action;
		}
	}

	return param;
}

CmdParamData cmdParam::get_backup_list_param(int argc, char **argv, Action action,
		const Option *options, int offset)
{
	std::string val;

	CmdParamData param;
	param.vmtype = PVTF_VM | PVTF_CT;

	param.action = action;
	GetOptLong opt(argc, argv, options, offset + 1);
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_BACKUP_LIST_FULL:
			param.backup.list_full = true;
			break;
		case CMD_LIST_NO_HDR:
			param.list_no_hdr = true;
			break;
		case CMD_BACKUP_ID:
			if (normalize_uuid(val, param.backup.id))
				param.backup.id = val;
			break;
		case CMD_LIST_LOCAL_VM:
			param.backup.list_local_vm = true;
			break;
		case CMD_BACKUP_STORAGE:
			if (parse_auth(val, param.backup.storage)) {
				fprintf(stderr, "An incorrect value is"
						" specified for backup storage: %s\n",
						val.c_str());
				return invalid_action;
			}
			opt.hide_arg();
			break;
		case CMD_SECURITY_LEVEL:
			if (get_security_level(val, &param.security_level)) {
				fprintf(stderr, "An incorrect security level is"
						" specified: %s\n",
						val.c_str());
				return invalid_action;
			}
			break;
		case CMD_VMTYPE:
			if (val == "ct" || val == "c") {
				param.vmtype = PVTF_CT;
			} else if (val == "vm" || val == "v") {
				param.vmtype = PVTF_VM;
			} else if (val == "all" || val == "a") {
				param.vmtype = PVTF_VM | PVTF_CT;
			} else {
				 fprintf(stderr, "An incorrect value for"
					" --vmtype is specified: %s\n", val.c_str());
				return invalid_action;
			}
			break;
		case GETOPTUNKNOWN:
			param.id = opt.get_next();
			break;
		case GETOPTERROR:
		default:
			return invalid_action;
		}
	}

	return param;
}

CmdParamData cmdParam::get_statistics_param(int argc, char **argv, Action action,
		const Option *options, int offset)
{
	std::string val;

	CmdParamData param;
	param.action = action;

	GetOptLong opt(argc, argv, options, offset);
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_LOOP:
			param.statistics.loop = true;
			break;
		case CMD_PERF_FILTER:
			param.statistics.filter = val;
			break;
		case CMD_LIST_ALL:
			param.list_all = true;
			break;
		case GETOPTUNKNOWN:
			param.id = opt.get_next();
			break;
		case GETOPTERROR:
		default:
			return invalid_action;
		}
	}
	return param;
}

CmdParamData cmdParam::get_problem_report_param(int argc, char **argv, Action action,
		const Option *options, int offset)
{
	std::string val;
	bool action_specified = false;

	CmdParamData param;

	g_problem_report_cmd = true;

	param.action = action;
	if (action == VmProblemReportAction)
		param.id = argv[offset++];

	GetOptLong opt(argc, argv, options, offset);
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_SEND_PROBLEM_REPORT:
			param.problem_report.send = true;
			action_specified = true;
			break ;
		case CMD_DUMP_PROBLEM_REPORT:
			param.problem_report.send = false;
			action_specified = true;
			break ;
		case CMD_CREATE_PROBLEM_REPORT_WITHOUT_SERVER:
			param.problem_report.stand_alone = true;
			break ;
		case CMD_PROBLEM_REPORT_USER_NAME:
			param.problem_report.user_name = val;
			break ;
		case CMD_PROBLEM_REPORT_USER_EMAIL:
			param.problem_report.user_email = val;
			break ;
		case CMD_PROBLEM_REPORT_DESCRIPTION:
			param.problem_report.description = val;
			break ;
		case CMD_USE_PROXY:
			param.problem_report.proxy_settings = val;
			opt.hide_arg();
			break;
		case CMD_DONT_USE_PROXY:
			param.problem_report.dont_use_proxy = true;
			break;
		case GETOPTUNKNOWN:
			fprintf(stderr, "Unrecognized option: %s\n",
					opt.get_next());
			return invalid_action;

		case GETOPTERROR:
		default:
			return invalid_action;
		}
	}

	if (!action_specified) {
		fprintf(stderr, "Please, choose an action: send the problem report "
			"to Parallels (-s, --send) or dump it to stdout (-d, --dump)\n");
		return invalid_action;
	}
	return param;
}

CmdParamData cmdParam::get_vm(int argc, char **argv)
{
	if (argc < 2) {
		usage_vm(argv[0]);
		return invalid_action;
	}

	if (!strcmp(argv[1], "--version")) {
		print_version(argv[0]);
		exit(0);
	}

	for (int i = 0; i < argc; i++) { 
		if (!strcmp(argv[i], "--help")) {
			usage_vm(argv[0]);
			exit(0);
		}
	}

	if (!strcmp(argv[1], "list"))
		 return get_param(argc, argv, VmListAction, list_options, 1);
	else if (!strcmp(argv[1], "backup-list"))
		return get_backup_list_param(argc, argv, VmBackupListAction, backup_list_options, 1);

	if (argc < 3) {
		fprintf(stderr, "Invalid usage\n");
		usage_vm(argv[0]);
		return invalid_action;
	}

	int i = 1;

	if (!strcmp(argv[1], "start")) {
		return get_param(argc, argv, VmStartAction, start_options, 2);
	} else if (!strcmp(argv[1], "change-sid")) {
		return get_param(argc, argv, VmChangeSidAction, no_options, 2);
	} else if (!strcmp(argv[1], "internal")) {
		return get_param(argc, argv, VmInternalCmd, no_options, 2);
	} else if (!strcmp(argv[1], "reset-uptime")) {
		return get_param(argc, argv, VmResetUptimeAction, no_options, 2);
	} else if (!strcmp(argv[1], "stop")) {
		return get_param(argc, argv, VmStopAction, stop_options, 2);
	} else if (!strcmp(argv[1], "mount")) {
		return get_param(argc, argv, VmMountAction, mount_options, 2);
	} else if (!strcmp(argv[1], "umount")) {
		return get_param(argc, argv, VmUmountAction, stop_options, 2);
	} else if (!strcmp(argv[1], "set")) {
		return get_param(argc, argv, VmSetAction, set_options, 2);
	} else if (!strcmp(argv[1], "suspend")) {
		return get_param(argc, argv, VmSuspendAction, no_options, 2);
	} else if (!strcmp(argv[1], "resume")) {
		return get_param(argc, argv, VmResumeAction, no_options, 2);
	} else if (!strcmp(argv[1], "pause")) {
		return get_param(argc, argv, VmPauseAction, no_options, 2);
	} else if (!strcmp(argv[1], "create")) {
		return get_param(argc, argv, VmCreateAction, create_options, 2);
	} else if (!strcmp(argv[1], "convert")) {
		return get_param(argc, argv, VmConvertAction, convert_options, 2);
	} else if (!strcmp(argv[1], "delete") || !strcmp(argv[1], "destroy")) {
		return get_param(argc, argv, VmDestroyAction, destroy_options, 2);
	} else if (!strcmp(argv[1], "register")) {
		return get_param(argc, argv, VmRegisterAction, register_options, 1);
	} else if (!strcmp(argv[1], "unregister")) {
		return get_param(argc, argv, VmUnregisterAction, no_options, 2);
	} else if (!strcmp(argv[1], "clone")) {
		return get_param(argc, argv, VmCloneAction, clone_options, 2);
	} else if (!strcmp(argv[1], "reset")) {
		return get_param(argc, argv, VmResetAction, no_options, 2);
	} else if (!strcmp(argv[1], "restart")) {
		return get_param(argc, argv, VmRestartAction, no_options, 2);
	} else if (!strcmp(argv[1], "installtools")) {
		return get_param(argc, argv, VmInstallToolsAction, no_options, 2);
	} else if (!strcmp(argv[1], "capture")) {
		return get_param(argc, argv, VmCaptureAction, capture_options, 2);
	} else if (!strcmp(argv[1], "snapshot")) {
		return get_param(argc, argv, VmSnapshot_Create,
						snapshot_options, 2);
	} else if (!strcmp(argv[1], "snapshot-switch")) {
		return get_param(argc, argv, VmSnapshot_SwitchTo,
						snapshot_switch_options, 2);
	} else if (!strcmp(argv[1], "snapshot-delete")) {
		return get_param(argc, argv, VmSnapshot_Delete,
						snapshot_delete_options, 2);
	} else if (!strcmp(argv[1], "snapshot-list")) {
		return get_param(argc, argv, VmSnapshot_List,
						snapshot_list_options, 2);
	} else if (!strcmp(argv[1], "migrate")) {
		return get_migrate_param(argc, argv, VmMigrateAction,
			migrate_options, 2);
	} else if (!strcmp(argv[1], "move")) {
		return get_param(argc, argv, VmMoveAction, move_options, 2);
	} else if (!strcmp(argv[1], "statistics")) {
		return get_statistics_param(argc, argv, VmPerfStatsAction,
                                    statistics_options, 2);
	} else if (!strcmp(argv[1], "problem-report")) {
		return get_problem_report_param(argc, argv, VmProblemReportAction,
                                    problem_report_options, 2);
	} else if (!strcmp(argv[1], "enter")) {
		return get_param(argc, argv, VmEnterAction,
                                    no_options, 2);
	} else if (!strcmp(argv[1], "console")) {
		return get_param(argc, argv, VmConsoleAction,
                                    no_options, 2);
	} else if (!strcmp(argv[1], "exec")) {
		return get_param(argc, argv, VmExecAction,
                                    exec_options, 2);
	} else if (!strcmp(argv[1], "backup")) {
		return get_backup_param(argc, argv, VmBackupAction, backup_options, 2);
	} else if (!strcmp(argv[1], "restore")) {
		return get_restore_param(argc, argv, VmRestoreAction, restore_options, 2);
	} else if (!strcmp(argv[1], "backup-delete")) {
		return get_backup_delete_param(argc, argv, VmBackupDeleteAction, backup_delete_options, 2);
	} else if (!strcmp(argv[1], "auth")) {
		return get_param(argc, argv, VmAuthAction, auth_options, 2);
	} else if (!strcmp(argv[1], "status")) {
		return get_param(argc, argv, VmStatusAction, no_options, 2);
	} else if (!strcmp(argv[i], "server")) {
		// sergeyt@:  very strange code
		++i;
		if (!strcmp(argv[i], "shutdown"))
			return get_disp_param(argc, argv, SrvShutdownAction,
					disp_shutdown_options, ++i);
		else if (!strcmp(argv[i], "info"))
			return get_disp_param(argc, argv, SrvHwInfoAction,
					disp_info_options, ++i);
		else if (!strcmp(argv[i], "install-license"))
			return get_disp_param(argc, argv, SrvInstallLicenseAction,
				lic_options, ++i);
		else if (!strcmp(argv[i], "problem-report"))
			return get_problem_report_param(argc, argv, SrvProblemReportAction,
				problem_report_options, ++i);
	}

	fprintf(stderr, "Unknown action: %s\n", argv[i]);
	usage_vm(argv[0]);
	return invalid_action;
}

CmdParamData cmdParam::get_vnet_param(int argc, char **argv,
		unsigned cmd, int offset)
{
	std::string val;
	CmdParamData param;

	param.action = SrvVNetAction;
	param.vnet.cmd = cmd;

	GetOptLong opt(argc, argv, disp_vnet_options, offset);
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_VNET_NEW_NAME:
			fprintf(stderr, "Virtual network's renaming is now deprecated."
					" You may create new virtual network with specified name"
					" and reattach all virtual machines to it.\n");
			return invalid_action;
		case CMD_VNET_DESCRIPTION:
			param.vnet.description = val;
			break;
		case CMD_VNET_IFACE:
			param.vnet.ifname = val;
			break;
		case CMD_VNET_IP_SCOPE_START:
			param.vnet.ip_scope_start = val;
			break;
		case CMD_VNET_IP_SCOPE_END:
			param.vnet.ip_scope_end = val;
			break;
		case CMD_VNET_IP6_SCOPE_START:
			param.vnet.ip6_scope_start = val;
			break;
		case CMD_VNET_IP6_SCOPE_END:
			param.vnet.ip6_scope_end = val;
			break;
		case CMD_VNET_NAT_TCP_ADD:
		case CMD_VNET_NAT_UDP_ADD:
			{
				str_list_t raw_rule = split(val, ",");
				if (raw_rule.size() != 4)
				{
					fprintf(stderr, "An incorrect value for"
						" --nat-<tcp|udp>-add is specified: %s\n",
							val.c_str());
					return invalid_action;
				}
				str_list_t::const_iterator it = raw_rule.begin();

				nat_rule_s nat_rule;
				nat_rule.name = *it; ++it;
				nat_rule.redir_entry = *it; ++it;
				nat_rule.in_port = atoi((*it).c_str()); ++it;
				nat_rule.redir_port = atoi((*it).c_str());

				nat_rule_list_t& nat_add_rules
					= (id == CMD_VNET_NAT_TCP_ADD
						? param.vnet.nat_tcp_add_rules : param.vnet.nat_udp_add_rules);
				nat_add_rules.add(nat_rule);
			}
			break;
		case CMD_VNET_NAT_TCP_DEL:
		case CMD_VNET_NAT_UDP_DEL:
			if (val.empty())
			{
				fprintf(stderr, "An incorrect value for"
					" --nat-<tcp|udp>-del is specified: rule name is empty\n");
				return invalid_action;
			}
			else
			{
				str_list_t& nat_del_rules
					= (id == CMD_VNET_NAT_TCP_DEL
						? param.vnet.nat_tcp_del_rules : param.vnet.nat_udp_del_rules);
				nat_del_rules.push_back(val);
			}
			break;
		case CMD_VNET_HOST_IP:
			normalize_ip(val);
			param.vnet.host_ip = ip_addr(val);
			break;
		case CMD_VNET_HOST_IP6:
			normalize_ip(val);
			param.vnet.host_ip6 = ip_addr(val);
			break;
		case CMD_VNET_DHCP_ENABLED:
			if ((param.vnet.dhcp_enabled = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --dhcp-server is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_VNET_DHCP6_ENABLED:
			if ((param.vnet.dhcp6_enabled = str2on_off(val)) == -1) {
				fprintf(stderr, "An incorrect value for"
					" --dhcp6-server is specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_VNET_DHCP_IP:
			param.vnet.dhcp_ip = val;
			break;
		case CMD_VNET_DHCP_IP6:
			param.vnet.dhcp_ip6 = val;
			break;
		case CMD_VNET_TYPE:
			if (!val.compare("bridged"))
				param.vnet.type = PVN_BRIDGED_ETHERNET;
			else if (!val.compare("host-only") ||
				!val.compare("shared"))
				param.vnet.type = PVN_HOST_ONLY;
			else {
				fprintf(stderr, "An unknown Virtual Network"
					" type: %s\n", val.c_str());
				return invalid_action;
			}
			if (!val.compare("shared"))
				param.vnet.is_shared = true;
			break;
		case CMD_VNET_MAC:
			param.vnet.mac = val;
			break;
		case CMD_USE_JSON:
			param.use_json = true;
			break;
		case GETOPTUNKNOWN:
			if (param.vnet.vnet.empty())
				param.vnet.vnet = opt.get_next();
			else {
				fprintf(stderr, "An unknown option: %s\n",
					opt.get_next());
				return invalid_action;
			}
			break;
		case GETOPTERROR:
		default:
			return invalid_action;
		}
	}

	if (param.vnet.vnet.empty() && cmd != VNetParam::List) {
		fprintf(stderr, "The Virtual Network name is not specified.\n");
		return invalid_action;
	}
	if (!param.vnet.ifname.empty() && !param.vnet.mac.empty()) {
		fprintf(stderr, "Specify either the --ifname or --mac option,"
			" but not both options.\n");
		return invalid_action;
	}
	if (param.vnet.type == PVN_BRIDGED_ETHERNET &&
		param.vnet.ifname.empty()) {
		fprintf(stderr, "For a bridged Virtual Network, you need to"
			" specify either the --ifname or --mac option.\n");
		return invalid_action;
	}
	if (!param.vnet.ifname.empty() && param.vnet.type == PVN_HOST_ONLY) {
		fprintf(stderr, "The --ifname option should be"
                                " specified for bridged virtual networks only\n");
		return invalid_action;
	}

	return param;
}

CmdParamData cmdParam::get_privnet_param(int argc, char **argv,
		unsigned cmd, int offset)
{
	std::string val;
	CmdParamData param;

	param.action = SrvPrivNetAction;
	param.privnet.cmd = cmd;

	GetOptLong opt(argc, argv, disp_privnet_options, offset);
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_PRIVNET_IPADD:
			if (val != "*")
				normalize_ip(val);
			param.privnet.ip.add(val);
			break;
		case CMD_PRIVNET_IPDEL:
			if (val != "*")
				normalize_ip(val);
			param.privnet.ip_del.add(val);
			break;
		case CMD_PRIVNET_GLOBAL:
			if ((param.privnet.is_global = str2on_off(val)) == -1) {
				fprintf(stderr, "An invalid value was"
					" specified for --global: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case GETOPTUNKNOWN:
			if (param.privnet.name.empty())
				param.privnet.name = opt.get_next();
			else {
				fprintf(stderr, "An unknown option: %s\n",
					opt.get_next());
				return invalid_action;
			}
			break;
		case CMD_USE_JSON:
			param.use_json = true;
			break;
		case GETOPTERROR:
		default:
			return invalid_action;
		}
	}

	if (param.privnet.name.empty() && cmd != PrivNetParam::List) {
		fprintf(stderr, "The IP private network name is not specified.\n");
		return invalid_action;
	}
	if (cmd == PrivNetParam::Set && param.privnet.ip.empty() &&
			param.privnet.ip_del.empty() &&
			param.privnet.is_global == -1) {
		fprintf(stderr, "Specify either the --ipadd or --ipdel option.\n");
		return invalid_action;
	}

	return param;
}

CmdParamData cmdParam::parse_usb_args(int argc, char **argv, int i)
{
	std::string val;
	CmdParamData param;
	param.action = DispUsbAction;

	if (argc - 1 <= i)
		return invalid_action;

	i++;
	if (!strcmp(argv[i], "list"))
	{
		param.usb.cmd = UsbParam::List;
	}
	else if (!strcmp(argv[i], "del"))
	{
		param.usb.cmd = UsbParam::Delete;
		if (argc - i < 2)
			return invalid_action;
		param.usb.name = argv[++i];
	}
	else if (!strcmp(argv[i],"set"))
	{
		param.usb.cmd = UsbParam::Set;
		if( argc -i < 3 )
			return invalid_action;
		param.usb.name = argv[++i];
		normalize_uuid(argv[++i], param.usb.id);
	} else
		return invalid_action;

	GetOptLong opt(argc, argv, disp_usb_options, ++i );
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_USE_JSON:
			param.use_json = true;
			break;
		default:
			return invalid_action;
		}
	}

	return param;
}

CmdParamData cmdParam::parse_vnet_args(int argc, char **argv, int i)
{
	unsigned cmd;
	if (argc - 1 <= i)
		return invalid_action;

	i++;
	if (!strcmp(argv[i], "list"))
		cmd = VNetParam::List;
	else if (!strcmp(argv[i], "set"))
		cmd = VNetParam::Set;
	else if ((!strcmp(argv[i], "add")) ||
		(!strcmp(argv[i], "new")))
		cmd = VNetParam::Add;
	else if (!strcmp(argv[i], "del"))
		cmd = VNetParam::Del;
	else if (!strcmp(argv[i], "info"))
		cmd = VNetParam::Info;
	/* compat options for compatibility with PVC vznetcfg interface */
	else if (!strcmp(argv[i], "addif")) {
		if (argc - i < 3)
			return invalid_action;
		CmdParamData param;
		param.action = SrvVNetAction;
		param.vnet.cmd = VNetParam::Set;
		param.vnet.vnet = argv[++i];
		param.vnet.ifname = argv[++i];
		param.vnet.type = PVN_BRIDGED_ETHERNET;
		return param;
	} else if (!strcmp(argv[i], "delif")) {
		if (argc - i < 2)
			return invalid_action;
		CmdParamData param;
		param.action = SrvVNetAction;
		param.vnet.cmd = VNetParam::Set;
		param.vnet.ifname = argv[++i];
		param.vnet.type = PVN_HOST_ONLY;
		return param;
	} else if (!strcmp(argv[i], "change")) {
		if (argc - i < 3)
			return invalid_action;
		CmdParamData param;
		param.action = SrvVNetAction;
		param.vnet.cmd = VNetParam::Set;
		param.vnet.vnet = argv[++i];
		return param;
	} else
		return invalid_action;

	return get_vnet_param(argc, argv, cmd, ++i);
}

CmdParamData cmdParam::parse_tc_args(int argc, char **argv, int i)
{
	if (argc - 1 <= i)
		return invalid_action;

	i++;
	if (!strcmp(argv[i], "restart"))
		return get_disp_param(argc, argv, SrvShapingRestartAction,
				no_options, ++i);

	return invalid_action;
}

CmdParamData cmdParam::parse_privnet_args(int argc, char **argv, int i)
{
	unsigned cmd;
	if (argc - 1 <= i)
		return invalid_action;
	i++;
	if (!strcmp(argv[i], "list"))
		cmd = PrivNetParam::List;
	else if (!strcmp(argv[i], "set"))
		cmd = PrivNetParam::Set;
	else if ((!strcmp(argv[i], "add")) ||
		(!strcmp(argv[i], "new")))
		cmd = PrivNetParam::Add;
	else if (!strcmp(argv[i], "del"))
		cmd = PrivNetParam::Del;
	else
		return invalid_action;

	return get_privnet_param(argc, argv, cmd, ++i);
}

CmdParamData cmdParam::parse_ct_template_args(int argc, char **argv, int i)
{
	if (argc <= i) {
		usage_disp(argv[0]);
		return invalid_action;
	}

	CmdParamData param;
	param.action = SrvCtTemplateAction;
	if (!strcmp(argv[i], "list")) {
		param.ct_tmpl.cmd = CtTemplateParam::List;
		if (argc > i + 1 && (!strcmp(argv[i + 1], "-j") ||
							!strcmp(argv[i + 1], "--json")))
			param.use_json = true;
	} else if (!strcmp(argv[i], "remove")) {
		i++;
		if (argc <= i) {
			usage_disp(argv[0]);
			return invalid_action;
		}

		param.ct_tmpl.cmd = CtTemplateParam::Remove;
		param.ct_tmpl.name = argv[i];
		if (argc > i + 1)
			param.ct_tmpl.os_name = argv[i + 1];
	} else if (!strcmp(argv[i], "copy")) {
		std::string val;

		if (argc <= i + 2) {
			usage_disp(argv[0]);
			return invalid_action;
		}

		param.action = SrvCopyCtTemplateAction;
		i++;
		if (parse_auth(argv[i], param.copy_ct_tmpl.dst, argv[i])) {
			fprintf(stderr, "An incorrect value is"
					" specified for dst: %s\n",
					val.c_str());
			return invalid_action;
		}
		param.ct_tmpl.name = argv[++i];
		if (argc > i + 1) {
			if (argv[i + 1][0] != '-') {
				param.ct_tmpl.os_name = argv[++i];
			}
		}

		GetOptLong opt(argc, argv, ct_template_copy_options, ++i);
		while (1) {
			int id = opt.parse(val);
			if (id == -1) // the end mark
				break;
			switch (id) {
			CASE_PARSE_OPTION_GLOBAL(val, param)
			case CMD_FORCE:
				param.copy_ct_tmpl.force = true;
				break;
			case CMD_SECURITY_LEVEL:
				if (get_security_level(val, &param.copy_ct_tmpl.security_level)) {
					 fprintf(stderr, "An incorrect security level is"
						" specified: %s\n",
						val.c_str());
					return invalid_action;
				}
				break;
			case GETOPTUNKNOWN:
				fprintf(stderr, "Unrecognized option: %s\n",
						opt.get_next());
				return invalid_action;
			case GETOPTERROR:
			default:
				return invalid_action;
			}
		}
	} else {
		fprintf(stderr, "Unknown cttemplate command %s\n", argv[i]);
		return invalid_action;
	}

	return param;
}

CmdParamData cmdParam::parse_monitor_args(int argc, char **argv)
{
	if (argc != 2) {
		usage_disp(argv[0]);
		return invalid_action;
	}

	CmdParamData param;
	param.action = SrvMonitorAction;

	return param;
}

CmdParamData cmdParam::parse_backup_node_args(int argc, char **argv,
		const Option *options, int offset)
{
	CmdParamData param = parse_backup_args(argc, argv, SrvBackupNodeAction, options, offset);
	if (param.action == InvalidAction)
		return param;

	if ((param.backup.flags & (PBT_INCREMENTAL | PBT_FULL)) == 0)
		param.backup.flags |= PBT_INCREMENTAL;

	return param;
}

CmdParamData cmdParam::parse_backup_args(int argc, char **argv, Action action,
		const Option *options, int offset)
{
	std::string val;
	CmdParamData param;
	param.action = action;

	GetOptLong opt(argc, argv, options, offset);
	while (1) {
		int id = opt.parse(val);
		if (id == -1) // the end mark
			break;
		switch (id) {
		CASE_PARSE_OPTION_GLOBAL(val, param)
		case CMD_BACKUP_FULL:
			param.backup.flags |= PBT_FULL;
			break;
		case CMD_BACKUP_INC:
			param.backup.flags |= PBT_INCREMENTAL;
			break;
		case CMD_BACKUP_DIFF:
			param.backup.flags |= PBT_DIFFERENTIAL;
			break;
		case CMD_SECURITY_LEVEL:
			if (get_security_level(val, &param.security_level)) {
				 fprintf(stderr, "An incorrect security level is"
					" specified: %s\n",
					val.c_str());
				return invalid_action;
			}
			break;
		case CMD_BACKUP_ID:
			normalize_uuid(val, param.backup.id);
			break;
		case CMD_BACKUP_STORAGE:
			if (parse_auth(val, param.backup.storage)) {
				fprintf(stderr, "An incorrect value is"
						" specified for backup storage: %s\n",
						val.c_str());
				return invalid_action;
			}
			opt.hide_arg();
			break;
		case CMD_DESC:
			param.desc = val;
			break;
		case CMD_UNCOMPRESSED:
			param.backup.flags |= PBT_UNCOMPRESSED;
			break;
		case GETOPTUNKNOWN:
			fprintf(stderr, "Unrecognized option: %s\n",
				opt.get_next());
			return invalid_action;
		case GETOPTERROR:
		default:
			return invalid_action;
		}
	}

	return param;
}

CmdParamData cmdParam::get_disp(int argc, char **argv)
{
	if (argc < 2) {
		usage_disp(argv[0]);
		return invalid_action;
	}

	int i = 1;
	if (!strcmp(argv[i], "shutdown") ||
	    !strcmp(argv[i], "stop"))
		return get_disp_param(argc, argv, SrvShutdownAction,
				disp_shutdown_options, ++i);
	else if (!strcmp(argv[i], "info"))
		return get_disp_param(argc, argv, SrvHwInfoAction,
				disp_info_options, ++i);
	else if (!strcmp(argv[i], "user")) {
		if (argc - 1 > i) {
			i++;
			if (!strcmp(argv[i], "list"))
				return get_param(argc, argv, SrvUsrListAction,
					user_list_options, i);
			else if (!strcmp(argv[i], "set"))
				 return get_param(argc, argv, SrvUsrSetAction,
					user_set_options, i);
		}
	} else if (!strcmp(argv[i], "set"))
		return get_disp_param(argc, argv, DispSetAction,
				disp_set_options, ++i);
	else if (!strcmp(argv[i], "install-license"))
		return get_disp_param(argc, argv, SrvInstallLicenseAction,
				lic_options, ++i);
	else if (!strcmp(argv[i], "update-license"))
		return get_disp_param(argc, argv, SrvUpdateLicenseAction,
				no_options, ++i);
	else if (!strcmp(argv[i], "statistics"))
		return get_statistics_param(argc, argv, SrvPerfStatsAction,
                                    statistics_options, ++i);
	else if (!strcmp(argv[i], "problem-report"))
		return get_problem_report_param(argc, argv, SrvProblemReportAction,
				problem_report_options, ++i);
	else if (!strcmp(argv[i], "net"))
		return parse_vnet_args(argc, argv, i);
	else if (!strcmp(argv[i], "tc"))
		return parse_tc_args(argc, argv, i);
	else if (!strcmp(argv[i], "privnet"))
		return parse_privnet_args(argc, argv, i);
	else if (!strcmp(argv[i], "usb"))
		return parse_usb_args(argc, argv, i);
	else if (!strcmp(argv[i], "appliance-install"))
		return get_disp_param(argc, argv, SrvInstApplianceAction,
				appliance_options, ++i);
	else if (!strcmp(argv[i], "update-host-reg-info"))
		return get_disp_param(argc, argv, SrvUpdateHostRegInfoAction,
				no_options, ++i);
	else if (!strcmp(argv[i], "prepare-for-uninstall"))
		return get_disp_param(argc, argv, SrvPrepareForUninstallAction,
				no_options, ++i);
	else if (!strcmp(argv[i], "list-network-config"))
		return get_disp_param(argc, argv, DispListAction,
				no_options, ++i);
	else if (!strcmp(argv[i], "up-listen-interface"))
		return get_disp_param(argc, argv, DispUpListeningInterfaceAction,
				disp_up_listen_iface_options, ++i);
	else if (!strcmp(argv[i], "start-nat-detect"))
		return get_disp_param(argc, argv, DispStartNatDetectAction,
				no_options, ++i);
	else if (!strcmp(argv[i], "cttemplate"))
		return parse_ct_template_args(argc, argv, ++i);
	else if (!strcmp(argv[i], "monitor"))
		return parse_monitor_args(argc, argv);
	else if (!strcmp(argv[i], "backup"))
		return parse_backup_node_args(argc, argv, backup_options, 2);
	else if (!strcmp(argv[i], "help") ||
			!strcmp(argv[i], "--help"))
	{
		usage_disp(argv[0]);
		exit(0);
	}

	fprintf(stderr, "Unknown action: %s\n", argv[i]);
	return invalid_action;
}

int cmdParam::parse_memguarantee(const char *value, CmdParamData &param)
{
	if (!value)
		return 1;

	param.memguarantee_set = true;

	if (!strcmp(value, "auto")) {
		param.memguarantee.type = PRL_MEMGUARANTEE_AUTO;
		return 0;
	}

	if (parse_ui(value, (unsigned int *)&param.memguarantee.value))
		return 1;

	// only value in percents is possible
	param.memguarantee.type = PRL_MEMGUARANTEE_PERCENTS;

	return 0;
}
