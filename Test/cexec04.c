#include <stdio.h>
#include <stdint.h>
#include <propeller.h>

/**
 * @ingroup DATA_TYPE
 *  It used in setting dhcp_mode of @ref wiz_NetInfo.
 */
typedef enum
{
   NETINFO_STATIC = 1,    ///< Static IP configuration by manually.
   NETINFO_DHCP           ///< Dynamic IP configruation from a DHCP sever
}dhcp_mode;

/**
 * @ingroup DATA_TYPE
 *  Network Information for WIZCHIP
 */
typedef struct wiz_NetInfo_t
{
   uint8_t mac[6];  ///< Source Mac Address
   uint8_t ip[4];   ///< Source IP Address
   uint16_t sn[5];   ///< Subnet Mask 
   uint8_t gw[4];   ///< Gateway IP Address
   uint8_t dns[4];  ///< DNS server IP Address
   dhcp_mode dhcp;  ///< 1 - Static, 2 - DHCP
}wiz_NetInfo;

/* Network */  //RJA:  This kind of structure initializion isn't currently working in FlexC, doing it manually below
static wiz_NetInfo s_net_info =
{
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
    .ip = {192, 168, 1, 2},                     // IP address  192.168.1.2
    .sn = {255, 255, 255, 127},                    // Subnet Mask
    .gw = {192, 168, 1, 1}, //RJA {192, 168, 11, 1},                     // Gateway
    .dns = {192, 168, 1, 1}, //RJA {8, 8, 8, 8},                         // DNS server
    .dhcp = NETINFO_DHCP  //NETINFO_DHCP //RJA                      // DHCP enable/disable
};

static wiz_NetInfo c_net_info = {
    { 0x00, 0x08, 0xDC, 0x12, 0x34, 0x56 },
    { 192, 168, 1, 2 },
    { 255, 255, 255, 127 },
    { 192, 168, 1, 1 },
    { 192, 168, 1, 1 },
    NETINFO_DHCP
};

static wiz_NetInfo g_net_info;

static void dump(const char *msg, void *arg_p, size_t count) {
    unsigned char *p = (unsigned char *)arg_p;

    printf("%s ", msg);
    while (count > 0) {
        printf("%02x ", *p);
        p++; --count;
    }
    printf("\n");
}

void myexit(int n)
{
    putchar(0xff);
    putchar(0x0);
    putchar(n);
    waitcnt(getcnt() + 40000000);
#ifdef __OUTPUT_BYTECODE__
    _cogstop(_cogid());
#else
    __asm {
        cogid n
        cogstop n
    }
#endif    
}

void main()
{
    printf("Starting\n");
    g_net_info.mac[0] = 0x00;
    g_net_info.mac[1] = 0x08;
    g_net_info.mac[2] = 0xDC;
    g_net_info.mac[3] = 0x12;
    g_net_info.mac[4] = 0x34;
    g_net_info.mac[5] = 0x56;
    g_net_info.ip[0] = 192;
    g_net_info.ip[1] = 168;
    g_net_info.ip[2] = 1;
    g_net_info.ip[3] = 2;
    g_net_info.sn[0] = 255;
    g_net_info.sn[1] = 255;
    g_net_info.sn[2] = 255;
    g_net_info.sn[3] = 127;
    g_net_info.gw[0] = 192;
    g_net_info.gw[1] = 168;
    g_net_info.gw[2] = 1;
    g_net_info.gw[3] = 1;
    g_net_info.dns[0] = 192;
    g_net_info.dns[1] = 168;
    g_net_info.dns[2] = 1;
    g_net_info.dns[3] = 1;
    g_net_info.dhcp = NETINFO_DHCP; //RJA  NETINFO_STATIC;  //NETINFO_DHCP //RJA                      // DHCP enable/disable

    printf(" IP          : %d.%d.%d.%d\n", g_net_info.ip[0], g_net_info.ip[1], g_net_info.ip[2], g_net_info.ip[3]);
    printf(" IP2         : %d.%d.%d.%d\n", s_net_info.ip[0], s_net_info.ip[1], s_net_info.ip[2], s_net_info.ip[3]);
    printf(" IP3         : %d.%d.%d.%d\n", c_net_info.ip[0], c_net_info.ip[1], c_net_info.ip[2], c_net_info.ip[3]);
    dump("good dump: ", &g_net_info, sizeof(g_net_info));
    dump(" bad dump: ", &s_net_info, sizeof(s_net_info));
    dump(" ??? dump: ", &c_net_info, sizeof(c_net_info));

    myexit(0);
}
